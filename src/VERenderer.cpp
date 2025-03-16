#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_windowName(windowName) {	};

    Renderer::~Renderer(){};

	auto Renderer::GetState(vecs::Registry& registry) -> std::tuple<vecs::Handle, vecs::Ref<VulkanState>> {
        return *registry.template GetView<vecs::Handle, VulkanState&>().begin();
    }

	bool Renderer::OnInit(Message message) {
		auto [handle, stateW, stateSDL] = WindowSDL::GetState(m_registry);
		m_windowState = stateW;
		m_windowSDLState = stateSDL;

		auto view = m_registry.template GetView<vecs::Handle, VulkanState&>();
		auto iterBegin = view.begin();
		auto iterEnd = view.end();
		if( !(iterBegin != iterEnd)) {
			m_vulkanStateHandle = m_registry.Insert(VulkanState{});
			m_vulkanState = m_registry.template Get<VulkanState&>(m_vulkanStateHandle);
			return false;
		}
		auto [handleV, stateV] = *iterBegin;
		m_vulkanStateHandle = handleV;
		m_vulkanState = stateV;
		return false;
	}

	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { 
		m_vulkanState().m_commandBuffersSubmit.push_back(commandBuffer); 
	};

};  // namespace vve

