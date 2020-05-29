export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;

export namespace vve {

    ///----------------------------------------------------------------------------------
    /// \brief Maps base class
    ///----------------------------------------------------------------------------------

    struct VeMapBase {
    };


    ///----------------------------------------------------------------------------------
    /// Hashed slot map class 
    ///----------------------------------------------------------------------------------

    struct VeSlotMap : VeMapBase {
        VeIndex d_first_free;
        static const uint32_t initial_hash_map_size = 64;


        struct slot_map_t {
            VeGuid          d_guid;             ///guid of entry
            VeTableIndex    d_table_index;      ///points to chunk and in chunk entry
            VeIndex         d_next;             ///next free or next with same GUID hash map index
            slot_map_t(VeGuid guid, VeTableIndex table_index, VeIndex next) : ///in place consturctor
                d_guid(guid), d_table_index(table_index), d_next(next) {};
        };

        std::vector<slot_map_t>	d_slot_map;     ///map pointing to entries in chunks

        VeSlotMap() : d_first_free(VeIndex::NULL()), d_slot_map() {};
        VeIndex                     insert( VeGuid guid, VeTableIndex table_index );
        std::optional<VeTableIndex> at(VeHandle handle);
        void                        erase(VeHandle handle);
    };

    ///----------------------------------------------------------------------------------
    /// \brief Insert a new item into the slot map and the hash map
    /// \param[in] guid The GUID of the new item
    /// \param[in] table_index The table index of the item that is inserted
    /// \returns the fixed slot map index of the new item to be stored in handles and other maps
    ///----------------------------------------------------------------------------------
    VeIndex VeSlotMap::insert(VeGuid guid, VeTableIndex table_index) {
        VeIndex new_slot;
 
        if (d_first_free != VeIndex::NULL()) {              //there is a free slot in the slot map
            new_slot = d_first_free;                        //point to the free slot to use
            d_first_free = d_slot_map[new_slot].d_next;     //let first_free point to the next free slot or NULL
            d_slot_map[new_slot] = { guid, table_index, VeIndex::NULL() };
        }
        else {
            new_slot = VeIndex((decltype(VeIndex::value))d_slot_map.size());    //point to the new slot
            d_slot_map.emplace_back(guid, table_index, VeIndex::NULL());        //create the new slot
        }
        return new_slot;
    }

    ///----------------------------------------------------------------------------------
    /// \brief
    ///----------------------------------------------------------------------------------
    std::optional<VeTableIndex> VeSlotMap::at(VeHandle handle) {
        if(!(handle.d_index < d_slot_map.size())) return std::nullopt;

        if (d_slot_map[handle.d_index].d_guid == handle.d_guid) return d_slot_map[handle.d_index].d_table_index;
        return std::nullopt;
    }


    ///----------------------------------------------------------------------------------
    /// \brief
    ///----------------------------------------------------------------------------------
    void VeSlotMap::erase(VeHandle handle) {

    }



    ///----------------------------------------------------------------------------------
    /// \brief Hash map
    ///----------------------------------------------------------------------------------

    template<int... Is>
    struct VeHashMap : VeMapBase {
        static constexpr auto s_indices = std::make_tuple(Is...);
        static const uint32_t initial_hash_map_size = 64;

        struct slot_map_t {
            VeTableIndex d_table_index;     ///points to table entry
            VeIndex32    d_next_index;      ///next entry with same hashed slot value
            VeIndex32    d_next_free;
        };

        std::vector<slot_map_t> d_slot_map;     //slot map points to table entry
        std::vector<VeIndex32>  d_hash_map;     //hashed value points to slot map

        VeHashMap() : d_slot_map(initial_hash_map_size), d_hash_map(initial_hash_map_size) {
            d_slot_map.clear();
        }
    };




};
