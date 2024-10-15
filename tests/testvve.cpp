
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"

#include "VEEngine.h"

int imgui_SDL2();

int main(int argc, char** argv) {

    vve::VeEngine engine;

    imgui_SDL2();

    std::cout << "Hello world\n";
    return 0;
}

