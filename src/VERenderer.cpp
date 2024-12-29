#include <any>
#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {
    
   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::Renderer(std::string systemName, Engine<ATYPE>& engine ) : System<ATYPE>{systemName, engine } {
		engine.RegisterCallback( { 
			{this,      0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} }
		} );
	};

   	template<ArchitectureType ATYPE>
    Renderer<ATYPE>::~Renderer(){};

   	template<ArchitectureType ATYPE>
    void Renderer<ATYPE>::OnAnnounce( Message message ) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( ((System<ATYPE>*)msg.m_sender)->GetName() == "VVE Window" ) {
			m_window = (WindowSDL<ATYPE>*)msg.m_sender;
		}
	};

    template class Renderer<ENGINETYPE_SEQUENTIAL>;
    template class Renderer<ENGINETYPE_PARALLEL>;

};  // namespace vve

