#pragma once

#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE>
    {
    public:
        RendererForward(std::string systemName, Engine<ATYPE>* engine, Window<ATYPE>* window );
        virtual ~RendererForward();

    private:
        virtual void OnInit(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnQuit(Message message);
    };

};   // namespace vve

