#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	/**
	 * @brief Constructs a forward renderer system
	 * @param systemName Name of the renderer system
	 * @param engine Reference to the engine instance
	 * @param windowName Name of the window to render to
	 */
	RendererForward::RendererForward( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,  3000, "INIT", [this](Message& message){ return OnInit(message);} }
	  } );
	};

	/**
	 * @brief Destructor for the forward renderer
	 */
	RendererForward::~RendererForward(){};

	/**
	 * @brief Initializes the forward renderer based on the Vulkan API version
	 * @param message Initialization message containing engine state
	 * @return false to continue message processing
	 */
	bool RendererForward::OnInit(Message message) {
		auto state = m_engine.GetState();

		switch( VK_VERSION_MINOR(state.m_apiVersion) ) {
			case 0:
				std::cout << "Minimum Vulkan API version is 1.1!\n";
				exit(1);
				break;
			default:
			m_engine.RegisterSystem(std::make_unique<RendererShadow11>(m_name() + "Shadow1.1", m_engine, m_windowName));
			m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "Light1.1", m_engine, m_windowName));
		};
		return false;
	}

};   // namespace vve