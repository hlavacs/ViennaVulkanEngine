#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace vve {

    class VeWindow
    {
    public:
        VeWindow(VkInstance instance, std::string windowName, int width, int height);
        virtual ~VeWindow();

        VkSurfaceKHR getSurface() { return m_surface; }
    private:
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    };

};   // namespace vve

