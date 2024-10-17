#pragma once

#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE>
    {
    public:
        RendererForward(Engine<ATYPE>& engine, std::weak_ptr<Window<ATYPE>> window);
        virtual ~RendererForward();

    private:
        virtual void PrepareRender() override;
        virtual void Render() override;
    };

};   // namespace vve

