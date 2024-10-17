#pragma once

#include "VEInclude.h"
#include "VEWindow.h"
#include "VERenderer.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
    public:
        RendererImgui(Engine<ATYPE>& engine, Window<ATYPE>& window);
        virtual ~RendererImgui();

    private:
        virtual void Render() override;

        Window<ATYPE>& m_window;
    };

};   // namespace vve
