#pragma once

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
    public:
        RendererImgui(Engine<ATYPE>& engine, std::weak_ptr<Window<ATYPE>> window);
        virtual ~RendererImgui();

    private:
        virtual void PrepareRender() override;
        virtual void Render() override;

    };

};   // namespace vve
