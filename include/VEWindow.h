#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
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
        virtual void prepareNextFrame() = 0;
        virtual void renderNextFrame() = 0;
        virtual std::pair<int, int> getSize() = 0;
        virtual void setClearColor(glm::vec4 clearColor);
        
    protected:
        Engine& m_engine;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

