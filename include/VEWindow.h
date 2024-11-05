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
        Window( Engine<ATYPE>* engine, std::string windowName, int width, int height, std::string name = "VVE Window" );
        virtual ~Window();

        virtual auto GetSize() -> std::pair<int, int> = 0;
        virtual void SetClearColor(glm::vec4 clearColor);
        
    protected:
        virtual void AddRenderer(std::shared_ptr<Renderer<ATYPE>> renderer);

        int m_width;
        int m_height;
        std::string m_windowName;
        std::vector<std::shared_ptr<Renderer<ATYPE>>> m_renderer;
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

