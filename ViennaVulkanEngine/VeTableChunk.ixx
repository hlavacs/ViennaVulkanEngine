export module VVE:VeTableChunk;

import std.core;
import :VeTypes;
import :VeUtil;
import :VeMap;
import :VeMemory;

export namespace vve {

	const uint32_t VE_TABLE_CHUNK_SIZE = 1 << 14;

	///----------------------------------------------------------------------------------
	/// \brief table states are cut into chunks of equal size
	///----------------------------------------------------------------------------------

	template<typename... Args>
	class VeTableChunk {
		static const uint32_t data_size = (sizeof(Args) + ...) + sizeof(VeIndex);
		static const uint32_t manage_size = sizeof(uint32_t) + sizeof(VeChunkIndex);
		static const uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - manage_size) / data_size;

		using data_arrays = std::tuple<std::array<Args, c_max_size>...>;
		using tuple_type = std::tuple<Args...>;
		using slot_map_array = std::array<VeIndex, c_max_size>;
		using Is = std::index_sequence_for<Args...>;

		data_arrays		d_data;				///arrays of data items
		uint32_t		d_size;				///number of array slots used currently
		VeChunkIndex	d_chunk_index;		///index of this chunk in the vector of the table state
		slot_map_array	d_slot_map_index;	///index of the slot map entry that points to this slot

	public:
		VeTableChunk();
		VeTableChunk( VeChunkIndex chunk_index );
		VeTableChunk( const VeTableChunk &) = delete;
		~VeTableChunk() = default;

		VeInChunkIndex				insert(tuple_type entry, VeIndex slot_index);
		bool						update(VeInChunkIndex in_chunk_index, tuple_type entry);
		std::optional<tuple_type>	at(VeInChunkIndex in_chunk_index);
		VeIndex						erase(VeInChunkIndex in_chunk_index);
		auto						data();
	};

	///----------------------------------------------------------------------------------
	/// \brief Constructor
	///----------------------------------------------------------------------------------
	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk() : d_size(0), d_chunk_index(0) {
	}

	///----------------------------------------------------------------------------------
	// \brief Constructor
	///----------------------------------------------------------------------------------
	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk(VeChunkIndex chunk_index) : d_size(0), d_chunk_index(chunk_index) {
	}
	
	///----------------------------------------------------------------------------------
	/// \brief Insert a new entry into the chunk
	/// \param[in] entry The tuple that is to be added to this chunk
	/// \param[in] slot_index Index of the table state slot map that points to this entry
	/// \returns the in chunk index of the new entry in this chunk 
	///----------------------------------------------------------------------------------
	template<typename... Args>
	VeInChunkIndex VeTableChunk<Args...>::insert(tuple_type entry, VeIndex slot_index) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>( [&,this](auto i) { 
			std::get<i>(d_data)[d_size] = std::get<i>(entry); 
		});
		d_slot_map_index[d_size] = slot_index;

		return VeInChunkIndex(d_size++);
	}

	///----------------------------------------------------------------------------------
	/// \brief Update an existing entry of this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays
	/// \param[in] entry The tuple that is to be updated in this chunk
	/// \returns whether the operation was successful
	///----------------------------------------------------------------------------------
	template<typename... Args>
	bool VeTableChunk<Args...>::update(VeInChunkIndex in_chunk_index, tuple_type entry) {
		if(!(in_chunk_index.value < d_size)) return false;

		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::get<i>(d_data)[in_chunk_index] = std::get<i>(entry);
		});

		return true;
	}

	///----------------------------------------------------------------------------------
	/// \brief Retrieve the data at a certain index from this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays to be retrieved
	/// \returns the data as tuple or a null value
	///----------------------------------------------------------------------------------
	template<typename... Args>
	std::optional<std::tuple<Args...>> VeTableChunk<Args...>::at(VeInChunkIndex in_chunk_index) {
		if ( in_chunk_index < d_size) {
			tuple_type tuple;
			static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
				std::get<i>(tuple) = std::get<i>(d_data)[in_chunk_index];
			});

			return std::optional<tuple_type>(tuple);
		}
		return std::nullopt;
	}

	///----------------------------------------------------------------------------------
	/// \brief Erase an entry at a certain index from this chunk
	/// \param[in] in_chunk_index Index of the entry in the data arrays to be erased
	/// \returns the slot map index of the entry thas was moved to the empty entry to fill the gap
	///----------------------------------------------------------------------------------
	template<typename... Args>
	VeIndex VeTableChunk<Args...>::erase(VeInChunkIndex in_chunk_index ) {
		if (!(in_chunk_index < d_size)) { return VeIndex::NULL(); }

		--d_size;

		if (in_chunk_index == d_size) return VeIndex::NULL();

		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::swap(	std::get<i>(d_data)[in_chunk_index],
						std::get<i>(d_data)[d_size]);
		});

		std::swap(d_slot_map_index[in_chunk_index], d_slot_map_index[d_size]);
		return d_slot_map_index[in_chunk_index];
	}

	///----------------------------------------------------------------------------------
	/// \brief Get a pointer to the raw chunk data
	/// \returns a pointer to the tuple containing the arrays of data items of this chunk
	///----------------------------------------------------------------------------------
	template<typename... Args>
	auto VeTableChunk<Args...>::data() {
		return &d_data;
	}

};

