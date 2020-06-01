export module VVE:VeMap;

import std.core;
import :VeTypes;
import :VeMemory;
#include "VETypes.h"

export namespace vve {

    ///----------------------------------------------------------------------------------
    /// Base map class
    ///----------------------------------------------------------------------------------

    template<typename KeyT, typename ValueT>
    class VeMap {
    protected:
        static const uint32_t initial_hash_map_size = 64;

        struct map_t {
            KeyT     d_key;     ///guid of entry
            ValueT   d_value;   ///points to chunk and in chunk entry
            VeIndex  d_next;    ///next free slot or next slot with same hash index or NULL

            map_t(KeyT key, ValueT value, VeIndex next) : 
                d_key(key), d_value(value), d_next(next) {};
        };

        std::tuple<VeIndex, VeIndex, VeIndex>   findInHashMap(KeyT key);     //if no index is given, find the slot map inded through the hash map
        VeIndex                                 eraseFromHashMap(KeyT key);  //remove an entry from the hash map

    public:
        VeIndex                 d_first_free;   ///first free entry in slot map
        std::vector<map_t>	    d_map;          ///map pointing to entries in chunks or simple hash entries
        std::vector<VeIndex>    d_hash_map;     ///hashed value points to slot map

        VeMap();
        VeIndex insert(KeyT key, ValueT value);
        ValueT  at(KeyT key, VeIndex index = VeIndex::NULL());
        bool    erase(KeyT key);
    };


    ///----------------------------------------------------------------------------------
    /// \brief Constructor of VeMap class
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeMap<KeyT, ValueT>::VeMap() : d_first_free(VeIndex::NULL()), d_map(), d_hash_map(initial_hash_map_size) {
        d_hash_map.clear();
    };

    ///----------------------------------------------------------------------------------
    /// \brief if no index is given, find the slot map inded through the hash map
    /// \param[in] The GUID of the slot map entry to find
    /// \returns the a 3-tuple containg the indices of prev, the slot map index and the hashmap index
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    std::tuple<VeIndex, VeIndex, VeIndex> VeMap<KeyT, ValueT>::findInHashMap(KeyT key) {
        VeIndex hashidx = (decltype(hashidx.value))(std::hash<decltype(key)>()(key) % d_hash_map.size()); //use hash map to find it
        VeIndex prev = VeIndex::NULL();

        VeIndex index = d_hash_map[hashidx];
        while (index != VeIndex::NULL() && d_map[index].d_key != key) {
            prev = index;
            index = d_map[index].d_next;   //iterate through the hash map list
        }
        if (index == VeIndex::NULL()) return { VeIndex::NULL(),VeIndex::NULL(),VeIndex::NULL() };    //if not found return NULL
        return { prev, index, hashidx };
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase  an entry from the hash map
    /// \param[in] The GUID of the entry
    /// \returns the slot map index of the given guid so it can be erased from the slot map
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeIndex VeMap<KeyT, ValueT>::eraseFromHashMap(KeyT key) {
        auto [prev, index, hashidx] = findInHashMap(key);      //get indices from the hash map
        if (index == VeIndex::NULL()) return VeIndex::NULL();    //if not found return NULL

        //it was found, so remove it
        if (prev == VeIndex::NULL()) {                      //it was the first element in the linked list
            d_hash_map[hashidx] = d_map[index].d_next; //let the hash map point to it directly
        }
        else {                                              //it was in the middle of the linked list
            d_map[prev].d_next = d_map[index].d_next; //de-link it
        }
        return index;
    }

    ///----------------------------------------------------------------------------------
    /// \brief Insert a new item into the slot map and the hash map
    /// \param[in] guid The GUID of the new item, its hash is the key of the new item
    /// \param[in] index The value of the new item
    /// \returns the fixed slot map index of the new item to be stored in handles and other maps
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeIndex VeMap<KeyT, ValueT>::insert(KeyT key, ValueT value) {
        VeIndex new_slot = d_first_free;   //point to the free slot to use if there is one
        VeIndex hash_index = (decltype(hash_index.value))std::hash<decltype(key)>()(key); //hash map index

        if (new_slot != VeIndex::NULL()) {                              //there is a free slot in the slot map
            d_first_free = d_map[new_slot].d_next;                      //let first_free point to the next free slot or NULL
            d_map[new_slot] = { key, value, d_hash_map[hash_index] };   //write over free slot
        }
        else { //no free slot -> add a new slot to the vector
            new_slot = d_map.size();                                    //point to the new slot in vector
            d_map.emplace_back(key, value, d_hash_map[hash_index]);     //create the new slot
        }
        d_hash_map[hash_index] = new_slot;  //let hash map point to the new entry
        return new_slot;                    //return the index of the new entry
    }

    ///----------------------------------------------------------------------------------
    /// \brief Get a table index from the slot map
    /// \param[in] A handle holding the index to the slot map
    /// \returns the value index or NULL
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    ValueT VeMap<KeyT, ValueT>::at(KeyT key, VeIndex index) {
        if (index == VeIndex::NULL()) {
            index = std::get<1>(findInHashMap(key));                    //if NULL find in hash map
            if (index == VeIndex::NULL() ) return ValueT::NULL();        //still not found -> return NULL
        }
        if( !(index.value < d_map.size()) ) return ValueT::NULL();      //if not allowed -> return NULL
        return d_map[index].d_value;                                    //return value of the map
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase an entry from the slot map
    /// \param[in] The handle holding an index to the slot map
    /// \returns true if the entry was found and removed, else false
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    bool VeMap<KeyT, ValueT>::erase(KeyT key) {
        VeIndex index = eraseFromHashMap(key);     //erase from hash map and get map index

        if (index == VeIndex::NULL() || !(index.value < d_map.size()))
            return false;                                    //if not found return false

        d_map[index] = { KeyT::NULL(), ValueT::NULL(), d_first_free }; //put into linked list of free slots
        d_first_free = index;   //first free slot point to the entry
        return true;            //it was found so return true
    }


    ///----------------------------------------------------------------------------------
    /// \brief Hashed Slot map
    ///----------------------------------------------------------------------------------

    class VeSlotMap : public VeMap<VeGuid, VeTableIndex> {
    public:
        VeSlotMap() : VeMap<VeGuid, VeTableIndex>() {};
    };


    ///----------------------------------------------------------------------------------
    /// \brief Hash map
    ///----------------------------------------------------------------------------------

    template<int... Is>
    class VeHashMap : public VeMap<std::size_t, VeIndex> {
    public:
        static constexpr auto s_indices = std::make_tuple(Is...);
        VeHashMap() : VeMap<std::size_t, VeIndex>() {};
    };


};
