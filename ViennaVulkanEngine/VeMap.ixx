export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;
#include "VETypes.h"

export namespace vve {

    ///----------------------------------------------------------------------------------
    /// Hashed slot map
    ///----------------------------------------------------------------------------------

    struct VeSlotMap {
        static const uint32_t initial_hash_map_size = 64;

        struct slot_map_t {
            VeGuid          d_guid;     ///guid of entry
            VeTableIndex    d_table_index;    ///points to chunk and in chunk entry
            VeIndex         d_next;     ///next free slot or next slot with same hash index or NULL

            slot_map_t(VeGuid guid, VeTableIndex table_index, VeIndex next) : 
                d_guid(guid), d_table_index(table_index), d_next(next) {};


        };

        VeIndex                 d_first_free;   ///first free entry in slot map
        std::vector<slot_map_t>	d_slot_map;  ///map pointing to entries in chunks
        std::vector<VeIndex>    d_hash_map;     ///hashed value points to slot map

        VeSlotMap();
        VeIndex         insert(VeGuid guid, VeTableIndex table_index);
        VeTableIndex    at(VeHandle handle);
        bool            erase(VeHandle handle);
    };


    VeSlotMap::VeSlotMap() : d_first_free(VeIndex::NULL()), d_slot_map(), d_hash_map(initial_hash_map_size) {
        d_hash_map.clear();
    };

    ///----------------------------------------------------------------------------------
    /// \brief Insert a new item into the slot map and the hash map
    /// \param[in] guid The GUID of the new item
    /// \param[in] table_index The table index of the item that is inserted
    /// \returns the fixed slot map index of the new item to be stored in handles and other maps
    ///----------------------------------------------------------------------------------
    VeIndex VeSlotMap::insert(VeGuid guid, VeTableIndex table_index) {
        VeIndex new_slot;
        VeIndex hash_index = (decltype(hash_index.value))std::hash<decltype(guid)>()(guid);

        if (d_first_free != VeIndex::NULL()) {              //there is a free slot in the slot map
            new_slot = d_first_free;                        //point to the free slot to use
            d_first_free = d_slot_map[new_slot].d_next;     //let first_free point to the next free slot or NULL
            d_slot_map[new_slot] = { guid, table_index, d_hash_map[hash_index] }; //write over free slot
        }
        else {
            new_slot = VeIndex((decltype(VeIndex::value))d_slot_map.size());        //point to the new slot
            d_slot_map.emplace_back(guid, table_index, d_hash_map[hash_index]);     //create the new slot
        }
        d_hash_map[hash_index] = new_slot;
        return new_slot;
    }

    ///----------------------------------------------------------------------------------
    /// \brief Get a table index from the slot map
    /// \param[in] A handle holding the index to the slot map
    /// \returns the table index or NULL
    ///----------------------------------------------------------------------------------
    VeTableIndex VeSlotMap::at(VeHandle handle) {
        VeIndex index = handle.d_index;
        if (index.value == VeIndex::NULL()) {
            index = (decltype(index.value))std::hash<decltype(handle)>()(handle) % d_hash_map.size();
            while (index != VeIndex::NULL() && d_slot_map[index].d_guid != handle.d_guid) {
                index = d_slot_map[index].d_next;
            }
            if (index.value == VeIndex::NULL()) return VeTableIndex::NULL();
        }

        if(!(index < d_slot_map.size())) return VeTableIndex::NULL();
        return d_slot_map[handle.d_index].d_table_index;
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase an entry from the slot map
    /// \param[in] The handle holding an index to the slot map
    /// \returns true if the entry was found and removed, else false
    ///----------------------------------------------------------------------------------
    bool VeSlotMap::erase(VeHandle handle) {
        VeIndex index = handle.d_index;
        VeIndex prev = VeIndex::NULL();

        if (d_slot_map[handle.d_index].d_guid == handle.d_guid) {
            d_slot_map[handle.d_index] = { VeGuid::NULL(), VeTableIndex::NULL(), d_first_free };
            d_first_free = handle.d_index;
            return true;
        }
        return false;
    }


    ///----------------------------------------------------------------------------------
    /// \brief Hash map
    ///----------------------------------------------------------------------------------

    template<int... Is>
    struct VeHashMap : VeSlotMap {
        static constexpr auto s_indices = std::make_tuple(Is...);

        VeHashMap() : VeSlotMap() {};

        VeIndex insert(size_t hash_value, VeTableIndex table_index);

    };

    /*VeIndex VeSlotMap::insert(VeGuid guid, VeTableIndex table_index) {
        VeIndex new_slot;
        VeIndex hash_index = (decltype(hash_index.value))std::hash<decltype(guid.value)>()(guid.value);

        if (d_first_free != VeIndex::NULL()) {              //there is a free slot in the slot map
            new_slot = d_first_free;                        //point to the free slot to use
            d_first_free = d_slot_map[new_slot].d_next;     //let first_free point to the next free slot or NULL
            d_slot_map[new_slot] = { guid, table_index, d_hash_map[hash_index] }; //write over free slot
        }
        else {
            new_slot = VeIndex((decltype(VeIndex::value))d_slot_map.size());        //point to the new slot
            d_slot_map.emplace_back(guid, table_index, d_hash_map[hash_index]);     //create the new slot
        }
        d_hash_map[hash_index] = new_slot;
        return new_slot;
    }*/


};
