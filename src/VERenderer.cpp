#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

    auto Renderer::GetVulkanState2(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<VulkanState>> {
        return *registry.template GetView<vecs::Handle, VulkanState&>().begin();
    }

    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_windowName(windowName) {
	};

    Renderer::~Renderer(){};

	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { 
		GetVulkanState()().m_commandBuffersSubmit.push_back(commandBuffer); 
	};

	auto Renderer::GetVulkanState() -> vecs::Ref<VulkanState> { 
		if(!m_vulkanStateHandle.IsValid()) {
			auto [handle, state] = GetVulkanState2(m_registry);
			m_vulkanStateHandle = handle;
			return state;
		}
		return m_registry.template Get<VulkanState&>(m_vulkanStateHandle);
	}

};  // namespace vve

