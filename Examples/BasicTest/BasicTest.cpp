
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

    VeTableChunk<uint64_t, float, uint32_t> ToAChunk;
    std::cout << sizeof(ToAChunk) << "\n";
    auto idx1 = ToAChunk.insert( 1, 4, 2.0f, 90 );
    auto idx2 = ToAChunk.insert( 2, 5, 2.5f, 97 );
    auto idx3 = ToAChunk.insert( 3, 6, 2.5f, 97 );
    auto idx4 = ToAChunk.insert( 4, 7, 2.5f, 91 );
    auto idx5 = ToAChunk.insert( 5, 8, 2.5f, 92 );
    auto idx6 = ToAChunk.insert( 6, 9, 2.5f, 93 );

    VeIndex slotmap; 
    auto tuple = ToAChunk.at(idx1, slotmap);
    ToAChunk.pop_back();


    VeTableState< Typelist< uint64_t, float, uint32_t, char>, Maplist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATableState;
    auto h1 = ToATableState.insert(  4, 2.0f, 90, 'a' );
    ToATableState.update(h1, 5, 3.0f, 91, 'b');
    std::promise<VeHandle> prom;
    auto fut = prom.get_future();
    auto h2 = ToATableState.insert( std::move(prom), 40, 20.0f, 900, 'c');
    auto h3 = fut.get();

    //auto it = ToATableState.begin();
    //auto [handle1, a, b, c, d] = *it;

    VeAssert(ToATableState.size() == 1);
    for (uint64_t i = 0; i < 10000; i++) {
        auto h = ToATableState.insert( i, 2.0f*i, (uint32_t)i*5, 'a' );
    }
    VeAssert(ToATableState.size() == 10001);
    ToATableState.clear();

 
    /*
    VeTable< Typelist< uint64_t, float, uint64_t, char>, Maplist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATable;

    VeTableChunk< std::tuple<uint64_t, float, uint64_t>> AoTChunk;
    std::cout << sizeof AoTChunk << "\n";
    VeTable< Typelist< std::tuple<uint64_t, float, uint64_t>, char, char >, Maplist< Hashlist<0>, Hashlist< 0, 1, 2>, Hashlist<0, 2>, Hashlist<1, 2>> > AoTTable;
    */


    auto tuple1 = std::make_tuple(1);
    auto tuple2 = std::make_tuple(1, 3.4, "333");

    std::cout << std::hash<decltype(tuple1)>()(tuple1) << " " << std::hash<decltype(tuple2)>()(tuple2) << "\n";

    std::cout << "Hello World!\n";

    return 0;
}
