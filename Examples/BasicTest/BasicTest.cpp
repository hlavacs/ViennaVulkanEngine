import std.core;

import VVE;



int main()
{
    using namespace vve;

    std::cout << sizeof(VeTableIndex) << std::endl;
    std::cout << sizeof(VeHandle) << std::endl;

    std::cout << sizeof(VeSlotMap<1>) << std::endl;
    std::cout << sizeof(VeSlotMap<511>)/511 << std::endl;

    VeHashMap<1, 2, 3> map;
    auto vrt = decltype(map)::s_indices;

    VeTableChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof ToAChunk << "\n";
    VeTable< Typelist< uint64_t, float, uint64_t>, Typelist< VeHashMap< 1, 2, 3>, VeHashMap< 1, 3 >> > ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< Typelist< std::tuple<uint64_t, float, uint64_t> >, Typelist<VeHashMap< 1, 2, 3>, VeHashMap<2, 3>> > AoTTable;

    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");

    std::cout << hash(tuple1) << " " << hash(tuple2) << "\n";

    VeHandle handle;

    std::cout << "Hello World!\n";

    return 0;
}
