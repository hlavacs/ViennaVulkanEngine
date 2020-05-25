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

    struct VeSlotMapBase : VeMapBase {
        //----------------------------------------------------------------------------------
        //a chunk index consists of a next free index and an in-chunk index, both packed into one integer

        struct VeChunkIndex {
            VePackedInt<VePackingType> d_next_and_in_chunk_index;

            VeChunkIndex() : d_next_and_in_chunk_index() { setNextIndex(VeNextIndexType::NULL()); setInChunkIndex(VeInChunkIndexType::NULL()); };
            VeChunkIndex(VeChunkIndex& table_index) = default;

            VeNextIndexType		getNextIndex() { return VeNextIndexType(d_next_and_in_chunk_index.getUpper()); };
            void				setNextIndex(VeNextIndexType idx) { d_next_and_in_chunk_index.setUpper(idx); };
            VeInChunkIndexType	getInChunkIndex() { return VeInChunkIndexType(d_next_and_in_chunk_index.getLower()); };
            void				setInChunkIndex(VeInChunkIndexType idx) { d_next_and_in_chunk_index.setLower(idx); };
        };        
        
        struct slot_map_t {
            VeGuid			d_guid;
            VeChunkIndex	d_chunk_index;		//next index and in chunk index combined to save memory
        };
    };

    template<int c_max_size>
    struct VeSlotMap : VeSlotMapBase {
        std::array<slot_map_t, c_max_size>	d_slot_map;
        VeSlotMap() : d_slot_map() {}
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





