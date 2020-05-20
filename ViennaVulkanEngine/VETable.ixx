export module VVE:VeTable;

import std.core;
import :VeTypes;


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

	template<typename... Args>
	struct VeTableChunk {
		static constexpr uint32_t tuple_size = (sizeof(Args) + ...);
		static constexpr uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / tuple_size;
		using type = std::tuple<std::array<Args, c_max_size>...>;
		size_t	d_size = 0;
		type	d_tuple_of_array;
		size_t	size() { return d_size; };
	};

	template<typename... Args>
	class VeTable {

		enum class VeClearOnSwap {yes,no};

		VeIndex32			m_thread_idx;
		bool				m_current_state;
		VeTable<Args...>*	m_other;
		VeClearOnSwap		m_clear_on_swap;

		using tuple_data = std::tuple<Args...>;
		static constexpr uint32_t c_max_size = (VE_TABLE_CHUNK_SIZE - sizeof(size_t)) / sizeof(tuple_data);
		using chunk_type = VeTableChunk<Args...>;

		std::vector<std::unique_ptr<chunk_type>> m_chunks;		///pointers to table chunks
		std::set<VeChunkIndex32> m_free_chunks;					///chunks that are not full
		std::set<VeChunkIndex32> m_deleted_chunks;				///chunks that do not exist yet

	public:
		VeTable(VeTable<Args...>* other = nullptr, VeClearOnSwap clear = no );
		~VeTable();

		//-------------------------------------------------------------------------------
		//read operations

		VeIndex32 getThreadIdx();
		VeTable<Args...>* getCurrentState();
		VeTable<Args...>* getNextState();

		//-------------------------------------------------------------------------------
		//write operations

		void setThreadIdx( VeIndex32 idx);
		void swapTables();
		void operator=(const VeTable<Args...>& rhs);
		void clear();
	};

	template<typename... Args>
	VeTable<Args...>::VeTable(VeTable<Args...>* other, VeClearOnSwap clear) : 
		m_other(other), m_clear_on_swap(clear), m_current_state(true) {

		m_thread_idx = VeIndex32::NULL;
		if (other != nullptr) {
			m_current_state = false;
			other->m_other = this;
		}
		m_chunks.emplace_back(std::make_unique<chunk_type>());
	};

	template<typename... Args>
	VeTable<Args...>::~VeTable() {};

	//-------------------------------------------------------------------------------
	//read operations

	template<typename... Args>
	VeIndex32 VeTable<Args...>::getThreadIdx() {
		return m_thread_idx;
	}

	template<typename... Args>
	VeTable<Args...>* VeTable<Args...>::getCurrentState() {
		if (m_current_state) return this;
		return m_other;
	}

	template<typename... Args>
	VeTable<Args...>* VeTable<Args...>::getNextState() {
		if (!m_current_state) return this;
		return m_other;
	}

	template<typename... Args>
	void VeTable<Args...>::setThreadIdx(VeIndex32 idx) {
		m_thread_idx = idx;
	}

	//-------------------------------------------------------------------------------
	//write operations

	template<typename... Args>
	void VeTable<Args...>::swapTables() {
		if (m_other == nullptr) return;

		std::swap( m_current_state, m_other->m_current_state );

		if (m_current_state) {
			return m_other->swapTables();
		}

		if (m_clear_on_swap) {
			*this = *m_other;
		}
		else {
			clear();
		}
	}

	template<typename... Args>
	void VeTable<Args...>::operator=(const VeTable<Args...>& rhs) {

	}

	template<typename... Args>
	void VeTable<Args...>::clear() {

	}


};



