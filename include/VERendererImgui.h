#pragma once

#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
    public:
        RendererImgui(Engine<ATYPE>& engine, std::shared_ptr<Window<ATYPE>> window);
        virtual ~RendererImgui();

    private:
        virtual void Render() override;

    };

};   // namespace vve
