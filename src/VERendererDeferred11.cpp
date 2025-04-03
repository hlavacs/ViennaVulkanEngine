#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this, 3500, "INIT", [this](Message& message) { return OnInit(message); } },
			{this,	  0, "QUIT", [this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred11::~RendererDeferred11() {};

	bool RendererDeferred11::OnInit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?
		Renderer::OnInit(message);

		vh::RenCreateRenderPassGeometry(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_swapChain, true, m_geometryPass);
		vh::RenCreateRenderPass(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_swapChain, true, m_lightingPass);

		// TODO: binding 0 might only need vertex globally
		vh::RenCreateDescriptorSetLayout(
			m_vkState().m_device,
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
				{	// Binding 3 : Albedo
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 4 : Light uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame);

		vh::ComCreateCommandPool(m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vkState().m_device, m_commandPool, m_commandBuffers);
		// TODO: shrink pool to only what is needed - why 1000?
		vh::RenCreateDescriptorPool(m_vkState().m_device, 1000, m_descriptorPool);
		vh::RenCreateDescriptorSet(m_vkState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);

		// Binding 0 : Vertex and Fragment uniform buffer
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersPerFrame, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);

		vh::ImgCreateImageSampler(m_vkState().m_physicalDevice, m_vkState().m_device, m_sampler);
		// Binding 1 : Position
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_positionImage, m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_positionImage, 1, m_descriptorSetPerFrame);

		// Binding 2 : Normals
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_normalsImage, m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_normalsImage, 2, m_descriptorSetPerFrame);

		// Binding 3 : Albedo
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_albedoImage, VK_FORMAT_R8G8B8A8_UNORM, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_albedoImage, 3, m_descriptorSetPerFrame);

		// Binding 4 : Light
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_maxNumberLights * sizeof(vh::Light), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersLights, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxNumberLights * sizeof(vh::Light), m_descriptorSetPerFrame);

		CreatePipelines();
		return false;
	}

	bool RendererDeferred11::OnQuit(Message message) {
		vkDeviceWaitIdle(m_vkState().m_device);

		vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);

		// TODO: Manage pipelines

		vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_geometryPass, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_lightingPass, nullptr);
		vkDestroySampler(m_vkState().m_device, m_sampler, nullptr);

		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_positionImage.m_gbufferImage, m_positionImage.m_gbufferImageAllocation);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_normalsImage.m_gbufferImage, m_normalsImage.m_gbufferImageAllocation);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_albedoImage.m_gbufferImage, m_albedoImage.m_gbufferImageAllocation);
		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersLights);

		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
	}

	void RendererDeferred11::CreatePipelines() {

	}

}	// namespace vve
