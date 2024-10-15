#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace vve {

    class VeEngine;

    class VeWindow
    {
    public:
        VeWindow(VeEngine& engine, VkInstance instance, std::string windowName, int width, int height, std::vector<const char*> instance_extensions) ;
        virtual ~VeWindow();
        virtual auto getSurface(VkInstance instance) -> VkSurfaceKHR = 0;

    protected:
        VeEngine& m_engine;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    };

};   // namespace vve

