import std.core;

import VVE;


int main()
{
    using namespace vve;

    VeTableToAChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof ToAChunk << "\n";

    VeTableAoTChunk<uint64_t, float, uint64_t, float> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";


    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");
    
    std::cout << hash(tuple1) << " " << hash(tuple2) << "\n";

    VeHandle<VeGuid32> handle;

    std::cout << "Hello World!\n";
    
    return 0;
}

