#pragma once

#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>
//#include <vulkan/vulkan.h>
#include "VEInclude.h"
#include "VHInclude.h"
#include "VERenderer.h"

namespace vve {

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
        auto GetSurface() -> VkSurfaceKHR { return m_surface; };
        auto GetInstanceExtensions() -> std::vector<const char*> { return m_instanceExtensions; }; 
		bool GetIsMinimized() { return m_isMinimized; }

    protected:
        int 			m_width;
        int 			m_height;
        std::string 	m_windowName;
        glm::vec4 		m_clearColor{0.45f, 0.55f, 0.60f, 1.00f};
        VkSurfaceKHR	m_surface{VK_NULL_HANDLE};
	    bool 			m_isMinimized = false;
        std::vector<const char*> m_instanceExtensions;

    };

};   // namespace vve

