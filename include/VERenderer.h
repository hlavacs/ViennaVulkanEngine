#pragma once


namespace vve
{
    class Engine;

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };


    class Renderer {
        friend class Engine;

    public:
        Renderer(Engine& m_engine);
        virtual ~Renderer();

    private:
        virtual void Render();
        Engine& m_engine;
    };

};   // namespace vve