#pragma once

#include "VEInclude.h"

namespace vve
{
   	template<ArchitectureType ATYPE>
    class Engine;

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };


   	template<ArchitectureType ATYPE>
    class Renderer {
        friend class Engine<ATYPE>;

    public:
        Renderer(Engine<ATYPE>& m_engine);
        virtual ~Renderer();

    private:
        virtual void Render();
        Engine<ATYPE>& m_engine;
    };

};   // namespace vve