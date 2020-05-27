export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;


export namespace vve {



    struct VeMapBase {
    };


    template<int NUM>
    struct VeSlotMap : VeMapBase {
        VeInChunkIndex			        d_first_free;

        std::array<VeGuid, NUM>			d_slot_guid;
        std::array<VeInChunkIndex, NUM>	d_slot_next_index;
        std::array<VeInChunkIndex, NUM>	d_hash_map;

        VeSlotMap() : d_first_free(VeInChunkIndex(0)) {};
    };


    template<int... Is>
    struct VeHashMap : VeMapBase {
        static constexpr auto s_indices = std::make_tuple(Is...);
        static const uint32_t initial_size = 64;

        struct slot_map_t {
            VeTableIndex d_table_index;     ///points to table entry
            VeIndex32    d_next_index;      ///next entry with same hashed slot value
            VeIndex32    d_next_free;
        };

        std::vector<slot_map_t> d_slot_map;     //slot map points to table entry
        std::vector<VeIndex32>  d_hash_map;     //hashed value points to slot map

        VeHashMap() : d_slot_map(initial_size), d_hash_map(initial_size) {
            d_slot_map.clear();
        }
    };




};
