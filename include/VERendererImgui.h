#pragma once

#include "VERendererVulkan.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(std::string systemName, Engine<ATYPE>* engine, Window<ATYPE>* window );
        virtual ~RendererImgui();

    private:
        virtual void OnInit(Message message);
        virtual void OnInit2(Message message);
        virtual void OnPollEvents(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnSDL(Message message);
        virtual void OnQuit(Message message);

		RendererVulkan<ATYPE>* m_vulkan{nullptr};
	    std::vector<VkCommandBuffer> m_commandBuffers;

    };

};   // namespace vve
