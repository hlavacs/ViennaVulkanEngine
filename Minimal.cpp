#include "vulkan/vulkan.h"
#include "glm.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"

auto InitVulkan() {

}

auto AcquireNextImage() {

}

auto RecordCommandBuffer() {

}

auto QueueSubmit() {
}

auto QueuePresent() {

}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("Minimal Vulkan", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    InitVulkan();

    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) running = false; }
        AcquireNextImage();
        RecordCommandBuffer();
        QueueSubmit();
        QueuePresent();
    }

    SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
    return 0;
}
