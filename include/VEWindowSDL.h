#pragma once

#include <vector>
#include <set>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include "VEWindow.h"



namespace vve {

   	template<ArchitectureType ATYPE>
    class Engine;

   	template<ArchitectureType ATYPE>
    class RendererImgui;

   	template<ArchitectureType ATYPE>
    class RendererVulkan;

    struct WindowSDLState {
        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
        VkSurfaceKHR m_surface{VK_NULL_HANDLE};
        ImGuiIO* m_io;
        int m_minImageCount = 2;
        bool m_isMinimized = false;
        bool m_swapChainRebuild = false;
        std::set<SDL_Scancode> m_keysDown;
        std::set<uint8_t> m_mouseButtonsDown;
        std::vector<const char*> m_instance_extensions;
    };

   	template<ArchitectureType ATYPE>
    class WindowSDL : public Window<ATYPE> {

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_renderer;
        using Window<ATYPE>::m_width;
        using Window<ATYPE>::m_height;
        using Window<ATYPE>::m_windowName;

        friend class RendererImgui<ATYPE>;
        //friend class RendererVulkan<ATYPE>;
    
    public:
        WindowSDL(Engine<ATYPE>* engine, std::string windowName, int width, int height, std::string name = "VVE WindowSDL" );
        virtual ~WindowSDL();
        auto GetSDLWindow() -> const SDL_Window* { return m_state.m_window; };
        auto GetSurface() -> VkSurfaceKHR { return m_state.m_surface; };
        auto GetIO() -> const ImGuiIO* { return m_state.m_io; };  
        auto GetMinImageCount() -> int { return m_state.m_minImageCount; };
        auto IsMinimized() -> bool { return m_state.m_isMinimized; };
        auto GetInstanceExtensions() -> std::vector<const char*> { return m_state.m_instance_extensions; }; 

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPollEvents(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

        WindowSDLState m_state;
    };


};  // namespace vve

