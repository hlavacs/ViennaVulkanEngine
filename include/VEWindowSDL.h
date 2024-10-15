#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include "VEWindow.h"


namespace vve {

    class VeEngine;

    class VeWindowSDL : public VeWindow {
    
    public:
        VeWindowSDL(VeEngine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*>& instance_extensions);
        virtual ~VeWindowSDL();
        virtual auto getSurface(VkInstance instance) -> VkSurfaceKHR override;
        virtual void render() override;
        
    private:
        bool InitSDL(VkInstance instance);
        inline static bool sdl_initialized{false};
        SDL_Window* m_window{nullptr};
    };


};  // namespace vve

