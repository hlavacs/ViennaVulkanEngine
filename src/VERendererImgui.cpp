
#include "VEInclude.h"
#include "VESystem.h"
#include "VEInclude.h"
#include "VHInclude.h"
#include "VEEngine.h"
#include "VERendererImgui.h"
#include "VEWindowSDL.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui( Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name) 
        : Renderer<ATYPE>(engine, window, name ) {

		engine->RegisterSystem( { 
			{this,  10000, MessageType::INIT, [this](Message message){this->OnInit(message);} },
			{this, -10000, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} },
			{this, -10000, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} },
			{this, -10000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} },
			{this,      0, MessageType::SDL, [this](Message message){this->OnSDL(message);} },
			{this,  10000, MessageType::INIT, [this](Message message){this->OnInit2(message);} },
			{this,  10000, MessageType::POLL_EVENTS, [this](Message message){this->OnPollEvents(message);} }
		} );

    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui() {};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit2(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnPrepareNextFrame(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnRenderNextFrame(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnPollEvents(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnSDL(Message message) {
    	SDL_Event event = message.GetData<MessageSDL>().m_event;
    	//ImGui_ImplSDL2_ProcessEvent(&event);
    }

    
   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnQuit(Message message) {
		//ImGui_ImplVulkan_Shutdown();
        //ImGui_ImplSDL2_Shutdown();
        //ImGui::DestroyContext();
    }

    template class RendererImgui<ArchitectureType::SEQUENTIAL>;
    template class RendererImgui<ArchitectureType::PARALLEL>;

};   // namespace vve

