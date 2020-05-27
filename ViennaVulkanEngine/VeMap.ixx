export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;


export namespace vve {



    struct VeMapBase {
    };


    struct VeSlotMap : VeMapBase {
        VeIndex d_first_free;

        struct slot_map_t {
            VeGuid          d_guid;             //guid of entry
            VeIndex         d_next;             //next free or next with same GUID hash map index
            VeTableIndex    d_table_index;      //points to chunk and in chunk entry
        };

        std::vector<slot_map_t>	d_slot_map;
        std::vector<VeIndex>    d_hash_map;

        VeSlotMap() : d_first_free() {};
        VeIndex         insert( VeGuid guid, VeTableIndex table_index );
        VeTableIndex    at(VeIndex index);
        void            erase(VeIndex index);
    };

    VeIndex VeSlotMap::insert(VeGuid guid, VeTableIndex table_index) {
        VeIndex idx = d_first_free;
        if (d_first_free != VeIndex::NULL()) {
            d_first_free = d_slot_map[idx].d_next;
        }
        return idx;
    }

    VeTableIndex VeSlotMap::at(VeIndex index) {
        return VeTableIndex();
    }


    void VeSlotMap::erase(VeIndex index) {

    }



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
