

#include "VERendererVulkan.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"


namespace vve {

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : Renderer<ATYPE>(engine, window, name) {

        engine->RegisterSystem( { 
			  {this, -1000, MessageType::INIT, [this](Message message){this->OnInit(message);} }
			, {this, -1000, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} }
			, {this, -1000, MessageType::RECORD_NEXT_FRAME, [this](Message message){this->OnRecordNextFrame(message);} }
			, {this, -1000, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} }
			, {this,    50, MessageType::INIT, [this](Message message){this->OnInit2(message);} }
			, {this,  1000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
		} );
    }

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::~RendererVulkan() {}


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit(Message message) {
      m_windowSDL = (WindowSDL<ATYPE>*)m_window;
      vh::createInstance( m_validationLayers, m_windowSDL->GetInstanceExtensions(), m_instance);
    }


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit2(Message message) {
    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRecordNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {

    }
    
    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit(Message message) { 
    }

    template class RendererVulkan<ArchitectureType::SEQUENTIAL>;
    template class RendererVulkan<ArchitectureType::PARALLEL>;

};   // namespace vve