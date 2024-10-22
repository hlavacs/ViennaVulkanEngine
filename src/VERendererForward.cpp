
#include "VESystem.h"
#include "VEEngine.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward(std::string name, Engine<ATYPE>& engine, Window<ATYPE>* window, int phase) 
        : Renderer<ATYPE>(name, engine, window) {

        engine.RegisterSystem( this, phase, {MessageType::PREPARE_NEXT_FRAME, MessageType::RECORD_NEXT_FRAME, MessageType::RENDER_NEXT_FRAME} );
    };

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnPrepareNextFrame(Message message) {

    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRecordNextFrame(Message message) {
        
    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRenderNextFrame(Message message) {

    }


    template class RendererForward<ArchitectureType::SEQUENTIAL>;
    template class RendererForward<ArchitectureType::PARALLEL>;

};   // namespace vve


