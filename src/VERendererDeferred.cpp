#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	/**
	 * @brief Constructs a deferred renderer system
	 * @param systemName Name of the renderer system
	 * @param engine Reference to the engine instance
	 * @param windowName Name of the window to render to
	 */
	RendererDeferred::RendererDeferred(const std::string& systemName, Engine& engine, const std::string& windowName) :
		System{ systemName, engine }, m_windowName(windowName) {

		engine.RegisterCallbacks({
			{this, 3000, "INIT", [this](Message& message) { return OnInit(message); } }
			});
	}

	/**
	 * @brief Destructor for the deferred renderer
	 */
	RendererDeferred::~RendererDeferred() {};

	/**
	 * @brief Initializes the deferred renderer based on the Vulkan API version
	 * @param message Initialization message containing engine state
	 * @return false to continue message processing
	 */
	bool RendererDeferred::OnInit(const Message& message) {
		auto state = m_engine.GetState();

		switch (VK_API_VERSION_MINOR(state.m_apiVersion)) {
			case 0:
				std::cout << "Minimum Vulkan API version is 1.1!\n";
				exit(1);
				break;
			case 4:
				//fall through
			case 3:
				std::cout << "Initializing Vulkan 1.3 Deferred Renderer\n";
				m_engine.RegisterSystem(std::make_unique<RendererShadow11>(m_name() + "Shadow1.1", m_engine, m_windowName));
				m_engine.RegisterSystem(std::make_unique<RendererGaussian>(m_name() + "Gaussian", m_engine, m_windowName));
				m_engine.RegisterSystem(std::make_unique<RendererDeferred13>(m_name() + "Light1.3", m_engine, m_windowName));
				break;
			default:
				// 1.1 is default and minimum
				std::cout << "Initializing Vulkan 1.1 Deferred Renderer\n";
				m_engine.RegisterSystem(std::make_unique<RendererShadow11>(m_name() + "Shadow1.1", m_engine, m_windowName));
				m_engine.RegisterSystem(std::make_unique<RendererGaussian>(m_name() + "Gaussian", m_engine, m_windowName));
				m_engine.RegisterSystem(std::make_unique<RendererDeferred11>(m_name() + "Light1.1", m_engine, m_windowName));
				break;
		}
		return false;
	}

}	 // namespace vve
