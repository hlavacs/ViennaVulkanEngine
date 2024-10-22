#pragma once

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using Renderer<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(std::string name, Engine<ATYPE>& engine, Window<ATYPE>* window);
        virtual ~RendererImgui();

    private:
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRecordNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        ImGuiIO* m_io;
   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;	
    };

};   // namespace vve
