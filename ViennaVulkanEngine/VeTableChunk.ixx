export module VVE:VeTableChunk;

import std.core;
import :VeTypes;
import :VeMap;
import :VeMemory;


export namespace vve {

	const uint32_t VE_TABLE_CHUNK_SIZE = 1 << 14;

	//----------------------------------------------------------------------------------
	//table states are cut into chunks of equal size

	template<typename... Args>
	struct VeTableChunk {

		static const uint32_t data_size = (sizeof(Args) + ...) + sizeof(VeSlotMap<100>) / 100;
		static const uint32_t manage_size = sizeof(std::tuple<uint32_t, VeInChunkIndex>);
		static const uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - manage_size) / data_size;

		using data_type = std::tuple<std::array<Args, c_max_size>...>;
		using tuple_type = std::tuple<Args...>;
		data_type				d_data;

		uint32_t				d_size = 0;
		VeInChunkIndex			d_first_free = VeInChunkIndex(0);

		VeSlotMap<c_max_size>	m_slot_map;

		VeTableChunk() = default;
		~VeTableChunk() = default;

		void clear();
		auto insert(VeGuid guid, tuple_type entry);
		auto insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle);
		auto find(VeHandle handle);
		void erase(VeHandle handle);

	};

};

