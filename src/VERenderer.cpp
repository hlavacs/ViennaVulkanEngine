#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {
    
    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_windowName(windowName) {
		engine.RegisterCallback( { 
			{this,      0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} }
		} );
	};

    Renderer::~Renderer(){};

    void Renderer::OnAnnounce( Message message ) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == m_windowName ) {
			m_window = dynamic_cast<Window*>(msg.m_sender);
		}
	};


};  // namespace vve

