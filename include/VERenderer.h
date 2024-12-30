#pragma once

#include <memory>
#include <any>


namespace vve
{

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };

    class Renderer : public System {
        friend class Engine;

    public:
        Renderer(std::string systemName, Engine& m_engine, std::string windowName);
        virtual ~Renderer();
        auto GetSurface() -> VkSurfaceKHR { return m_surface; };

    protected:
		void OnAnnounce(Message message);
        VkSurfaceKHR	m_surface{VK_NULL_HANDLE};
		std::string 	m_windowName;
        Window* 	m_window;
    };

};   // namespace vve