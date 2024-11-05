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
    class WindowSDL : public Window<ATYPE> {

        using Window<ATYPE>::m_engine;
        using Window<ATYPE>::m_renderer;
        using Window<ATYPE>::m_width;
        using Window<ATYPE>::m_height;
        using Window<ATYPE>::m_windowName;
        using Window<ATYPE>::m_surface;
        using Window<ATYPE>::m_instance_extensions;
   
    public:
        WindowSDL(Engine<ATYPE>* engine, std::string windowName, int width, int height, std::string name = "VVE WindowSDL" );
        virtual ~WindowSDL();
        auto GetMinImageCount() -> int { return m_minImageCount; };
        auto GetSwapChainRebuild() -> bool { return m_swapChainRebuild; };
        void SetSwapChainRebuild(bool rebuild) { m_swapChainRebuild = rebuild; };

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPollEvents(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
        ImGuiIO* m_io;
        int m_minImageCount = 2;
        bool m_isMinimized = false;
        bool m_swapChainRebuild = false;
        std::set<SDL_Scancode> m_keysDown;
        std::set<uint8_t> m_mouseButtonsDown;
    };


};  // namespace vve

