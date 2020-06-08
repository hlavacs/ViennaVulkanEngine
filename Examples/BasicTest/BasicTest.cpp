import std.core;
import std.regex;
import std.filesystem;
import std.memory;
import std.threading;

import VVE;
#include "VEHash.h"

void test(vve::VeIndex idx) {

}


using namespace vve;


int main()
{

    test(3);

    VeHandle handle1;
    VeHandle handle2;
    //if (handle1 == handle2) return 0;

    std::tuple<VeHandle, VeHandle> ht;


    VeIndex vidx1(1);
    VeIndex vidx2(2);
    if (vidx1 > vidx2) return 0;

    std::unordered_map<VeGuid, int> wmap;

    std::cout << sizeof(VeTableIndex) << std::endl;
    std::cout << sizeof(VeHandle) << std::endl;

    VeTableChunk<uint64_t, float, uint64_t> ToAChunk;
    std::cout << sizeof(ToAChunk) << "\n";
    auto idx1 = ToAChunk.insert( 1, { 4, 2.0f, 90 });
    auto idx = ToAChunk.insert( 2, { 5, 2.5f, 97 });
    idx = ToAChunk.insert( 3, { 6, 2.5f, 97 });
    idx = ToAChunk.insert( 4, { 7, 2.5f, 97 });
    idx = ToAChunk.insert( 5, { 8, 2.5f, 97 });
    idx = ToAChunk.insert( 6, { 9, 2.5f, 97 });

    VeIndex slotmap;
    auto tuple = ToAChunk.at(idx1, slotmap);
    ToAChunk.pop_back();


    VeTableState< Typelist< uint64_t, float, uint64_t, char>, Typelist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATableState;


    /*
    VeTable< Typelist< uint64_t, float, uint64_t, char>, Typelist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< Typelist< std::tuple<uint64_t, float, uint64_t>, char, char >, Typelist< Hashlist<0>, Hashlist< 0, 1, 2>, Hashlist<0, 2>, Hashlist<1, 2>> > AoTTable;
    */


    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");

    std::cout << std::hash<decltype(tuple1)>()(tuple1) << " " << std::hash<decltype(tuple2)>()(tuple2) << "\n";

    std::cout << "Hello World!\n";

    return 0;
}
