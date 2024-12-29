#pragma once

#include <memory>
#include <any>


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
        Renderer(std::string systemName, Engine<ATYPE>& m_engine );
        virtual ~Renderer();

    protected:
		virtual void OnAnnounce(Message message);
        Window<ATYPE>* m_window;
    };

};   // namespace vve