#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	RendererDeferred::RendererDeferred(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		// TODO engine.RegisterCallback -> register Callbacks
		engine.RegisterCallback({
			{this, 3000, "INIT", [this](Message& message) { return OnInit(message); } }
			});
	}

	RendererDeferred::~RendererDeferred() {};

	bool RendererDeferred::OnInit(Message message) {
		std::cout << "RendererDeferred: OnInit callback\n";
		auto state = m_engine.GetState();

		switch (VK_API_VERSION_MINOR(state.apiVersion)) {
			case 0:
				std::cout << "Minimum Vulkan API version is 1.1!\n";
				exit(1);
				break;
			default:
				// TODO 1.1 Renderer & 1.3 Renderer
				m_engine.RegisterSystem(std::make_unique<RendererDeferred11>(m_name() + "DefLight1.1", m_engine, m_windowName));
				break;
		}
		return false;
	}

}	 // namespace vve
