#pragma once

#include "VERenderer.h"

namespace vve
{

    class RendererForward : public Renderer
    {
    public:
        RendererForward(Engine& engine);
        virtual ~RendererForward();

    private:
        virtual void Render() override;
    };

};   // namespace vve

