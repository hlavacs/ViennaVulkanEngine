import std.core;

import VVE;



int main()
{
    using namespace vve;

    VeMap<1, 2, 3> map;
    auto vrt = decltype(map)::s_indices;

    VeTableChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof ToAChunk << "\n";
    VeTable< VeGuid32, Typelist< uint64_t, float, uint64_t>, Typelist<VeMap<1, 2, 3>, VeMap<1, 3>> > ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< VeGuid32, Typelist< std::tuple<uint64_t, float, uint64_t> >, Typelist<VeMap<1, 2, 3>, VeMap<2, 3>> > AoTTable;

    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");
    
    std::cout << hash(tuple1) << " " << hash(tuple2) << "\n";

    VeHandle<VeGuid32> handle;

    std::cout << "Hello World!\n";
    
    return 0;
}

