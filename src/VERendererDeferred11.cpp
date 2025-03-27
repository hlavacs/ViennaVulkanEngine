#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallback({
			{this, 3500, "INIT", [this](Message& message) { return OnInit(message); } },
			{this,	  0, "QUIT", [this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred11::~RendererDeferred11() {};

	bool RendererDeferred11::OnInit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?
		Renderer::OnInit(message);

		vh::RenCreateRenderPass(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain, false, m_renderPass);

		// TODO: binding 0 might only need vertex globally
		vh::RenCreateDescriptorSetLayout(
			m_vulkanState().m_device,
			{
				{	// Binding 0 : Vertex and Fragment uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : Position
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 2 : Normals
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 3 : Albedo (Diffuse)
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 4 : Light uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame);

		vh::ComCreateCommandPool(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vulkanState().m_device, m_commandPool, m_commandBuffers);
		// TODO: shrink pool to only what is needed - why 1000?
		vh::RenCreateDescriptorPool(m_vulkanState().m_device, 1000, m_descriptorPool);
		vh::RenCreateDescriptorSet(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);

		// Binding 0 : Vertex and Fragment uniform buffer
		vh::BufCreateUniformBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSetUBO(m_vulkanState().m_device, m_uniformBuffersPerFrame, 0, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);

		// Binding 1 : Position
		vh::ImgCreateTextureImage(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool,
			m_texturePosition.m_pixels, m_texturePosition.m_width, m_texturePosition.m_height, m_texturePosition.m_size, m_texturePosition);
		vh::RenUpdateDescriptorSetTexture(m_vulkanState().m_device, m_texturePosition, 1, m_descriptorSetPerFrame);

		// Binding 2 : Normals
		vh::ImgCreateTextureImage(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool,
			m_textureNormals.m_pixels, m_textureNormals.m_width, m_textureNormals.m_height, m_textureNormals.m_size, m_textureNormals);
		vh::RenUpdateDescriptorSetTexture(m_vulkanState().m_device, m_textureNormals, 2, m_descriptorSetPerFrame);

		// Binding 3 : Albedo
		vh::ImgCreateTextureImage(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool,
			m_textureAlbedo.m_pixels, m_textureAlbedo.m_width, m_textureAlbedo.m_height, m_textureAlbedo.m_size, m_textureAlbedo);
		vh::RenUpdateDescriptorSetTexture(m_vulkanState().m_device, m_textureAlbedo, 3, m_descriptorSetPerFrame);

		// Binding 4 : Light
		vh::BufCreateUniformBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_maxNumberLights * sizeof(vh::Light), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSetUBO(m_vulkanState().m_device, m_uniformBuffersLights, 4, m_maxNumberLights * sizeof(vh::Light), m_descriptorSetPerFrame);
		return false;
	}

	bool RendererDeferred11::OnQuit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?

		return false;
	}

}	// namespace vve
