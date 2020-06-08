export module VVE:VEMap;

import std.core;
import std.memory;

import :VETypes;
import :VEUtil;
import :VEMemory;
#include "VEHash.h"

export namespace vve {



    ///----------------------------------------------------------------------------------
    /// Base map class
    ///----------------------------------------------------------------------------------

    template<typename KeyT, typename ValueT>
    class VeHashMapBase {
    protected:
        static const uint32_t initial_bucket_size = 64;
        uint64_t m_hash_mask = 64 - 1; //use this to AND away hash index
        using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

        struct map_t {
            KeyT     d_key;     ///guid of entry or hash of the tuples
            ValueT   d_value;   ///points to chunk and in chunk entry
            VeIndex  d_next;    ///next free slot or next slot with same hash index or NULL

            map_t(KeyT key, ValueT value, VeIndex next) : d_key(key), d_value(value), d_next(next) {};
        };

        std::tuple<VeIndex, VeIndex, VeIndex>   findInHashMap(KeyT &key);     //if no index is given, find the slot map index through the hash map
        VeIndex                                 eraseFromHashMap(KeyT &key);  //remove an entry from the hash map
        void                                    increaseBuckets();           //increase the number of buckets and rehash the hash map

        std::size_t                 d_size;         ///number of mappings in the map
        VeIndex                     d_first_free;   ///first free entry in slot map
        std::pmr::vector<map_t>     d_map;          ///map pointing to entries in chunks or simple hash entries
        std::pmr::vector<VeIndex>   d_bucket;       ///hashed value points to slot map

    public:
        VeHashMapBase(allocator_type alloc = {});
        VeIndex     insert( KeyT &key, ValueT value);
        bool        update( KeyT &key, ValueT value, VeIndex index = VeIndex::NULL());
        ValueT      find(   KeyT &key, VeIndex index = VeIndex::NULL());
        bool        erase(  KeyT &key);
        std::size_t size() { return d_size; };
        float       loadFactor() { return (float)d_size / d_bucket.size();  };
    };


    ///----------------------------------------------------------------------------------
    /// \brief Constructor of VeMap class
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeHashMapBase<KeyT, ValueT>::VeHashMapBase(allocator_type alloc) : 
        d_size(0), d_first_free(VeIndex::NULL()), d_map(alloc), d_bucket(initial_bucket_size, {}, alloc) {
        d_bucket.clear();
    };

    ///----------------------------------------------------------------------------------
    /// \brief if no index is given, find the slot map inded through the hash map
    /// \param[in] The GUID of the slot map entry to find
    /// \returns the a 3-tuple containg the indices of prev, the slot map index and the hashmap index
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    std::tuple<VeIndex, VeIndex, VeIndex> VeHashMapBase<KeyT, ValueT>::findInHashMap(KeyT &key) {
        VeIndex hashidx = (decltype(hashidx.value))std::hash<KeyT>()(key) & m_hash_mask; //  % d_bucket.size(); //use hash map to find it
        VeIndex prev = VeIndex::NULL();

        VeIndex index = d_bucket[hashidx];
        while (index != VeIndex::NULL() && d_map[index].d_key != key) {
            prev = index;
            index = d_map[index].d_next;   //iterate through the hash map list
        }
        if (index == VeIndex::NULL()) return { VeIndex::NULL(),VeIndex::NULL(),VeIndex::NULL() };    //if not found return NULL
        return { prev, index, hashidx };
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase an entry from the hash map
    /// \param[in] The key of the entry
    /// \returns the slot map index of the given guid so it can be erased from the slot map
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeIndex VeHashMapBase<KeyT, ValueT>::eraseFromHashMap(KeyT &key) {
        auto [prev, index, hashidx] = findInHashMap(key);      //get indices from the hash map
        if (index == VeIndex::NULL()) return VeIndex::NULL();    //if not found return NULL

        //it was found, so remove it
        if (prev == VeIndex::NULL()) {                      //it was the first element in the linked list
            d_bucket[hashidx] = d_map[index].d_next; //let the hash map point to it directly
        }
        else {                                              //it was in the middle of the linked list
            d_map[prev].d_next = d_map[index].d_next; //de-link it
        }
        return index;
    }

    ///----------------------------------------------------------------------------------
    /// \brief increase the number of buckets and rehash the hash map
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    void VeHashMapBase<KeyT, ValueT>::increaseBuckets() {
        d_bucket.resize(2 * d_bucket.size(), VeIndex::NULL());
        m_hash_mask = 2 * (m_hash_mask + 1) - 1;    //must be power of 2 minus one

        for (uint32_t i = 0; i < d_map.size(); ++i) {
            if (d_map[i].d_key != KeyT::NULL()) {
                VeIndex hash_index = (decltype(hash_index.value))std::hash<KeyT>()(d_map[i].d_key) & m_hash_mask; //% d_bucket.size();
                d_map[i].d_next = d_bucket[hash_index];
                d_bucket[hash_index] = i;
            }
        }
    }

    ///----------------------------------------------------------------------------------
    /// \brief Insert a new item into the hash map
    /// \param[in] key The key of the new item
    /// \param[in] value The value of the new item
    /// \returns the fixed slot map index of the new item to be stored in handles and other maps
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    VeIndex VeHashMapBase<KeyT, ValueT>::insert(KeyT &key, ValueT value) {
        if (loadFactor() > 0.9f) increaseBuckets();

        VeIndex new_slot = d_first_free;   //point to the free slot to use if there is one
        VeIndex hash_index = (decltype(hash_index.value)) (std::hash<KeyT>()(key) & m_hash_mask); //% d_bucket.size(); //hash map index

        if (new_slot != VeIndex::NULL()) {                             //there is a free slot in the slot map
            d_first_free = d_map[new_slot].d_next;                     //let first_free point to the next free slot or NULL
            d_map[new_slot] = { key, value, d_bucket[hash_index] };    //write over free slot
        }
        else { //no free slot -> add a new slot to the vector
            new_slot = (decltype(new_slot.value))d_map.size();         //point to the new slot in vector
            d_map.emplace_back(key, value, d_bucket[hash_index]);      //create the new slot
        }
        ++d_size;                           ///increase size
        d_bucket[hash_index] = new_slot;    //let hash map point to the new entry
        return new_slot;                    //return the index of the new entry
    }

    ///----------------------------------------------------------------------------------
    /// \brief Update the value for a given key
    /// \param[in] key The item key for which the value should be updated
    /// \param[in] value The new value to be saved
    /// \param[in] index Either the shortcut to the item or NULL
    /// \returns true if the value was changed, else false
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    bool VeHashMapBase<KeyT, ValueT>::update(KeyT &key, ValueT value, VeIndex index ) {
        if (index == VeIndex::NULL()) {
            index = std::get<1>(findInHashMap(key));           //if NULL find in hash map
            if (index == VeIndex::NULL()) return false;        //still not found -> return false
        }
        if (!(index.value < d_map.size()) || d_map[index].d_key != key) return false;    //if not allowed -> return false
        d_map[index].d_value = value;      //change the value
        return true;                       //return true
    }

    ///----------------------------------------------------------------------------------
    /// \brief Get a table index from the slot map
    /// \param[in] key The key of the item to be found
    /// \param[in] index Either the shortcut to the item or NULL
    /// \returns the value or NULL
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    ValueT VeHashMapBase<KeyT, ValueT>::find(KeyT &key, VeIndex index) {
        if (index == VeIndex::NULL()) {
            index = std::get<1>(findInHashMap(key));                    //if NULL find in hash map
            if (index == VeIndex::NULL() ) return ValueT::NULL();        //still not found -> return NULL
        }
        if( !(index.value < d_map.size()) || d_map[index].d_key != key ) return ValueT::NULL();      //if not allowed -> return NULL
        return d_map[index].d_value;                                    //return value of the map
    }

    ///----------------------------------------------------------------------------------
    /// \brief Erase an entry from the slot map
    /// \param[in] The handle holding an index to the slot map
    /// \returns true if the entry was found and removed, else false
    ///----------------------------------------------------------------------------------
    template<typename KeyT, typename ValueT>
    bool VeHashMapBase<KeyT, ValueT>::erase(KeyT &key) {
        VeIndex index = eraseFromHashMap(key);     //erase from hash map and get map index

        if (index == VeIndex::NULL() || !(index.value < d_map.size()))
            return false;                                    //if not found return false

        d_map[index] = { KeyT::NULL(), ValueT::NULL(), d_first_free }; //put into linked list of free slots
        d_first_free = index;   //first free slot point to the entry
        --d_size;
        return true;            //it was found so return true
    }


    //-----------------------------------------------------------------------------------


    ///----------------------------------------------------------------------------------
    /// \brief Hashed Slot map
    ///----------------------------------------------------------------------------------

    class VeSlotMap : public VeHashMapBase<VeGuid, VeTableIndex> {
    public:
        using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

        VeSlotMap(allocator_type alloc = {}) : VeHashMapBase<VeGuid, VeTableIndex>(alloc) {};
        auto insert(VeHandle &handle, VeTableIndex table_index);
        auto update(VeHandle &handle, VeTableIndex table_index);
        auto find(VeHandle &handle);
        auto erase(VeHandle &handle);
    };

    ///----------------------------------------------------------------------------------
    /// \brief Overload of the update member function
    /// \param[in] handle The handle holding an index to the slot map
    /// \param[in] table_index The new table_index to be stored in the slot map
    /// \returns true if the entry was found and updated, else false
    ///----------------------------------------------------------------------------------
    auto VeSlotMap::insert(VeHandle &handle, VeTableIndex table_index) {
        return VeHashMapBase<VeGuid, VeTableIndex>::insert(handle.d_guid, table_index );
    }

    ///----------------------------------------------------------------------------------
    /// \brief Overload of the update member function
    /// \param[in] handle The handle holding an index to the slot map
    /// \param[in] table_index The new table_index to be stored in the slot map
    /// \returns true if the entry was found and updated, else false
    ///----------------------------------------------------------------------------------
    auto VeSlotMap::update(VeHandle &handle, VeTableIndex table_index) {
        return VeHashMapBase<VeGuid, VeTableIndex>::update(handle.d_guid, table_index, handle.d_index);
    }

    ///----------------------------------------------------------------------------------
    /// \brief Overload of the at member function
    /// \param[in] handle The handle holding an index to the slot map
    /// \returns the table index for this handle
    ///----------------------------------------------------------------------------------
    auto VeSlotMap::find(VeHandle &handle) {
        return VeHashMapBase<VeGuid, VeTableIndex>::find(handle.d_guid, handle.d_index);
    }

    ///----------------------------------------------------------------------------------
    /// \brief Overload of the erase member function
    /// \param[in] handle The handle holding an index to the slot map
    /// \returns true if the item was erased, else false
    ///----------------------------------------------------------------------------------
    auto VeSlotMap::erase(VeHandle &handle) {
        return VeHashMapBase<VeGuid, VeTableIndex>::erase(handle.d_guid);
    }


    //-----------------------------------------------------------------------------------



    ///----------------------------------------------------------------------------------
    /// \brief Hash map
    ///----------------------------------------------------------------------------------


    template< typename tuple_type, int... Is>
    class VeHashMap : public VeHashMapBase<VeHash, VeIndex> {
    public:
        using allocator_type = std::pmr::polymorphic_allocator<std::byte>;
        using sub_type = std::tuple< std::tuple_element<Is, tuple_type>... >;

        VeHashMap(allocator_type alloc = {}) : VeHashMapBase<VeHash, VeIndex>(alloc) {};
        auto insert(tuple_type &data, VeIndex index);
        auto update(tuple_type &data, VeIndex index);
        auto find(tuple_type &data);
        auto erase(tuple_type &data);
    };

    template< typename tuple_type, int... Is>
    auto VeHashMap<tuple_type, Is...>::insert(tuple_type &data, VeIndex index) {
        return VeHashMapBase<VeHash, VeIndex>::insert( std::hash<sub_type>()(std::make_tuple(std::tuple_element<Is, data> ...)), index );
    }

    template< typename tuple_type, int... Is>
    auto VeHashMap<tuple_type, Is...>::update(tuple_type &data, VeIndex index) {
        return VeHashMapBase<VeHash, VeIndex>::update(std::hash<sub_type>()(std::make_tuple(std::tuple_element<Is, data> ...)), index);
    }

    template< typename tuple_type, int... Is>
    auto VeHashMap<tuple_type, Is...>::find(tuple_type &data) {
        return VeHashMapBase<VeHash, VeIndex>::find(std::hash<sub_type>()(std::make_tuple(std::tuple_element<Is, data> ...)));
    }

    template< typename tuple_type, int... Is>
    auto VeHashMap<tuple_type, Is...>::erase(tuple_type &data) {
        return VeHashMapBase<VeHash, VeIndex>::erase(std::hash<sub_type>()(std::make_tuple(std::tuple_element<Is, data> ...)));
    }

};
