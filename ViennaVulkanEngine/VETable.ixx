export module VVE:VeTable;

import std.core;
import :VeTypes;
import :VeMap;
import :VeMemory;

namespace vve {
	const uint32_t VE_TABLE_CHUNK_SIZE = 1 << 14;

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

};


export namespace vve {

	//----------------------------------------------------------------------------------
	//create tuple of arrays from type list

	struct VeTableChunkBase {
		VeTableChunkBase() = default;
		~VeTableChunkBase() = default;
	};

	template<typename... Args>
	struct VeTableChunk : VeTableChunkBase {

		static const uint32_t tuple_size = sizeof(VeSlotMapBase::slot_map_t) + sizeof(VeInChunkIndexType) + (sizeof(Args) + ...);
		static const uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / tuple_size;
		using data_type = std::tuple<std::array<Args, c_max_size>...>;

		size_t										d_size = 0;
		VeSlotMap<c_max_size>						d_slot_map;
		std::array<VeInChunkIndexType, c_max_size>	d_slot_map_index;
		data_type									d_data;

		VeTableChunk() = default;
		~VeTableChunk() = default;
	};
	

	class VeTableBase {
	public:
		VeTableBase() = default;
		~VeTableBase() = default;
	};


	// A template to hold a parameter pack
	template < typename... >
	struct Typelist {};

	// Delare VeTable
	template< typename... Types> struct VeTable;

	#define VeTableType VeTable< Typelist < TypesOne... >, Typelist < TypesTwo... > >

	// Specialization of VeTable
	template< typename... TypesOne, typename... TypesTwo>
	class VeTableType : VeTableBase {

		enum class VeOnSwapDo {CLEAR, COPY};

		VeIndex32		m_thread_idx;
		bool			m_current_state;
		VeTableType*	m_other;
		VeOnSwapDo		m_on_swap_do;

		using tuple_type = std::tuple<TypesOne...>;
		using chunk_type = VeTableChunk<TypesOne...>;

		std::vector<std::unique_ptr<chunk_type>>	m_chunks;				///pointers to table chunks
		std::set<VeChunkIndex32>					m_free_chunks;			///chunks that are not full
		std::set<VeChunkIndex32>					m_deleted_chunks;		///chunks that have been deleted -> empty slot

		using map_type = std::tuple<VeHashMap<0>, TypesTwo...>;
		map_type m_maps;

	public:
		VeTable(VeOnSwapDo on_swap = VeOnSwapDo::COPY, VeTableType* other = nullptr );
		~VeTable() = default;

		//-------------------------------------------------------------------------------
		//read operations

		VeIndex32 getThreadIdx();
		auto* getCurrentState();
		auto* getNextState();

		//-------------------------------------------------------------------------------
		//write operations

		void setThreadIdx( VeIndex32 idx);
		void swapTables();
		void operator=(const VeTableType& rhs);
		void clear();
	};

	template<typename... TypesOne, typename... TypesTwo>
	VeTableType::VeTable(VeOnSwapDo on_swap, VeTableType* other) :
		m_other(other), m_on_swap_do(on_swap), m_current_state(true), m_maps() {

		m_thread_idx = VeIndex32::NULL();
		if (other != nullptr) {
			m_current_state = false;
			other->m_other = this;
		}
		m_chunks.emplace_back(std::make_unique<chunk_type>());
	};


	//-------------------------------------------------------------------------------
	//read operations

	template<typename... TypesOne, typename... TypesTwo>
	VeIndex32 VeTableType::getThreadIdx() {
		return m_thread_idx;
	}

	template<typename... TypesOne, typename... TypesTwo>
	auto* VeTableType::getCurrentState() {
		if (m_current_state) return this;
		return m_other;
	}

	template<typename... TypesOne, typename... TypesTwo>
	auto* VeTableType::getNextState() {
		if (!m_current_state) return this;
		return m_other;
	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::setThreadIdx(VeIndex32 idx) {
		m_thread_idx = idx;
	}

	//-------------------------------------------------------------------------------
	//write operations

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::swapTables() {
		if (m_other == nullptr) return;

		std::swap( m_current_state, m_other->m_current_state );

		if (m_current_state) {
			return m_other->swapTables();
		}

		if (m_on_swap_do = VeOnSwapDo::COPY) {
			*this = *m_other;
		}
		else {
			clear();
		}
	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::operator=(const VeTableType& rhs) {

	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::clear() {

	}


};



