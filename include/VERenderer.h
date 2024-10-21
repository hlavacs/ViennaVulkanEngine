#pragma once

#include <memory>
#include "VEInclude.h"


namespace vve
{
   	template<ArchitectureType ATYPE>
    class System;

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
    class Renderer : public System<ATYPE> {
        friend class Engine<ATYPE>;

    public:
        Renderer(Engine<ATYPE>& m_engine, std::weak_ptr<Window<ATYPE>> window);
        virtual ~Renderer();

    protected:
        virtual void PrepareRender();
        virtual void Render();
        std::weak_ptr<Window<ATYPE>> m_window;
    };

};   // namespace vve