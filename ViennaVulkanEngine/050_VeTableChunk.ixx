export module VVE:VETableChunk;

import std.core;
import std.memory;

import :VEUtil;
import :VETypes;
import :VEMap;
import :VEMemory;

export namespace vve {


	///----------------------------------------------------------------------------------
	/// \brief table states are cut into chunks of equal size
	///----------------------------------------------------------------------------------

	template<int Size, typename... Args>
	class VeTableChunk {
	public:
		static const uint32_t data_size = (sizeof(Args) + ...) + sizeof(VeIndex); //d_data + slot_map_array
		static const uint32_t manage_size = sizeof(VeGuid) + sizeof(uint32_t) + sizeof(VeChunkIndex);
		static const uint32_t c_max_size = (Size - manage_size) / data_size;

	private:
		using data_arrays = std::tuple<std::array<Args, c_max_size>...>;	//tuple of arrays
		using tuple_type = std::tuple<Args...>;								//basic tuple type
		using slot_map_array = std::array<VeIndex, c_max_size>;				//index of slot map

		VeGuid			d_guid;				//current GUID reflecting the state of the chunk
		VeChunkIndex	d_chunk_index;		///index of this chunk in the chunks vector of the table state
		uint32_t		d_size;				///number of array slots used currently
		data_arrays		d_data;				///arrays of data items
		slot_map_array	d_slot_map_index;	///index of the slot map entry that points to this slot

	public:
		VeTableChunk();
		VeTableChunk( VeChunkIndex chunk_index );
		VeTableChunk( const VeTableChunk &) = delete;
		~VeTableChunk() = default;

		VeInChunkIndex	insert(VeIndex slot_map_index, Args... args);
		bool			update(VeInChunkIndex in_chunk_index, Args... args);
		bool			update(VeIndex slot_map_index, VeInChunkIndex in_chunk_index, Args... args);
		tuple_type		at(VeInChunkIndex in_chunk_index);
		tuple_type		at(VeInChunkIndex in_chunk_index, VeIndex& slot_map_index);
		void			pop_back();
		auto			data();
		bool			full();
		VeGuid			guid();
		size_t			size();
		size_t			capacity();
		VeIndex			swap(VeInChunkIndex index, VeTableChunk<Size, Args...>& other_chunk, VeInChunkIndex other_index);
		VeIndex			slot(VeInChunkIndex);
		void			operator=(const VeTableChunk<Size, Args...>& rhs);

	};

	///----------------------------------------------------------------------------------
	/// \brief Default Constructor
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeTableChunk<Size, Args...>::VeTableChunk() : d_size(0), d_chunk_index(VeChunkIndex::NULL()) {}

	///----------------------------------------------------------------------------------
	// \brief Constructor with chunk index
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeTableChunk<Size, Args...>::VeTableChunk(VeChunkIndex chunk_index) :
		d_guid(newGuid()), d_size(0), d_chunk_index(chunk_index) {}
	
	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the chunk
	/// \param[in] slot_map_index The index of the slot map that points to this entry
	/// \param[in] args The data that is to be added to this chunk
	/// \returns the in chunk index of the new entry in this chunk 
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeInChunkIndex VeTableChunk<Size, Args...>::insert(VeIndex slot_map_index, Args... args) {
		/*auto f = [&, this]<int i, typename T, typename... Bs>(auto& self, T t, Bs... bs) {
			std::get<i>(this->d_data)[this->d_size] = t; 
			if constexpr (sizeof... (Bs) > 0) {
				self.template operator() < i + 1, Bs... > (self, bs...);
			}
		};
		f.template operator() <0>(f, args...);*/

		auto f = [&, this]<std::size_t... Idx, typename... Bs>(std::index_sequence<Idx...>, Bs... bs) {
			std::initializer_list<int>{ (std::get<Idx>(this->d_data)[this->d_size] = bs, 0 ) ...};
		};
		f(std::make_index_sequence<sizeof...(Args)>{}, args...);

		d_slot_map_index[d_size] = slot_map_index;		//index of the slot map that points to this entry
		return VeInChunkIndex(d_size++);				//increase size and return old size as in chunk index
	}

	///----------------------------------------------------------------------------------
	/// \brief Update an existing entry of this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays
	/// \param[in] args The data that is to be updated in this chunk
	/// \returns whether the operation was successful
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	bool VeTableChunk<Size, Args...>::update(VeInChunkIndex in_chunk_index, Args... args) {
		return update(VeIndex::NULL(), in_chunk_index, args...);
	}

	///----------------------------------------------------------------------------------
	/// \brief Update an existing entry of this chunk
	/// \param[in] slot_map_index New index from the slot map that points to this entry
	/// \param[in] in_chunk_index Index of the entry in the data arrays
	/// \param[in] args The data that is to be updated in this chunk
	/// \returns whether the operation was successful
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	bool VeTableChunk<Size, Args...>::update(VeIndex slot_map_index, VeInChunkIndex in_chunk_index, Args... args) {
		if (!(in_chunk_index.value < d_size)) return false;

		auto f = [&, this]<std::size_t... Idx, typename... Bs>(std::index_sequence<Idx...>, Bs... bs) {
			std::initializer_list<int>{ (std::get<Idx>(this->d_data)[in_chunk_index] = bs, 0) ...};
		};
		f(std::make_index_sequence<sizeof...(Args)>{}, args...);

		if (slot_map_index != VeIndex::NULL()) { d_slot_map_index[in_chunk_index] = slot_map_index; }
		return true;
	}


	///----------------------------------------------------------------------------------
	/// \brief Retrieve the data at a certain index from this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays to be retrieved
	/// \param[out] slot_map_index The slot map index or NULL if the item was not found
	/// \returns the data as tuple or an empty tuple
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	std::tuple<Args...> VeTableChunk<Size, Args...>::at(VeInChunkIndex in_chunk_index) {
		VeIndex idx;
		return at(in_chunk_index, idx);
	}

	template<int Size, typename... Args>
	std::tuple<Args...> VeTableChunk<Size, Args...>::at(VeInChunkIndex in_chunk_index, VeIndex& slot_map_index) {
		tuple_type tuple;
		slot_map_index = VeIndex::NULL();

		if ( in_chunk_index < d_size) {
			static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) { //copy tuple from arrays
				std::get<i>(tuple) = std::get<i>(d_data)[in_chunk_index];
			});
			slot_map_index = d_slot_map_index[in_chunk_index];
		}
		return tuple;
	}

	///----------------------------------------------------------------------------------
	/// \brief Erase an entry at a certain index from this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays to be erased
	/// \returns the slot map index of the entry thas was moved to the empty entry to fill the gap
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	void VeTableChunk<Size, Args...>::pop_back() {
		if (d_size == 0) return;
		--d_size;									//reduce size by 1
	}

	///----------------------------------------------------------------------------------
	/// \brief Returns whether the chunk is full or whether it can hold more items
	/// \returns true if chunk is full, else false
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	bool VeTableChunk<Size, Args...>::full() {
		return d_size >= c_max_size;
	}

	///----------------------------------------------------------------------------------
	/// \brief Get a pointer to the raw chunk data
	/// \returns a pointer to the tuple containing the arrays of data items of this chunk
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	auto VeTableChunk<Size, Args...>::data() {
		return &d_data;
	}

	///----------------------------------------------------------------------------------
	/// \brief Get the current GUID of the chunk
	/// \returns the current GUID reflecting the chunk state
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeGuid VeTableChunk<Size, Args...>::guid() {
		return d_guid;
	}

	///----------------------------------------------------------------------------------
	/// \brief Get the number of entries in the chunk
	/// \returns the number of entries in this chunk
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	std::size_t VeTableChunk<Size, Args...>::size() {
		return d_size;
	}

	///----------------------------------------------------------------------------------
	/// \brief Get the capacity if this chunk type
	/// \returns the number of entries this chunk type can store
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	std::size_t VeTableChunk<Size, Args...>::capacity() {
		return c_max_size;
	}

	///----------------------------------------------------------------------------------
	/// \brief Swap an entry of this chunk with another entry, possibly in another chunk
	/// Can be done when erasing an entry from the table. Then first the entry is swapped with the last
	/// entry of the last chunk, then this last entry is simply popped away.
	/// \returns slot map index of the second entry, so we can swap in the slot map also
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeIndex VeTableChunk<Size, Args...>::swap(VeInChunkIndex in_chunk_index, VeTableChunk<Size, Args...> &other_chunk, VeInChunkIndex other_index) {
		if (in_chunk_index < d_size && other_index < other_chunk.d_size ) {
			static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) { //copy tuple from arrays
				std::swap( std::get<i>(d_data)[in_chunk_index], std::get<i>(other_chunk.d_data)[other_index] );
				});

			std::swap( d_slot_map_index[in_chunk_index], other_chunk.d_slot_map_index[other_index] );
		}
		return d_slot_map_index[in_chunk_index];
	}

	///----------------------------------------------------------------------------------
	/// \brief Get the slot map index of a given entry
	/// \returns the slot map index of an entry
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	VeIndex	VeTableChunk<Size, Args...>::slot(VeInChunkIndex in_chunk_index) {
		if (!(in_chunk_index < d_slot_map_index.size())) { return VeIndex::NULL(); }
		return d_slot_map_index[in_chunk_index];
	}

	///----------------------------------------------------------------------------------
	/// \brief Copy a chunk over this chunk
	/// \param[in] the chunk to be copied
	///----------------------------------------------------------------------------------
	template<int Size, typename... Args>
	void VeTableChunk<Size, Args...>::operator=(VeTableChunk<Size, Args...> const& rhs) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) { //copy tuple from arrays
			std::get<i>(d_data) = std::get<i>(rhs.d_data);
			});
		d_slot_map_index = rhs.d_slot_map_index;
		d_size = rhs.d_size;
		d_guid = rhs.d_guid;
	}

};

