
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"
#include "vve.h"

int imgui_SDL2();
int imgui_glfw3();

using namespace vecs;

int main() {

    //imgui_glfw3();
    imgui_SDL2();

    std::cout << "Hello world\n";
    return 0;
}

