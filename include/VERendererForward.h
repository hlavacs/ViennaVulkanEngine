#pragma once

#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE>
    {
    public:
        RendererForward(Engine<ATYPE>& engine);
        virtual ~RendererForward();

    private:
        virtual void Render() override;
    };

};   // namespace vve

