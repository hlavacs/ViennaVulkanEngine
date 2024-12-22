#pragma once

#include <memory>
#include <any>
#include "VEInclude.h"
#include "VESystem.h"   

namespace vve
{

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
        Renderer(std::string systemName, Engine<ATYPE>& m_engine, Window<ATYPE>* window );
        virtual ~Renderer();

    protected:
        Window<ATYPE>* m_window;
    };

};   // namespace vve