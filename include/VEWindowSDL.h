#pragma once

#include "VeWindow.h"
#include <SDL.h>
#include <SDL_vulkan.h>

namespace vve {

    class VeWindowSDL : public VeWindow {
    
    public:
        VeWindowSDL(std::string windowName, int width, int height);
        virtual ~VeWindowSDL();
    
    private:
        void initSDL();
        inline static bool sdl_initialized{false};
        SDL_Window* window;
    };


};  // namespace vve

