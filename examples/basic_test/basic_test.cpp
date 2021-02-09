
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
    //et.erase(h);

    VeHandle h2 = et.create(VeEntityDraw{}, VeComponentMaterial{}, VeComponentGeometry{});
    std::cout << typeid(VeEntityDraw).hash_code() << " " << typeid(VeEntityDraw).name() << std::endl;

    auto erase_handle = []<typename E>(VeHandle_t<E> &h) {
        VeHandle_t<E> uu;
        std::cout << typeid(E).hash_code() << " " << typeid(E).name() << std::endl;
        VeComponentReferencePool<E>();
        int i = 0;
    };

    std::visit(erase_handle, h1);
    std::visit(erase_handle, h2);



    imgui();
    std::cout << "Hello world\n";
    return 0;
}

