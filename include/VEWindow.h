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

    public:
        Window( Engine<ATYPE>* engine, std::string windowName, int width, int height, std::string name = "VVE Window" );
        virtual ~Window();

        int GetWidth() { return m_width; };
        int GetHeight() { return m_height; };
        auto GetClearColor() -> glm::vec4 { return m_clearColor; };
        auto GetWindowName() -> std::string { return m_windowName; };
        virtual void SetClearColor(glm::vec4 clearColor);
        virtual void AddRenderer(std::shared_ptr<Renderer<ATYPE>> renderer);

    protected:
        std::vector<std::shared_ptr<Renderer<ATYPE>>> m_renderer;

        int m_width;
        int m_height;
        std::string m_windowName;
        glm::vec4 m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
    };

};   // namespace vve

