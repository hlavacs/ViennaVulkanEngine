#pragma once


namespace vve
{

    enum class VeRendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };


    class VeRenderer {

    public:
        VeRenderer();
        virtual ~VeRenderer();

    private:

    };

}   // namespace vve