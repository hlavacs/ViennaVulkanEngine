export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;


export namespace vve { 
    template <typename T, int... Is>
    auto makeKey(T& t) {
        return std::tuple(std::get<Is>(t) ...);
    }

    template <typename T, int... Is>
    struct typed_map {

        std::map<T, int> m;
        typed_map() : m() {};

        void mapValue(T& t, int val) {
            auto key = makeKey<T, Is...>(t);
            m[key] = val;
        };
    };


    struct VeMapBase {
    };


    template<int NUM>
    struct VeSlotMap : VeMapBase {
        std::array<VeGuid, NUM>			d_slot_guid;
        std::array<VeInChunkIndex, NUM>	d_slot_next_index;
        std::array<VeInChunkIndex, NUM>	d_hash_map;
        std::array<VeInChunkIndex, NUM>	d_slot_map_index;
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





