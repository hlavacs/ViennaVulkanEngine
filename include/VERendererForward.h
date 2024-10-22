#pragma once

#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE>
    {
    public:
        RendererForward(std::string name, Engine<ATYPE>& engine, Window<ATYPE>* window, int phase=100);
        virtual ~RendererForward();

    private:
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRecordNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
    };

};   // namespace vve

