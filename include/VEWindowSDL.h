#pragma once

#include "VeWindow.h"
#include <vector>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace vve {

    class VeWindowSDL : public VeWindow {
    
    public:
        VeWindowSDL(std::string windowName, int width, int height);
        virtual ~VeWindowSDL();
    
    private:
        bool InitSDL();
        inline static bool sdl_initialized{false};
        SDL_Window* window{nullptr};
        ImGui_ImplVulkanH_Window g_MainWindowData;	

    };


};  // namespace vve

