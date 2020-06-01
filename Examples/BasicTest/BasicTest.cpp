import std.core;
import std.regex;
import std.filesystem;
import std.memory;
import std.threading;

import VVE;
#include "VETypes.h"

void test(vve::VeIndex idx) {

}


using namespace vve;


int main()
{

    test(3);

    VeHandle handle1;
    VeHandle handle2;
    //if (handle1 == handle2) return 0;

    VeIndex vidx1(1);
    VeIndex vidx2(2);
    if (vidx1 > vidx2) return 0;

    std::unordered_map<VeGuid, int> wmap;

    std::cout << sizeof(VeTableIndex) << std::endl;
    std::cout << sizeof(VeHandle) << std::endl;

    VeHashMap< 1, 2, 3> map;
    auto vrt = decltype(map)::s_indices;

    VeTableChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof(ToAChunk) << "\n";
    auto idx1 = ToAChunk.insert( { 4, 2.0f, 90 }, VeIndex(1));
    auto idx = ToAChunk.insert( { 5, 2.5f, 97 }, VeIndex(2));
    idx = ToAChunk.insert({ 6, 2.5f, 97 }, VeIndex(3));
    idx = ToAChunk.insert({ 7, 2.5f, 97 }, VeIndex(4));
    idx = ToAChunk.insert({ 8, 2.5f, 97 }, VeIndex(5));
    idx = ToAChunk.insert({ 9, 2.5f, 97 }, VeIndex(6));

    auto tuple = ToAChunk.at(idx1);
    ToAChunk.erase(idx1);
    ToAChunk.erase(idx);

    VeTable< Typelist< uint64_t, float, uint64_t>, Typelist< VeHashMap< 1, 2, 3>, VeHashMap< 1, 3 >> > ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< Typelist< std::tuple<uint64_t, float, uint64_t> >, Typelist<VeHashMap< 1, 2, 3>, VeHashMap<2, 3>> > AoTTable;

    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");

    std::cout << std::hash<decltype(tuple1)>()(tuple1) << " " << std::hash<decltype(tuple2)>()(tuple2) << "\n";

    std::cout << "Hello World!\n";

    return 0;
}
