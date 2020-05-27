export module VVE:VeTableChunk;

import std.core;
import :VeTypes;
import :VeMap;
import :VeMemory;

namespace vve {
	template <typename T, T Begin, class Func, T ...Is>
	constexpr void static_for_impl(Func&& f, std::integer_sequence<T, Is...>) {
		(f(std::integral_constant<T, Begin + Is>{ }), ...);
	}

	template <typename T, T Begin, T End, class Func >
	constexpr void static_for(Func&& f) {
		static_for_impl<T, Begin>(std::forward<Func>(f), std::make_integer_sequence<T, End - Begin>{ });
	}
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

		++d_size;
		return VeHandle{ guid, {d_chunk_index, VeInChunkIndex(d_size - 1)} };
	}

	template<typename... Args>
	VeHandle VeTableChunk<Args...>::insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle) {
		static_for<std::size_t, 0, std::tuple_size_v<tuple_type>>([&, this](auto i) {
			std::get<i>(d_data)[d_size] = std::get<i>(entry);
		});

		auto ret =  VeHandle{ guid, {d_chunk_index, d_size} };
		*handle = ret;
		++d_size;
		return ret;
	}

	template<typename... Args>
	std::optional<std::tuple<Args...>> VeTableChunk<Args...>::find(VeHandle handle) {
		if (handle.d_table_index.d_in_chunk_index < d_size) {
			tuple_type tuple;
			//std::make_integer_sequence<Is...>(std::get<Is>(tuple) = std::get<Is>(d_data[handle.d_table_index.d_in_chunk_index]), 0);
			//std::apply([]() {std::get<Is>(tuple) = std::get<Is>(d_data[handle.d_table_index.d_in_chunk_index]); }, Is...);

			return std::optional<tuple_type>(tuple);
		}
		return std::nullopt;
	}

	template<typename... Args>
	void VeTableChunk<Args...>::erase(VeHandle handle) {
		if (handle.d_table_index.d_in_chunk_index < d_size - 1) {
			//std::make_integer_sequence<Is...>(std::get<Is>(tuple) = std::get<Is>(d_data[handle.d_table_index.d_in_chunk_index]), 0);

		}


	}


};

