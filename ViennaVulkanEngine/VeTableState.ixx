export module VVE:VETableState;

import std.core;
import std.memory;

import :VETypes;
import :VEUtil;
import :VEMap;
import :VEMemory;
import :VETableChunk;

export namespace vve {

	//----------------------------------------------------------------------------------
	template < int... Is >
	struct Hashlist {

		template<typename tuple_type>
		auto getMap() {
			//using sub_type = std::tuple< std::tuple_element<Is, tuple_type>::type ... >;
			//return std::make_tuple(VeHashMap<tuple_type, Is...>());
			return VeHashMap<tuple_type, Is...>();
		}
	};

	//template < int... Is >
	//struct Sortlist {
	//	template<typename tuple_type>
	//	auto getTuple() {
	//		return std::make_tuple(Is...);
	//	}
	//};

	//----------------------------------------------------------------------------------
	//Turn List of Integers into maps

	template<typename tuple_type, typename T>
	constexpr auto TupleOfMaps_impl() {
		T map;
		return map.getMap<tuple_type>(); //calls Hashlist::getTuple2
	}

	template<typename tuple_type, typename... Ts>
	constexpr auto TupleOfMaps() {
		return std::make_tuple(TupleOfMaps_impl<tuple_type, Ts>()...); //return a tuple of maps
	}


	//----------------------------------------------------------------------------------
	// Delare VeTableState
	template< typename... Types> struct VeTableState;

	#define VeTableStateType VeTableState< Typelist < TypesOne... >, Typelist < TypesTwo... > >

	//----------------------------------------------------------------------------------
	// Specialization of VeTableState
	template< typename... TypesOne, typename... TypesTwo>
	class VeTableStateType {

		using tuple_type = std::tuple<TypesOne...>;
		using chunk_type = VeTableChunk<TypesOne...>;
		static_assert(sizeof(chunk_type) <= VE_TABLE_CHUNK_SIZE);
		using chunk_ptr  = std::unique_ptr<chunk_type>;
		using map_type = decltype(TupleOfMaps<tuple_type, TypesTwo...>());
		using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

		//chunks
		std::pmr::vector<chunk_ptr>	d_chunks;	///pointers to table chunks

		//maps
		VeSlotMap	d_slot_map;		//the table slot map
		map_type	d_maps;			//the search maps

		VeTableIndex getTableIndexFromHandle( VeHandle handle);

	public:

		VeTableState(allocator_type alloc = {});
		~VeTableState() = default;

		//-------------------------------------------------------------------------------
		//read operations

		std::optional<tuple_type> find(VeHandle handle);

		//-------------------------------------------------------------------------------
		//write operations

		VeHandle	insert(VeGuid guid, tuple_type &entry);
		VeHandle	insert(VeGuid guid, tuple_type &entry, std::shared_ptr<VeHandle> handle);
		bool		update(VeHandle handle, tuple_type &entry);
		bool		erase(VeHandle handle);
		void		operator=(const VeTableStateType& rhs);
		void		clear();
	};

	//----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableStateType::VeTableState(allocator_type alloc) : d_chunks(alloc), d_maps() {
		d_chunks.emplace_back(std::make_unique<chunk_type>());	//create one chunk
	};


	template<typename... TypesOne, typename... TypesTwo>
	VeTableIndex VeTableStateType::getTableIndexFromHandle(VeHandle handle) {
		VeTableIndex table_index = d_slot_map.at(handle);

		if (table_index == VeTableIndex::NULL() ||
			!(table_index.d_chunk_index < d_chunks.size()) ||
			!d_chunks[table_index.d_chunk_index]) {
			return VeTableIndex::NULL();
		}
		return table_index;
	}


	//-------------------------------------------------------------------------------
	//read operations

	template< typename... TypesOne, typename... TypesTwo>
	std::optional<std::tuple<TypesOne...>> VeTableStateType::find(VeHandle handle) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if( table_index == VeTableIndex::NULL() )  { return std::nullopt; }
		VeIndex slot_map_index;
		return d_chunks[table_index.d_chunk_index]->at(table_index.d_in_chunk_index, slot_map_index);
	}


	//-------------------------------------------------------------------------------
	//write operations

	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(VeGuid guid, tuple_type &entry) {
		VeIndex last = d_chunks.size() - 1;		//index of last chunk
		if (d_chunks[last].full()) {			//if its full we need a new chunk
			d_chunks.emplace_back(std::make_unique<chunk_type>());	//create a new chunk
			last = d_chunks.size() - 1;
		}

		VeInChunkIndex in_chunk_index = d_chunks[last].insert(entry);	//insert data into the chunk
		return { .d_guid = guid, .d_table_index = { .d_chunk_index = last, .d_in_chunk_index = in_chunk_index} };
	}

	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(VeGuid guid, tuple_type &entry, std::shared_ptr<VeHandle> handle) {
		*handle = insert(guid, entry);
		return *handle;
	}

	template< typename... TypesOne, typename... TypesTwo>
	bool VeTableStateType::update(VeHandle handle, tuple_type &entry) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if (table_index == VeTableIndex::NULL()) { return false; }
		return d_chunks[table_index.d_chunk_index].update( handle, entry);
	}

	template< typename... TypesOne, typename... TypesTwo>
	bool VeTableStateType::erase(VeHandle handle) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if (table_index == VeTableIndex::NULL()) { return false; }

		//swap with last element of the last chunk
		VeTableIndex last_index = { .d_chunk_index = d_chunks.size() - 1, .d_in_chunk_index = d_chunks[d_chunks.size() - 1].size() - 1 };
		VeIndex slot_map_index_last;
		std::tuple<TypesOne...> last_tuple = d_chunks[last_index.d_chunk_index]->at(last_index.d_in_chunk_index, slot_map_index_last);
		VeIndex slot_map_index = d_chunks[table_index.d_chunk_index]->slotMapIndex(table_index.d_in_chunk_index);

		d_chunks[table_index.d_chunk_index]->update(slot_map_index_last, table_index.d_in_chunk_index, last_tuple);
		//d_slot_map[slot_map_index_last] = table_index;



	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::operator=(const VeTableStateType& rhs) {

	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::clear() {
		d_chunks.clear();
		d_slot_map.clear();
		for (auto& map : d_maps) { map.clear(); }
	}

};


