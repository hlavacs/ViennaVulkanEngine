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
    class System;

   	template<ArchitectureType ATYPE>
    class Window : public System<ATYPE> {

        friend class Engine<ATYPE>;

    public:
        Window( std::string name, Engine<ATYPE>& engine, VkInstance instance, std::string windowName
                , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~Window();

        virtual auto GetSize() -> std::pair<int, int> = 0;
        virtual void SetClearColor(glm::vec4 clearColor);
        
    protected:
        virtual void Init() = 0;
        virtual void AddRenderer(int64_t priority, std::shared_ptr<Renderer<ATYPE>> renderer);

        std::map< int64_t, std::shared_ptr<Renderer<ATYPE>>> m_renderer;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

