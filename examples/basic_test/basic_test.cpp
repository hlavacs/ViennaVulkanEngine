
#include <iostream>
#include <utility>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "vulkan/vulkan.h"
#include "vve.h"
#include "basic_test.h"

int imgui();

using namespace vve;

int main() {
    VeEntityManager et;

    std::cout << sizeof(VeHandle) << " "  << sizeof(VeHandle_t<VeEntityType<>>) << " " << sizeof(index_t) << std::endl;
    std::cout << tl::size_of<VeEntityTypeList>::value << std::endl;

    VeHandle h1 = et.insert<VeEntityTypeNode>(VeComponentPosition{ glm::vec3{1.0f, 2.0f, 3.0f} }, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityTypeNode).hash_code() << " " << typeid(VeEntityTypeNode).name() << std::endl;

    auto data = et.get(h1);

    et.erase(h1);

    VeHandle h2 = et.insert<VeEntityTypeDraw>( VeComponentMaterial{}, VeComponentGeometry{});
    std::cout << typeid(VeEntityTypeDraw).hash_code() << " " << typeid(VeEntityTypeDraw).name() << std::endl;
    et.erase(h2);


    imgui();
    std::cout << "Hello world\n";
    return 0;
}

