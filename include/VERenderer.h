#pragma once

#include <memory>
#include <any>
#include "VEInclude.h"
#include "VESystem.h"
#include "VEEngine.h"


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

        using System<ATYPE>::m_engine;

    public:
        Renderer(Engine<ATYPE>* m_engine, Window<ATYPE>* window, std::string name = "VVE Renderer" );
        virtual ~Renderer();
        virtual auto GetState() -> std::any;

    protected:
        Window<ATYPE>* m_window;
    };

};   // namespace vve