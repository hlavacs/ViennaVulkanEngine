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

    VeTableToAChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof ToAChunk << "\n";

    VeTableAoTChunk<uint64_t, float, uint64_t, float> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";


    std::tuple tuple1(1);
    std::tuple tuple2(1, 3.4, "333");
    
    std::cout << hash(tuple1) << " " << hash(tuple2) << "\n";

    VeHandle<VeGuid32> handle;

    std::cout << "Hello World!\n";
    
    return 0;
}

