#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace vve {

    class Engine;

    class Window
    {
    public:
        Window(Engine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~Window();
        virtual void Init() = 0;
        virtual bool pollEvents() = 0;
        virtual void renderNextFrame() = 0;
        virtual std::pair<int, int> getSize() = 0;
        
    protected:
        Engine& m_engine;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    };

};   // namespace vve

