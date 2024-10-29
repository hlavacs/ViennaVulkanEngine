#pragma once

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using Renderer<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererImgui" );
        virtual ~RendererVulkan();

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	
    };

};   // namespace vve

