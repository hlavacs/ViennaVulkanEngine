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
        Renderer(std::string name, Engine<ATYPE>& m_engine, Window<ATYPE>* window);
        virtual ~Renderer();

    protected:
        virtual void PrepareRender();
        virtual void Render();
        Window<ATYPE>* m_window;
    };

};   // namespace vve