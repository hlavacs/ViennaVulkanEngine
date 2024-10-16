
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"

#include "VEEngine.h"

int main() {

    vve::VeEngine engine;

    engine.Run();

    std::cout << "Hello world\n";
    return 0;
}

