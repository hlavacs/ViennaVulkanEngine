#pragma once

#include "VERendererVulkan.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererImgui" );
        virtual ~RendererImgui();

    private:
        virtual void OnInit(Message message);
        virtual void OnInit2(Message message);
        virtual void OnPollEvents(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnQuit(Message message);
    };

};   // namespace vve
