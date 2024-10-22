
#include "VESystem.h"
#include "VEEngine.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward(std::string name, Engine<ATYPE>& engine, Window<ATYPE>* window) 
        : Renderer<ATYPE>(name, engine, window) {

        engine.RegisterSystem( this,  -100
            , {MessageType::INIT, MessageType::PREPARE_NEXT_FRAME, MessageType::RECORD_NEXT_FRAME, MessageType::RENDER_NEXT_FRAME, MessageType::QUIT} );
    };

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};


   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnInit(Message message) {

    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnPrepareNextFrame(Message message) {

    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRecordNextFrame(Message message) {
        
    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRenderNextFrame(Message message) {

    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnQuit(Message message) {

    }

    template class RendererForward<ArchitectureType::SEQUENTIAL>;
    template class RendererForward<ArchitectureType::PARALLEL>;

};   // namespace vve


