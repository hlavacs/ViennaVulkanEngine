#include <iostream>
#include <functional>
#include <tuple>
#include <array>
#include <map>
#include <type_traits>
#include <utility>


#include "VVE.h"



int main()
{
    using namespace vve;

    VeTableToAChunk<uint64_t, float, uint64_t> tuple;
    std::cout << sizeof tuple << "\n";

    VeTableAoTChunk<uint64_t, float, uint64_t, float> tuple2;
    std::cout << sizeof tuple2 << "\n";

    VeHandle<VeGuid32> handle;

    std::cout << "Hello World!\n";
    
    return 0;
}

