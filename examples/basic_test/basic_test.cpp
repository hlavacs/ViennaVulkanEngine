
#include <iostream>
#include <utility>
#include "vulkan/vulkan.h"
#include "vve.h"
#include "basic_test.h"

int imgui();

using namespace vve;

int main() {
    VeEntityManager et;

    VeHandle h1 = et.create(VeEntityNode{}, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityNode).hash_code() << " " << typeid(VeEntityNode).name() << std::endl;
    //et.erase(h1);

    VeHandle h2 = et.create(VeEntityDraw{}, VeComponentMaterial{}, VeComponentGeometry{});
    std::cout << typeid(VeEntityDraw).hash_code() << " " << typeid(VeEntityDraw).name() << std::endl;
    //et.erase(h2);


    imgui();
    std::cout << "Hello world\n";
    return 0;
}

