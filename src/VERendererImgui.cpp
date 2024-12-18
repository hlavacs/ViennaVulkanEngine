
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VEInclude.h"
#include "VESystem.h"
#include "VEInclude.h"
#include "VHInclude.h"
#include "VEEngine.h"
#include "VERendererImgui.h"
#include "VEWindowSDL.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui( std::string systemName, Engine<ATYPE>* engine, Window<ATYPE>* window ) 
        : Renderer<ATYPE>(systemName, engine, window ) {

		engine->RegisterCallback( { 
			{this,   3000, "INIT", [this](Message message){this->OnInit(message);} },
			{this,      0, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message message){this->OnSDL(message);} },
			{this,  -2000, "QUIT", [this](Message message){this->OnQuit(message);} }
		} );

    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui() {};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit(Message message) {
		if(m_vulkan == nullptr) { m_vulkan = (RendererVulkan<ATYPE>*)m_engine->GetSystem("VVE RendererVulkan"); }

		//auto rend = ((RendererVulkan<ATYPE>*)(m_engine->GetSystem("VVE RendererVulkan")));
		WindowSDL<ATYPE>* windowSDL = (WindowSDL<ATYPE>*)m_window;

		vh::setupImgui(windowSDL->GetSDLWindow(), m_vulkan->GetInstance(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetQueueFamilies(), m_vulkan->GetDevice(), m_vulkan->GetGraphicsQueue(), 
			m_vulkan->GetCommandPool(), m_vulkan->GetDescriptorPool(), m_vulkan->GetRenderPass());  

        vh::createCommandPool(m_window->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool); 
	}

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnRecordNextFrame(Message message) {

        if(!m_window->GetIsMinimized()) {
		    ImGui_ImplVulkan_NewFrame();
			ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
		}
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnSDL(Message message) {
    	SDL_Event event = message.GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
    }

    
   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    template class RendererImgui<ENGINETYPE_SEQUENTIAL>;
    template class RendererImgui<ENGINETYPE_PARALLEL>;

};   // namespace vve

