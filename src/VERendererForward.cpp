#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	RendererForward::RendererForward( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			{this,  3000, "INIT", [this](Message& message){ return OnInit(message);} }
	  } );
	};

	RendererForward::~RendererForward(){};

	bool RendererForward::OnInit(Message message) {
		auto state = m_engine.GetState();

		switch( VK_VERSION_MINOR(state.apiVersion) ) {
			case 0:
				std::cout << "Minimum Vulkan API version is 1.1!\n";
				exit(1);
				break;
			case 1:
				m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "1.1", m_engine, m_windowName));
				break;
			case 2:
				m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "1.1", m_engine, m_windowName));
				break;
			case 3:
				m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "1.1", m_engine, m_windowName));
				break;
			case 4:
				m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "1.1", m_engine, m_windowName));
				break;
			default:
				m_engine.RegisterSystem(std::make_unique<RendererForward11>(m_name() + "1.1", m_engine, m_windowName));
		};
		return false;
	}


};   // namespace vve