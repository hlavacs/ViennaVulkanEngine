export module VVE:VeTableChunk;

import std.core;
import :VeTypes;
import :VeUtil;
import :VeMap;
import :VeMemory;

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

		VeInChunkIndex				insert(tuple_type entry, VeIndex32 slot_index);
		void						update(tuple_type entry, VeInChunkIndex in_chunk_index);
		std::optional<tuple_type>	at(VeInChunkIndex in_chunk_index);
		void						erase(VeInChunkIndex in_chunk_index);
		auto						data();
	};

	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk() : d_size(0), d_chunk_index(0) {
	}

	template<typename... Args>
	VeTableChunk<Args...>::VeTableChunk(VeChunkIndex chunk_index) : d_size(0), d_chunk_index(chunk_index) {
	}
	
	template<typename... Args>
	VeInChunkIndex VeTableChunk<Args...>::insert(tuple_type entry, VeIndex32 slot_index) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>( [&,this](auto i) { 
			std::get<i>(d_data)[d_size] = std::get<i>(entry); 
		});
		d_slot_map_index[d_size] = slot_index;

		return VeInChunkIndex(d_size++);
	}

	template<typename... Args>
	void VeTableChunk<Args...>::update(tuple_type entry, VeInChunkIndex in_chunk_index) {
		if(!(in_chunk_index.value < d_size)) return;

		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::get<i>(d_data)[in_chunk_index] = std::get<i>(entry);
		});

		return VeInChunkIndex(d_size++);
	}

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

	template<typename... Args>
	void VeTableChunk<Args...>::erase(VeInChunkIndex in_chunk_index ) {
		if (!(in_chunk_index < d_size)) { return; }

		--d_size;

		if (in_chunk_index == d_size) return;

		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::swap(	std::get<i>(d_data)[in_chunk_index],
						std::get<i>(d_data)[d_size]);
		});

		std::swap(d_slot_map_index[in_chunk_index], d_slot_map_index[d_size]);
	}

	template<typename... Args>
	auto VeTableChunk<Args...>::data() {
		return &d_data;
	}

};

