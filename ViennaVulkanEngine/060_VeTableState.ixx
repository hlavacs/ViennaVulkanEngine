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
	// Declare VeTableStateIterator

	template<bool Const, typename... Types> class VeTableStateIterator;

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

		using iterator = VeTableStateIterator< false, Typelist<TypesOne...>, Maplist<TypesTwo...> >;
		using const_iterator = VeTableStateIterator< true, Typelist<TypesOne...>, Maplist<TypesTwo...> >;
		using range = std::pair<iterator, iterator>;
		using crange = std::pair<const_iterator, const_iterator>;
		friend iterator;
		friend const_iterator;

	private:
		VeGuid						d_guid;			//current GUID reflecting the state of the table state
		bool						d_read_only;	//if true then iterators are always const
		std::pmr::vector<chunk_ptr>	d_chunks;		//pointers to table chunks
		VeSlotMap					d_slot_map;		//the table slot map
		map_type					d_maps;			//the search maps

		bool			isValid( VeTableIndex table_index );
		VeTableIndex	getTableIndexFromHandle( VeHandle &handle);
		VeHandle		insertGUID(VeGuid guid, TypesOne... args);
		VeHandle		insertGUID(VeGuid guid, std::promise<VeHandle> handle, TypesOne... args);

	public:
		VeTableState(allocator_type alloc = {});
		~VeTableState() = default;

		//-------------------------------------------------------------------------------
		//read operations

		tuple_type	at(VeHandle &handle);
		std::size_t	size();
		VeHandle	handle( VeTableIndex table_index );

		template<int map, typename... Args>
		tuple_type find(Args... args);

		//-------------------------------------------------------------------------------
		//write operations

		VeHandle	insert(TypesOne... args);
		VeHandle	insert(std::promise<VeHandle> prom, TypesOne... args);
		bool		update(VeHandle handle, TypesOne... args);
		bool		erase(VeHandle handle);

		template<int map, typename... Args>
		std::pair<iterator, iterator> equal_range( Args... args );
		void		operator=(const VeTableStateType& rhs);
		void		clear();

		//-------------------------------------------------------------------------------
		//iterate

		iterator begin();
		iterator end();
		const_iterator begin() const;
		const_iterator end() const;

	};


	///----------------------------------------------------------------------------------
	/// \brief Constructor
	/// \param[in] alloc PMR allocator to be used
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableStateType::VeTableState(allocator_type alloc) : 
		d_guid(newGuid()), d_read_only(false), d_chunks(alloc), d_slot_map(), d_maps() {};


	template<typename... TypesOne, typename... TypesTwo>
	bool VeTableStateType::isValid(VeTableIndex table_index) {
		if (table_index == VeTableIndex::NULL() ||
			table_index.d_chunk_index >= d_chunks.size() ||
			table_index.d_chunk_index >= d_chunks[table_index.d_chunk_index]->size() ) {
			return false;
		}
		return true;
	}

	///----------------------------------------------------------------------------------
	/// \brief Return the table index for a given handle from the table slot map
	/// \param[in] handle The handle to be found
	/// \returns the table index that is stored in the slot map 
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableIndex VeTableStateType::getTableIndexFromHandle(VeHandle &handle) {
		VeTableIndex table_index = d_slot_map.find(handle);
		if (!isValid(table_index) ) { return VeTableIndex::NULL(); }
		return table_index;
	}



	//-------------------------------------------------------------------------------
	//read operations

	///----------------------------------------------------------------------------------
	/// \brief Find the data for a given handle
	/// \param[in/out] handle The handle for the data entry. If the entry does not exist then it is changed to NULL()
	/// \returns the data tuple that is stored in the table or std::nullopt if not found
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	typename VeTableStateType::tuple_type VeTableStateType::at(VeHandle &handle) {
		VeTableIndex table_index = getTableIndexFromHandle(handle);
		if (!isValid(table_index)) {
			handle = VeHandle::NULL();
			return tuple_type{}; 
		}
		return d_chunks[table_index.d_chunk_index]->at(table_index.d_in_chunk_index, handle.d_index);
	}

	///----------------------------------------------------------------------------------
	/// \brief Return the number of entries in this table
	/// \returns the number of entries currently stored in this table
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	std::size_t VeTableStateType::size() {
		return d_chunks.size()> 0 ? (d_chunks.size() - 1) * chunk_type::c_max_size + d_chunks[d_chunks.size() - 1]->size() : 0;
	}

	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::handle(VeTableIndex table_index) {
		if (!isValid(table_index)) { return VeHandle::NULL(); }
		VeIndex slot = d_chunks[table_index.d_chunk_index]->slot(table_index.d_in_chunk_index);
		return VeHandle{ d_slot_map.map()[slot].d_key, slot };
	}

	template< typename... TypesOne, typename... TypesTwo>
	template<int i, typename... Args>
	typename VeTableStateType::tuple_type VeTableStateType::find(Args... args) {
		using sub_type = typename decltype(std::get<i>(d_maps))::sub_type;

		sub_type  tup = std::make_tuple(args...);
		auto [first, second] = std::get<i>(d_maps).equal_range(tup);
		return *first;
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
	VeHandle VeTableStateType::insertGUID(VeGuid guid, TypesOne... args ) {
		d_guid = newGuid();
		if(d_chunks.size() == 0) { d_chunks.emplace_back(std::make_unique<chunk_type>());} 
		VeChunkIndex last = (decltype(std::declval<VeIndex>().value))(d_chunks.size() - 1);		//index of last chunk

		if (d_chunks[last]->full()) {												//if its full we need a new chunk
			d_chunks.emplace_back(std::make_unique<chunk_type>());					//create a new chunk
			last = (decltype(std::declval<VeIndex>().value))d_chunks.size() - 1;
		}

		VeTableIndex table_index{ last, VeInChunkIndex::NULL() };		//no in_chunk_index yet
		VeHandle handle{ guid, d_slot_map.insert(guid, table_index) };	//handle of the new item

		table_index.d_in_chunk_index = d_chunks[last]->insert(handle.d_index, args...);	//insert data into the chunk, get in_chunk_index

		d_slot_map.update(handle, table_index);		//update slot map table index with new in_chunk_index

		auto tup = std::make_tuple(args...);
		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //copy tuple from arrays
			std::get<i>(d_maps).insert(tup, handle.d_index);
			});

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
	VeHandle VeTableStateType::insertGUID(VeGuid guid, std::promise<VeHandle> prom, TypesOne... args) {
		VeHandle handle = insertGUID(guid, args...);
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
		return insertGUID(newGuid(), args...);
	}

	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the table
	/// \param[in] prom A promise for the new handle
	/// \param[in] entry The data
	/// \returns a new unique handle describing the entry
	///----------------------------------------------------------------------------------
	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(std::promise<VeHandle> prom, TypesOne... args) {
		return insertGUID(newGuid(), std::move(prom), args...);
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
		if (!isValid(table_index)) { return false; }

		d_guid = newGuid();

		auto old_tup = at(handle);
		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //remove old binding from maps
			std::get<i>(d_maps).erase(old_tup);
			});

		auto new_tup = std::make_tuple(args...);
		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //insert new binding
			std::get<i>(d_maps).update(new_tup, handle.d_index);
			});

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
		if (!isValid(table_index)) { return false; }

		d_guid = newGuid();

		VeChunkIndex last = (decltype(VeChunkIndex::value))(d_chunks.size() - 1);

		VeIndex other = d_chunks[table_index.d_chunk_index.value]->swap( 
			table_index.d_in_chunk_index, *d_chunks[last], (decltype(VeInChunkIndex::value))d_chunks[last]->size() - 1);
		d_slot_map.swap(handle.d_index, other);
						 
		d_chunks[last]->pop_back();
		if (d_chunks[last]->size() == 0) { d_chunks.pop_back(); }

		auto old_tup = at(handle);
		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //remove old binding from maps
			std::get<i>(d_maps).erase(old_tup);
			});

		return true;
	};


	template< typename... TypesOne, typename... TypesTwo>
	template<int i, typename... Args>
	typename VeTableStateType::range VeTableStateType::equal_range(Args... args) {
		auto [first, second] = std::get<i>(d_maps).equal_range(args...);
		return std::make_pair<typename VeTableStateType::iterator, typename VeTableStateType::iterator>	( iterator(this, first ), iterator(this, second));
	}


	///----------------------------------------------------------------------------------
	/// \brief Copy another table state over this state
	/// \param[in] rhs The new state to copy
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::operator=(const VeTableStateType& rhs) {
		if (d_guid == rhs.d_guid) return;

		if (d_chunks.size() != rhs.d_chunks.size()) { 
			d_chunks.clear(); 
			for (auto& chunk : rhs.d_chunks) { d_chunks.emplace_back(std::make_unique<chunk_type>()); }
		}
		for (int i = 0; i < d_chunks.size();++i) {
			if (d_chunks[i]->guid() != rhs.d_chunks[i]->guid()) {
				*d_chunks[i] = *rhs.d_chunks[i];
			}
		}
		d_guid = rhs.d_guid;
		d_slot_map = rhs.d_slot_map;
	}

	///----------------------------------------------------------------------------------
	/// \brief Delete all data from this table state
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::clear() {
		d_guid = newGuid();
		d_chunks.clear();
		d_slot_map.clear();

		static_for<std::size_t, 0, std::tuple_size_v<map_type>>([&, this](auto i) { //copy tuple into the chunk
			std::get<i>(d_maps).clear();
			});
	}

	///----------------------------------------------------------------------------------
	/// \brief Return an iterator for this table state
	/// \returns a non-const iterator
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	typename VeTableStateType::iterator VeTableStateType::begin() {
		if (size() == 0) {
			return iterator(this, VeTableIndex::NULL());
		}
		if (!d_read_only) {
			return iterator(this, VeTableIndex{ 0,0 });
		}
		return end();
	}

	///----------------------------------------------------------------------------------
	/// \brief Return an end iterator for this table state
	/// \returns a non-const end iterator
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	typename VeTableStateType::iterator VeTableStateType::end() {
		return iterator(this, VeTableIndex::NULL());
	}

	///----------------------------------------------------------------------------------
	/// \brief Return a const iterator for this table state
	/// \returns a const iterator
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	typename VeTableStateType::const_iterator VeTableStateType::begin() const {
		if (size() == 0) {
			return iterator(this, VeTableIndex::NULL());
		}
		return const_iterator(this, VeTableIndex{ 0,0 });
	}

	///----------------------------------------------------------------------------------
	/// \brief Return an end const iterator for this table state
	/// \returns a const end iterator
	///----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	typename VeTableStateType::const_iterator VeTableStateType::end() const {
		return const_iterator(this, VeTableIndex::NULL());
	}



	///----------------------------------------------------------------------------------
	/// Iterator
	///----------------------------------------------------------------------------------
	
	#define VeTableStateIteratorType VeTableStateIterator< Const, Typelist<TypesOne...>, Maplist<TypesTwo...> >

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	class VeTableStateIteratorType {
		using table_state_other = typename VeTableStateIterator<!Const, Typelist<TypesOne...>, Maplist<TypesTwo...>>;
		using map_iterator = map_iterator<VeHash, VeIndex, Const>;
		friend class table_state_other;
		friend class VeTableStateType;

		VeTableStateType*	d_table_state;
		VeTableIndex		d_table_index;
		map_iterator		d_map_it;

	public:
		using difference_type = std::ptrdiff_t;     // Member typedefs required by std::iterator_traits
		using value_type = typename VeTableStateType::tuple_type;
		using tuple_type = value_type;
		using pointer = std::conditional_t<Const, const VeHandle*, VeHandle*>;
		using reference = std::conditional_t<Const, const VeHandle&, VeHandle&>;
		using iterator_category = std::forward_iterator_tag;

		explicit VeTableStateIterator(VeTableStateType* table_state, VeTableIndex table_index = VeIndex::NULL());
		explicit VeTableStateIterator(VeTableStateType* table_state, map_iterator &map_it);

		void operator*(value_type& arg);
		value_type operator*() const;
		VeHandle operator*(int) const;
		template<int N>
		auto& operator*() const;
		auto& operator++();
		auto operator++(int);

		template<bool R>
		bool operator==(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const;

		template<bool R>
		bool operator!=(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...>>& rhs) const;

		operator VeTableStateIterator<false, Typelist<TypesOne...>, Maplist<TypesTwo...>>() const;
	};

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeTableStateIteratorType::VeTableStateIterator(VeTableStateType* table_state, VeTableIndex table_index)
		: d_table_state(table_state), d_table_index(table_index), d_map_it() {};

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeTableStateIteratorType::VeTableStateIterator(VeTableStateType* table_state, typename VeTableStateIteratorType::map_iterator& map_it ) 
		: d_table_state(table_state), d_table_index(VeTableIndex::NULL()), d_map_it(map_it) {};

	///----------------------------------------------------------------------------------
	/// read data
	template< bool Const, typename... TypesOne, typename... TypesTwo>
	typename VeTableStateIteratorType::value_type VeTableStateIteratorType::operator*() const {
		return d_table_state->d_chunks[d_table_index.d_chunk_index.value]->at(d_table_index.d_in_chunk_index);
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	template<int i>
	auto& VeTableStateIteratorType::operator*() const {
		auto data = d_table_state->d_chunks[d_table_index.d_chunk_index.value]->data();
		return std::get<i>(*data)[d_table_index.d_in_chunk_index.value];
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateIteratorType::operator*(int i) const {
		return d_table_state->handle(d_table_index);
	}


	///----------------------------------------------------------------------------------
	/// write data
	template< bool Const, typename... TypesOne, typename... TypesTwo>
	void VeTableStateIteratorType::operator*(typename VeTableStateIteratorType::value_type& args) {
		auto& data = d_table_state->d_chunks[d_table_index.d_chunk_index.value].data();
		static_for<std::size_t, 0, std::tuple_size_v<value_type>>([&, this](auto i) { //copy tuple from arrays
			std::get<i>(data)[d_table_index.d_in_chunk_index] = std::get<i>(args);
			});
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	auto& VeTableStateIteratorType::operator++() {
		if (d_table_state == nullptr || d_table_index == VeTableIndex::NULL()) { return *this; }
		if (d_table_index.d_chunk_index >=d_table_state->d_chunks.size() ) { d_table_index = VeTableIndex::NULL(); return *this; }

		++d_table_index.d_in_chunk_index;
		if (d_table_index.d_in_chunk_index >= d_table_state->d_chunks[d_table_index.d_chunk_index]->size()) { 
			++d_table_index.d_chunk_index;

			if (d_table_index.d_chunk_index < d_table_state->d_chunks.size()) {
				d_table_index.d_in_chunk_index = 0;
				return *this; 
			}

			d_table_index = VeTableIndex::NULL();
			return *this; 
		}

		return *this;
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	auto VeTableStateIteratorType::operator++(int) {
		auto result = *this;
		++* this;
		return result;
	}

	template<bool Const, typename... TypesOne, typename... TypesTwo>
	template<bool R>
	bool VeTableStateIteratorType::operator==(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const {
		if (d_map_it.d_hash_map == nullptr) {
			return	d_table_state == rhs.d_table_state &&
					d_table_index.d_chunk_index == rhs.d_table_index.d_chunk_index &&
					d_table_index.d_in_chunk_index == rhs.d_table_index.d_in_chunk_index;
		};
		
		return d_table_state == rhs.d_table_state && d_map_it == rhs.d_map_it;
	}

	template<bool Const, typename... TypesOne, typename... TypesTwo>
	template<bool R>
	bool VeTableStateIteratorType::operator!=(const VeTableStateIterator<R, Typelist<TypesOne...>, Maplist<TypesTwo...> >& rhs) const {
		return !( *this == rhs );
	}

	template< bool Const, typename... TypesOne, typename... TypesTwo>
	VeTableStateIteratorType::operator VeTableStateIterator<false, Typelist<TypesOne...>, Maplist<TypesTwo...>>() const {
		return VeTableStateIterator<true, Typelist<TypesOne...>, Maplist<TypesTwo...>>{d_table_state, d_table_index};
	}
	

};


