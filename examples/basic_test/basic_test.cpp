
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
    std::cout << h1.index() << std::endl;
    //et.erase(h);

    VeHandle h2 = et.create(VeEntityDraw{}, VeComponentMaterial{}, VeComponentGeometry{});
    std::cout << h2.index() << std::endl;

    auto erase_handle = []<typename E>(VeHandle_t<E> &h) {
        //VeComponentReferencePool<E>().erase(h.m_next);
        int i = 0;
    };

    std::visit(erase_handle, h1);



    imgui();
    std::cout << "Hello world\n";
    return 0;
}

