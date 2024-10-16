#pragma once


namespace vve
{

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };


    class Renderer {

    public:
        Renderer();
        virtual ~Renderer();

    private:

    };

};   // namespace vve