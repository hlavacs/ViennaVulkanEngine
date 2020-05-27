export module VVE:VeTableChunk;

import std.core;
import :VeTypes;
import :VeUtil;
import :VeMap;
import :VeMemory;

namespace vve {

};

export namespace vve {

	const uint32_t VE_TABLE_CHUNK_SIZE = 1 << 14;

	//----------------------------------------------------------------------------------
	//table states are cut into chunks of equal size

	template<typename... Args>
	class VeTableChunk {
	public:

		static const uint32_t data_size = (sizeof(Args) + ...) + sizeof(VeIndex32);
		static const uint32_t manage_size = sizeof(uint32_t) + sizeof(VeChunkIndex);
		static const uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - manage_size) / data_size;

		using data_type = std::tuple<std::array<Args, c_max_size>...>;
		using tuple_type = std::tuple<Args...>;
		using slot_map_type = std::array<VeIndex32, c_max_size>;
		using Is = std::index_sequence_for<Args...>;

		data_type		d_data;
		uint32_t		d_size;
		VeChunkIndex	d_chunk_index;
		slot_map_type	d_slot_map_index;

		VeTableChunk();
		VeTableChunk( VeChunkIndex chunk_index );
		VeTableChunk( const VeTableChunk &) = delete;
		~VeTableChunk() = default;

		VeHandle insert(VeGuid guid, tuple_type entry);
		VeHandle insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle);
		std::optional<tuple_type> find(VeHandle handle);
		void erase(VeHandle handle);
	};

	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk() : d_size(0), d_chunk_index(0) {
	}

	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk(VeChunkIndex chunk_index) : d_size(0), d_chunk_index(chunk_index) {
	}
	
	template<typename... Args>
	VeHandle VeTableChunk<Args...>::insert(VeGuid guid, tuple_type entry) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>( [&,this](auto i) { 
			std::get<i>(d_data)[d_size] = std::get<i>(entry); 
		});

		return VeHandle{ guid, {d_chunk_index, VeInChunkIndex(d_size++)} };
	}

	template<typename... Args>
	VeHandle VeTableChunk<Args...>::insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::get<i>(d_data)[d_size] = std::get<i>(entry);
		});

		return *handle = VeHandle{ guid, {d_chunk_index, d_size++} } ;
	}

	template<typename... Args>
	std::optional<std::tuple<Args...>> VeTableChunk<Args...>::find(VeHandle handle) {
		if (handle.d_table_index.d_in_chunk_index < d_size) {
			tuple_type tuple;
			static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
				std::get<i>(tuple) = std::get<i>(d_data)[handle.d_table_index.d_in_chunk_index];
			});

			return std::optional<tuple_type>(tuple);
		}
		return std::nullopt;
	}

	template<typename... Args>
	void VeTableChunk<Args...>::erase(VeHandle handle) {
		VeInChunkIndex idx = handle.d_table_index.d_in_chunk_index;

		if (!((uint32_t)(idx) < d_size)) { static_assert(std::false_type); }

		--d_size;

		if (idx == d_size) return;

		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::swap(	std::get<i>(d_data)[idx],
						std::get<i>(d_data)[d_size]);
		});

		std::swap(d_slot_map_index[idx], d_slot_map_index[d_size]);
	}


};

