
#include "VEWindowSDL.h"

namespace vve {

    VeWindowSDL::VeWindowSDL(VeEngine& engine, VkInstance instance, std::string windowName
            , int width, int height, std::vector<const char*> instance_extensions) 
                : VeWindow(engine, instance, windowName, width, height, instance_extensions) {

        if(!sdl_initialized) {
            sdl_initialized = InitSDL(instance);
            if(!sdl_initialized)  return;
        }

        // Create window with Vulkan graphics context
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        SDL_Window* m_window = SDL_CreateWindow(windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
        if (m_window == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            return;
        }

        uint32_t extensions_count = 0;
        std::vector<const char*> extensions;
        SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, nullptr);
        extensions.resize(extensions_count);
        SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, extensions.data());
        instance_extensions.insert(instance_extensions.end(), extensions.begin(), extensions.end());
    }

    VeWindowSDL::~VeWindowSDL(){}

    bool VeWindowSDL::InitSDL(VkInstance instance) {
        // Setup SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
            printf("Error: %s\n", SDL_GetError());
            return false;
        }

        // From 2.0.18: Enable native IME.
    #ifdef SDL_HINT_IME_SHOW_UI
        SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    #endif

        return true;
    }

    auto VeWindowSDL::getSurface( VkInstance instance ) -> VkSurfaceKHR { 
        if(m_surface != VK_NULL_HANDLE) return m_surface; 

        // Create Window Surface
        if (SDL_Vulkan_CreateSurface(m_window, instance, &m_surface) == 0)
        {
            printf("Failed to create Vulkan surface.\n");
            return VK_NULL_HANDLE;
        }
        return m_surface; 
    }


};  // namespace vve
