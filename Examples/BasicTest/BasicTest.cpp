import std.core;

import VVE;



template<int a>
int Sum() {
    return a;
}

template<int a, int b, int... args>
int Sum() {
    return Sum<b, args...>() + a;
}


int main()
{
    using namespace vve;

    int s = Sum<1, 2, 3>();

    VeMap<1,2,3> map;

    auto t = decltype(map)::s_indices;


    VeTableChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof ToAChunk << "\n";
    VeTable<uint64_t, float, uint64_t> ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< std::tuple<uint64_t, float, uint64_t>> AoTTable;

    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");
    
    std::cout << hash(tuple1) << " " << hash(tuple2) << "\n";

    VeHandle<VeGuid32> handle;

    std::cout << "Hello World!\n";
    
    return 0;
}

