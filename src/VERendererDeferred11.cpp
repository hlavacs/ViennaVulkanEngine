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

		// TODO: maybe split uniform buffer into separate vertex and fragment buffer
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
			},
			m_descriptorSetLayoutPerFrame);

		return false;
	}

	bool RendererDeferred11::OnQuit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?

		return false;
	}

}	// namespace vve
