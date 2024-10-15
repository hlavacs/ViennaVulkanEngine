

#include "VEWindowSDL.h"

using namespace vve;

VeWindowSDL::VeWindowSDL(VkInstance instance, std::string windowName, int width, int height) : VeWindow(instance, windowName, width, height) {
    if(!sdl_initialized) {
        sdl_initialized = InitSDL(instance);
    }

}

VeWindowSDL::~VeWindowSDL(){}

bool VeWindowSDL::InitSDL(VkInstance instance) {
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return false;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
    
        // Create window with Vulkan graphics context
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* m_window = SDL_CreateWindow("Dear ImGui SDL2+Vulkan example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (m_window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return false;
    }

    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, nullptr);
    m_extensions.resize(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(m_window, &extensions_count, m_extensions.data());
    
    //////////////////SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err;
    //if (SDL_Vulkan_CreateSurface(window, g_Instance, &surface) == 0)
    {
        printf("Failed to create Vulkan surface.\n");
        return false;
    }

    // Create Framebuffers
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);

    return true;
}
