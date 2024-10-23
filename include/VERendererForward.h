#pragma once

#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE>
    {
    public:
        RendererForward(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererForward" );
        virtual ~RendererForward();

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRecordNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;
    };

};   // namespace vve

