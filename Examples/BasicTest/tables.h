
/*import std.core;
import std.regex;
import std.filesystem;
import std.memory;
import std.threading;

import VVE;
#include "VEDefine.h"
#include "VEHash.h"
*/


namespace tables {
    using namespace vve;

	int test() {

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

        VeTableChunk< (1 << 14), uint64_t, float, uint32_t> ToAChunk;
        std::cout << sizeof(ToAChunk) << "\n";
        auto idx1 = ToAChunk.insert(1, 4, 2.0f, 90);
        auto idx2 = ToAChunk.insert(2, 5, 2.5f, 97);
        auto idx3 = ToAChunk.insert(3, 6, 2.5f, 97);
        auto idx4 = ToAChunk.insert(4, 7, 2.5f, 91);
        auto idx5 = ToAChunk.insert(5, 8, 2.5f, 92);
        auto idx6 = ToAChunk.insert(6, 9, 2.5f, 93);

        VeIndex slotmap;
        auto tuple = ToAChunk.at(idx1, slotmap);
        ToAChunk.pop_back();


        VeTableState<1 << 14, Typelist< uint64_t, float, uint32_t, char>, Maplist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATableState;
        auto h1 = ToATableState.insert(4, 2.0f, 90, 'a');
        ToATableState.update(h1, 5, 3.0f, 91, 'b');
        std::promise<VeHandle> prom;
        auto fut = prom.get_future();
        auto h2 = ToATableState.insert(std::move(prom), 40, 20.0f, 900, 'c');
        auto h3 = fut.get();

        const int N = 100;
        VeAssert((ToATableState.size() == 2));
        for (uint64_t i = 0; i < N; i++) {
            auto h = ToATableState.insert(i, 2.0f * i, (uint32_t)i * 5, 'a');
        }
        VeAssert((ToATableState.size() == N + 2));

        auto [i64, fl, i32, ch] = ToATableState.at(h1);
        auto [i64b, flb, i32b, chb] = ToATableState.at(h3);
        VeHandle h;

        for (auto it = ToATableState.begin(); it != ToATableState.end(); ++it) {
            auto [i64c, flc, i32c, chc, han] = *it;

            std::cout << i64c << " " << flc << " " << i32c << " " << chc << std::endl;
            it.operator*<2>() = 11;
            std::cout << it.operator*<0>() << " " << it.operator*<1>() << " " << it.operator*<2>() << " " << it.operator*<3>() << std::endl;
        }
        ToATableState.erase(h1);
        ToATableState.erase(h2);
        ToATableState.erase(h3);

        ToATableState.clear();

        h1 = ToATableState.insert(4, 2.0f, 90, 'a');
        h2 = ToATableState.insert(40, 20.0f, 900, 'b');
        h3 = ToATableState.insert(80, 30.0f, 1800, 'c');

        auto res = ToATableState.find<0>((uint64_t)4, 2.0f, (uint32_t)90);

        VeTableState<1 << 14, Typelist< uint64_t, float, uint32_t, char>, Maplist< Hashlist< 0, 1, 2>, Hashlist< 1, 2 >> > ToATableState2;

        ToATableState2 = ToATableState;



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

	}

}


