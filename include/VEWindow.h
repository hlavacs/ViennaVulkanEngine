#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "VEInclude.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class Renderer;

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
        virtual void addRenderer(int64_t priority, std::shared_ptr<Renderer<ATYPE>> renderer);

        Engine<ATYPE>& m_engine;
        std::map< int64_t, std::shared_ptr<Renderer<ATYPE>>> m_renderer;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

