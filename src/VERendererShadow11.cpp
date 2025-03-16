#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			{this,  3000, "INIT", [this](Message& message){ return OnInit(message);} }
	  } );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		auto state = m_engine.GetState();

		return false;
	}


};   // namespace vve