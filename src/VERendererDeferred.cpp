#include "VHInclude2.h"
#include "VEInclude.h"


namespace vve {

	RendererDeferred::RendererDeferred(std::string systemName, Engine& engine, std::string windowName) :
		System{ systemName, engine }, m_windowName(windowName) {

		engine.RegisterCallbacks({
			{this, 3000, "INIT", [this](Message& message) { return OnInit(message); } }
			});
	}

	RendererDeferred::~RendererDeferred() {};

	bool RendererDeferred::OnInit(Message message) {
		auto state = m_engine.GetState();

		switch (VK_API_VERSION_MINOR(state.apiVersion)) {
			case 0:
				std::cout << "Minimum Vulkan API version is 1.1!\n";
				exit(1);
				break;
			case 4:
				//fall through
			case 3:
				std::cout << "Initializing Vulkan 1.3 Deferred Renderer\n";
				m_engine.RegisterSystem(std::make_unique<RendererDeferred13>(m_name() + "Light1.3", m_engine, m_windowName));
				break;
			default:
				// 1.1 is default and minimum
				std::cout << "Initializing Vulkan 1.1 Deferred Renderer\n";
				m_engine.RegisterSystem(std::make_unique<RendererDeferred11>(m_name() + "Light1.1", m_engine, m_windowName));
				break;
		}
		return false;
	}

}	 // namespace vve
