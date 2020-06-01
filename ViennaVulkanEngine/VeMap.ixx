export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;
#include "VETypes.h"

export namespace vve {

    ///----------------------------------------------------------------------------------
    /// Hashed slot map
    ///----------------------------------------------------------------------------------

    class VeSlotMap {
        static const uint32_t initial_hash_map_size = 64;

        struct slot_map_t {
            VeGuid          d_guid;     ///guid of entry
            VeTableIndex    d_table_index;    ///points to chunk and in chunk entry
            VeIndex         d_next;     ///next free slot or next slot with same hash index or NULL

            slot_map_t(VeGuid guid, VeTableIndex table_index, VeIndex next) : 
                d_guid(guid), d_table_index(table_index), d_next(next) {};
        };

        std::tuple<VeIndex, VeIndex, VeIndex>   findInHashMap(VeGuid guid);     //if no index is given, find the slot map inded through the hash map
        VeIndex                                 eraseFromHashMap(VeGuid guid);  //remove an entry from the hash map

    public:
        VeIndex                 d_first_free;   ///first free entry in slot map
        std::vector<slot_map_t>	d_slot_map;  ///map pointing to entries in chunks
        std::vector<VeIndex>    d_hash_map;     ///hashed value points to slot map

        VeSlotMap();
        VeIndex         insert(VeGuid guid, VeTableIndex table_index);
        VeTableIndex    at(VeHandle handle);
        bool            erase(VeHandle handle);
    };


    ///----------------------------------------------------------------------------------
    /// \brief Constructor of VeSlotMap class
    ///----------------------------------------------------------------------------------
    VeSlotMap::VeSlotMap() : d_first_free(VeIndex::NULL()), d_slot_map(), d_hash_map(initial_hash_map_size) {
        d_hash_map.clear();
    };

    ///----------------------------------------------------------------------------------
    /// \brief if no index is given, find the slot map inded through the hash map
    /// \param[in] The GUID of the slot map entry to find
    /// \returns the a 3-tuple containg the indices of prev, the slot map index and the hashmap index
    ///----------------------------------------------------------------------------------
    std::tuple<VeIndex, VeIndex, VeIndex> VeSlotMap::findInHashMap(VeGuid guid) {
        VeIndex hashidx = (decltype(hashidx.value))(std::hash<decltype(guid)>()(guid) % d_hash_map.size()); //use hash map to find it
        VeIndex prev = VeIndex::NULL();

        VeIndex index = d_hash_map[hashidx];
        while (index != VeIndex::NULL() && d_slot_map[index].d_guid != guid) {
            prev = index;
            index = d_slot_map[index].d_next;   //iterate through the hash map list
        }
        if (index == VeIndex::NULL()) return { VeIndex::NULL(),VeIndex::NULL(),VeIndex::NULL() };    //if not found return NULL
        return { prev, index, hashidx };
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase  an entry from the hash map
    /// \param[in] The GUID of the entry
    /// \returns the slot map index of the given guid so it can be erased from the slot map
    ///----------------------------------------------------------------------------------
    VeIndex VeSlotMap::eraseFromHashMap(VeGuid guid) {
        auto [prev, index, hashidx] = findInHashMap(guid);      //get indices from the hash map
        if (index == VeIndex::NULL()) return VeIndex::NULL();    //if not found return NULL

        //it was found, so remove it
        if (prev == VeIndex::NULL()) {                      //it was the first element in the linked list
            d_hash_map[hashidx] = d_slot_map[index].d_next; //let the hash map point to it directly
        }
        else {                                              //it was in the middle of the linked list
            d_slot_map[prev].d_next = d_slot_map[index].d_next; //de-link it
        }

        return index;
    }

    ///----------------------------------------------------------------------------------
    /// \brief Insert a new item into the slot map and the hash map
    /// \param[in] guid The GUID of the new item
    /// \param[in] table_index The table index of the item that is inserted
    /// \returns the fixed slot map index of the new item to be stored in handles and other maps
    ///----------------------------------------------------------------------------------
    VeIndex VeSlotMap::insert(VeGuid guid, VeTableIndex table_index) {
        VeIndex new_slot = d_first_free;   //point to the free slot to use if there is one
        VeIndex hash_index = (decltype(hash_index.value))std::hash<decltype(guid)>()(guid); //hash map index

        if (new_slot != VeIndex::NULL()) {                  //there is a free slot in the slot map
            d_first_free = d_slot_map[new_slot].d_next;     //let first_free point to the next free slot or NULL
            d_slot_map[new_slot] = { guid, table_index, d_hash_map[hash_index] }; //write over free slot
        }
        else { //no free slot -> add a new slot to the vector
            new_slot = VeIndex((decltype(VeIndex::value))d_slot_map.size());        //point to the new slot in vector
            d_slot_map.emplace_back(guid, table_index, d_hash_map[hash_index]);     //create the new slot
        }
        d_hash_map[hash_index] = new_slot;  //let hash map point to the new entry
        return new_slot;                    //return the index of the new entry
    }

    ///----------------------------------------------------------------------------------
    /// \brief Get a table index from the slot map
    /// \param[in] A handle holding the index to the slot map
    /// \returns the table index or NULL
    ///----------------------------------------------------------------------------------
    VeTableIndex VeSlotMap::at(VeHandle handle) {
        VeIndex index = handle.d_index;                                     //slot map index, can be NULL
        if (index == VeIndex::NULL()) {
            index = std::get<1>(findInHashMap(handle.d_guid));              //if NULL find in hash map
            if (index == VeIndex::NULL() ) return VeTableIndex::NULL();     //still not found -> return NULL
        }
        if( !(index.value < d_slot_map.size()) ) return VeTableIndex::NULL();   //if not allowed -> return NULL
        return d_slot_map[index].d_table_index;    //return index to table chunk 
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase an entry from the slot map
    /// \param[in] The handle holding an index to the slot map
    /// \returns true if the entry was found and removed, else false
    ///----------------------------------------------------------------------------------
    bool VeSlotMap::erase(VeHandle handle) {
        VeIndex index = eraseFromHashMap(handle.d_guid);     //erase from hash map and get slot map index

        if (index == VeIndex::NULL() || !(index.value < d_slot_map.size())) 
            return false;                                    //if not found return false

        d_slot_map[index] = { VeGuid::NULL(), VeTableIndex::NULL(), d_first_free }; //put into linked list of free slots
        d_first_free = index;   //first free slot point to the entry
        return true;
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
