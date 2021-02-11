
#include <iostream>
#include <utility>
#include "vulkan/vulkan.h"
#include "vve.h"
#include "basic_test.h"

int imgui();

using namespace vve;

int main() {
    VeEntityManager et;

    std::shared_timed_mutex m1;
    std::mutex m2;
    std::atomic<uint32_t> ai;
    VeReadWriteMutex sm;
    std::cout << sizeof(m1) << " " << sizeof(m2) << " " << sizeof(sm) << " " << sizeof(ai) << std::endl;

    int a = 11;
    int b = 55;
    auto tup = std::make_tuple( std::ref(a) );

    std::cout << std::get<0>(tup) << std::endl;
    std::get<0>(tup) = 22;
    std::cout << std::get<0>(tup) << std::endl;
    std::get<0>(tup) = std::ref(b);
    std::cout << std::get<0>(tup) << std::endl;
    b = 66;
    std::cout << std::get<0>(tup) << std::endl;

    VeHandle h1 = et.create(VeEntityTypeNode{}, VeComponentPosition{}, VeComponentOrientation{}, VeComponentTransform{});
    std::cout << typeid(VeEntityTypeNode).hash_code() << " " << typeid(VeEntityTypeNode).name() << std::endl;
    //et.erase(h1);

    VeHandle h2 = et.create(VeEntityTypeDraw{}, VeComponentMaterial{}, VeComponentGeometry{});
    std::cout << typeid(VeEntityTypeDraw).hash_code() << " " << typeid(VeEntityTypeDraw).name() << std::endl;
    //et.erase(h2);


    imgui();
    std::cout << "Hello world\n";
    return 0;
}

