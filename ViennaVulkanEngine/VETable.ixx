export module VVE:VeTable;

import std.core;
#include <tuple>

import :VeTypes;

export namespace vve {

	constexpr uint32_t VE_TABLE_CHUNK_SIZE = 1 << 14;


	//----------------------------------------------------------------------------------
	//create tuple of arrays from tuple (experimental)

	template<class Tuple, std::size_t... Is>
	auto ToA_impl(const Tuple& t, std::index_sequence<Is...>) {
		return std::make_tuple(std::array<std::tuple_element<Is, Tuple>::type, 10>{} ...);
	}

	template<class... Args>
	auto ToA(const std::tuple<Args...>& t) {
		const uint32_t VE_TABLE_CHUNK_NUMBER = VE_TABLE_CHUNK_SIZE / sizeof(t);
		return ToA_impl(t, std::index_sequence_for<Args...>{});
	}

	//----------------------------------------------------------------------------------
	//create tuple of arrays from type list

	template<typename... Args>
	struct VeTableToAChunk {
		static constexpr uint32_t tuple_size = (sizeof(Args) + ...);
		static constexpr uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / tuple_size;
		using type = std::tuple<std::array<Args, c_max_size>...>;
		size_t	d_size = 0;
		type	d_tuple_of_array;
		size_t	size() { return d_size;  };
	};

	template<typename... Args>
	class VeTableToA {
		using tuple_data = std::tuple<Args...>;
		static constexpr uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / sizeof(tuple_data);
		using chunk_type = VeTableToAChunk<Args...>;

		std::vector<std::unique_ptr<chunk_type>> m_chunks;		///pointers to table chunks
		std::set<VeChunkIndex32> m_free_chunks;					///chunks that are not full
		std::set<VeChunkIndex32> m_deleted_chunks;				///chunks that do not exist yet

	public:
		VeTableToA() {
			m_chunks.emplace_back(std::make_unique<chunk_type>());
		};

		~VeTableToA() {};



	};


	//----------------------------------------------------------------------------------
	//create array of tuples from type list

	template<typename... Args>
	struct VeTableAoTChunk {
		using tuple_data = std::tuple<Args...>;
		static constexpr uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / sizeof(tuple_data);
		using type = std::array<tuple_data, c_max_size>;
		size_t	d_size = 0;
		type	d_array_of_tuples;
		size_t	size() { return d_size; };
	};


};



