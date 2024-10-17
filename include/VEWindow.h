#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "VEInclude.h"
#include "VERenderer.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class Window {

        friend class Engine<ATYPE>;

    public:
        Window(Engine<ATYPE>& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~Window();

        virtual std::pair<int, int> getSize() = 0;
        virtual void setClearColor(glm::vec4 clearColor);
        
    protected:
        virtual void Init() = 0;
        virtual bool pollEvents() = 0;
        virtual void prepareNextFrame() = 0;
        virtual void renderNextFrame() = 0;        
        void setRenderer(Renderer<ATYPE>* renderer);

        Engine<ATYPE>& m_engine;
        Renderer<ATYPE>* m_renderer{nullptr};
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

