
#include <iostream>
#include <utility>
#include "vulkan/vulkan.h"
#include "vve.h"
#include "basic_test.h"

int imgui();

using namespace vve;

int main() {
    VeEntityManager et;

    VeHandle h = et.create(VeEntityNode{}, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
    et.erase(h);

    imgui();
    std::cout << "Hello world\n";
    return 0;
}

