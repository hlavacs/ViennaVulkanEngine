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
	// Delare VeTable to be friend for calling private function
	template< typename... Types> class VeTable;

	//----------------------------------------------------------------------------------
	// Specialization of VeTable
	template< typename... TypesOne, typename... TypesTwo>
	class VeTable<Typelist<TypesOne... >, Maplist<TypesTwo...>>;

	//----------------------------------------------------------------------------------
	// Delare VeTableState
	template< typename... Types> class VeTableState;

	#define VeTableStateType VeTableState< Typelist < TypesOne... >, Maplist < TypesTwo... > >

	//----------------------------------------------------------------------------------
	// Specialization of VeTableState
	template< typename... TypesOne, typename... TypesTwo>
	class VeTableStateType {
		friend class VeTable < Typelist<TypesOne... >, Maplist<TypesTwo...> >;

	public:
		using tuple_type = std::tuple<TypesOne...>;
		using chunk_type = VeTableChunk<TypesOne...>;
		static_assert(sizeof(chunk_type) <= VE_TABLE_CHUNK_SIZE);
		using chunk_ptr  = std::unique_ptr<chunk_type>;
		using map_type = decltype(TupleOfInstances<tuple_type, TypesTwo...>());
		using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

	private:
		std::pmr::vector<chunk_ptr>	d_chunks;	//pointers to table chunks
		VeSlotMap	d_slot_map;					//the table slot map
		map_type	d_maps;						//the search maps

		VeTableIndex getTableIndexFromHandle( VeHandle &handle);
		VeHandle	insert(VeGuid guid, TypesOne... args);
		VeHandle	insert(VeGuid guid, std::promise<VeHandle> handle, TypesOne... args);

	public:
		VeTableState(allocator_type alloc = {});
		~VeTableState() = default;

		//-------------------------------------------------------------------------------
		//read operations

		std::optional<tuple_type>	find(VeHandle handle);
		std::size_t					size();

		//-------------------------------------------------------------------------------
		//write operations

		VeHandle	insert(TypesOne... args);
		VeHandle	insert(std::promise<VeHandle> prom, TypesOne... args);
		bool		update(VeHandle handle, TypesOne... args);
		bool		erase(VeHandle handle);
		void		operator=(const VeTableStateType& rhs);
		void		clear();
	};


	///----------------------------------------------------------------------------------
	/// \brief Constructor
	/// \param[in] alloc PMR allocator to be used
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableStateType::VeTableState(allocator_type alloc) : d_chunks(alloc), d_slot_map(), d_maps() {
		d_chunks.emplace_back(std::make_unique<chunk_type>());	//create one chunk
	};

	///----------------------------------------------------------------------------------
	/// \brief Return the table index for a given handle from the table slot map
	/// \param[in] handle The handle to be found
	/// \returns the table index that is stored in the slot map 
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableIndex VeTableStateType::getTableIndexFromHandle(VeHandle &handle) {
		VeTableIndex table_index = d_slot_map.find(handle);

		if (table_index == VeTableIndex::NULL() ||
			!(table_index.d_chunk_index < d_chunks.size()) ||
			!d_chunks[table_index.d_chunk_index]) {
			return VeTableIndex::NULL();
		}
		return table_index;
	}


	//-------------------------------------------------------------------------------
	//read operations

	///----------------------------------------------------------------------------------
	/// \brief Find the data for a given handle
	/// \param[in] handle The handle for the data entry
	/// \returns the data tuple that is stored in the table or std::nullopt if not found
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	std::optional<std::tuple<TypesOne...>> VeTableStateType::find(VeHandle handle) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if( table_index == VeTableIndex::NULL() )  { return std::nullopt; }
		VeIndex slot_map_index;
		return d_chunks[table_index.d_chunk_index]->at(table_index.d_in_chunk_index, slot_map_index);
	}

	///----------------------------------------------------------------------------------
	/// \brief Return the number of entries in this table
	/// \returns the number of entries currently stored in this table
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	std::size_t VeTableStateType::size() {
		return d_chunks.size()> 0 ? (d_chunks.size() - 1) * chunk_type::c_max_size + d_chunks[d_chunks.size() - 1]->size() : 0;
	}

	//-------------------------------------------------------------------------------
	//write operations

	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the table
	/// \param[in] guid The GUID of this new entry
	/// \param[in] args The new data
	/// \returns a new unique handle describing the entry
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(VeGuid guid, TypesOne... args ) {

		VeChunkIndex last = (decltype(std::declval<VeIndex>().value))(d_chunks.size() - 1);		//index of last chunk
		if (d_chunks[last]->full()) {												//if its full we need a new chunk
			d_chunks.emplace_back(std::make_unique<chunk_type>());					//create a new chunk
			last = (decltype(std::declval<VeIndex>().value))d_chunks.size() - 1;
		}

		VeTableIndex table_index{ last, VeInChunkIndex::NULL() };		//no in_chunk_index yet
		VeHandle handle{ guid, d_slot_map.insert(guid, table_index) };	//handle of the new item

		table_index.d_in_chunk_index = d_chunks[last]->
			insert(handle.d_index, args...);				//insert data into the chunk, get in_chunk_index

		d_slot_map.update(handle, table_index);				//update slot map table index with new in_chunk_index

		//insert into the search maps
		return handle;

	}

	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the table
	/// \param[in] guid The GUID of this new entry
	/// \param[in] prom A promise for the new handle
	/// \param[in] args The data
	/// \returns a new unique handle describing the entry
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(VeGuid guid, std::promise<VeHandle> prom, TypesOne... args) {
		VeHandle handle = insert(guid, args...);
		prom.set_value(handle);
		return handle;
	}

	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the table
	/// \param[in] args The data
	/// \returns a new unique handle describing the entry
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(TypesOne... args) {
		return insert(newGuid(), args...);
	}

	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the table
	/// \param[in] prom A promise for the new handle
	/// \param[in] entry The data
	/// \returns a new unique handle describing the entry
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(std::promise<VeHandle> prom, TypesOne... args) {
		return insert(newGuid(), std::move(prom), args...);
	}

	///----------------------------------------------------------------------------------
	/// \brief Update an existing entry with new data
	/// \param[in] handle The handle describing the entry
	/// \param[in] args A data containing the new data
	/// \returns true if the entry was updated, else false
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	bool VeTableStateType::update(VeHandle handle, TypesOne... args) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if (table_index == VeTableIndex::NULL()) { return false; }
		return d_chunks[table_index.d_chunk_index]->update(table_index.d_in_chunk_index, args...);
	}

	///----------------------------------------------------------------------------------
	/// \brief Erase an existing entry from the table
	/// \param[in] handle The handle describing the entry
	/// \returns true if the entry was erased, else false
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	bool VeTableStateType::erase(VeHandle handle) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if (table_index == VeTableIndex::NULL()) { return false; }
		VeChunkIndex last = d_chunks.size() - 1;
		d_slot_map.swap(handle.d_index, d_chunks[table_index.d_chunk_index]->swap(table_index, d_chunks[last], d_chunks[last].size() - 1));
		d_chunks[last]->pop_back();
		if (d_chunks[last].size() == 0) { d_chunks.pop_back(); }
	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::operator=(const VeTableStateType& rhs) {

	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::clear() {
		d_chunks.clear();
		d_slot_map.clear();

		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //copy tuple into the chunk
			std::get<i>(d_maps).clear();
			});
	}


	///----------------------------------------------------------------------------------
	/// Iterator
	///----------------------------------------------------------------------------------
	
	//----------------------------------------------------------------------------------
	// Declare VeTableStateIterator
	template<bool Const, typename... Types> class VeTableStateIterator;

	#define VeTableStateIteratorType VeTableStateIterator< Const, Typelist<TypesOne...>, Maplist<TypesTwo...> >

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	class VeTableStateIteratorType {
		using table_state_other = typename VeTableStateIterator<!Const, Typelist<TypesOne...>, Maplist<TypesTwo...>>;
		friend class table_state_other;
		friend class VeTableStateType;

		VeTableStateType* d_table_state;
		VeTableIndex      d_table_index;
	public:
		using difference_type = std::ptrdiff_t;     // Member typedefs required by std::iterator_traits
		using value_type = VeHandle;
		using pointer = std::conditional_t<Const, const VeHandle*, VeHandle*>;
		using reference = std::conditional_t<Const, const VeHandle&, VeHandle&>;
		using iterator_category = std::forward_iterator_tag;

		explicit VeTableStateIterator(VeTableStateType *table_state, VeTableIndex table_index = VeIndex::NULL());
		reference operator*() const;
		auto& operator++();
		auto operator++(int);

		//template<bool R, typename... TypesOne, typename... TypesTwo>
		//bool operator==(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const;

		//template<bool R, typename... TypesOne, typename... TypesTwo>
		//bool operator!=(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...>>& rhs) const;

		operator VeTableStateIterator<false, Typelist<TypesOne...>, Maplist<TypesTwo...>>() const;
	};

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeTableStateIteratorType::VeTableStateIterator(VeTableStateType* table_state, VeTableIndex table_index)
		: d_table_state(table_state), d_table_index(table_index) {};

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	typename VeTableStateIteratorType::reference VeTableStateIteratorType::operator*() const {
		return 0;
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	auto& VeTableStateIteratorType::operator++() {
		return *this;
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	auto VeTableStateIteratorType::operator++(int) {
		auto result = *this;
		++* this;
		return result;
	}

	/*template<bool Const, typename... TypesOne, typename... TypesTwo>
	template<bool R>
	bool VeTableStateIteratorType::operator==(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const {
		return true;
	}

	template<bool Const, typename... TypesOne, typename... TypesTwo>
	template<bool R>
	bool VeTableStateIteratorType::operator!=(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const {
		return true;
	}*/

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeTableStateIteratorType::operator VeTableStateIterator<false, Typelist<TypesOne...>, Maplist<TypesTwo...>>() const {
		return VeTableStateIterator<true, Typelist<TypesOne...>, Maplist<TypesTwo...>>{d_table_state, d_table_index};
	}
	

};


