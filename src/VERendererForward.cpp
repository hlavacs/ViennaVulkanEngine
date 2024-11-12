
#include "VHInclude.h"
#include "VHVulkan.h"

#include "VESystem.h"
#include "VEEngine.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : Renderer<ATYPE>(engine, window, name) {

  		engine->RegisterSystem( { 
  			{this, -5000, MessageType::INIT, [this](Message message){this->OnInit(message);} },
  			{this, -5000, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} },
  			{this, -5000, MessageType::RECORD_NEXT_FRAME, [this](Message message){this->OnRecordNextFrame(message);} },
  			{this, -5000, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} },
  			{this, -5000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
  		} );
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


