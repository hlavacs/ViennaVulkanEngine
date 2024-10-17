#pragma once

#include <memory>
#include "VEInclude.h"


namespace vve
{
   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class Window;

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };

   	template<ArchitectureType ATYPE>
    class Renderer {
        friend class Engine<ATYPE>;

    public:
        Renderer(Engine<ATYPE>& m_engine, std::weak_ptr<Window<ATYPE>> window);
        virtual ~Renderer();

    private:
        virtual void PrepareRender();
        virtual void Render();
        Engine<ATYPE>& m_engine;
        std::weak_ptr<Window<ATYPE>> m_window;
    };

};   // namespace vve