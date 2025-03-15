#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

    auto Renderer::GetState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<VulkanState>> {
        return *registry.template GetView<vecs::Handle, VulkanState&>().begin();
    }

	auto Renderer::GetState2() -> vecs::Ref<VulkanState> { 
		if(!m_vulkanStateHandle.IsValid() || !m_vulkanState.IsValid()) {
			auto [handle, state] = GetState(m_registry);
			m_vulkanStateHandle = handle;
			m_vulkanState = state;
			return state;
		}
		return m_vulkanState;
	}

    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_windowName(windowName) {	};

    Renderer::~Renderer(){};

	bool Renderer::OnInit(Message message) {
		auto state = WindowSDL::GetState(m_registry);
		m_windowState = std::get<1>(state);
		m_windowSDLState = std::get<2>(state);
		return false;
	}

	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { 
		GetState2()().m_commandBuffersSubmit.push_back(commandBuffer); 
	};

};  // namespace vve

