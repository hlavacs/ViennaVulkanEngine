export module VVE:VeTable;

import std.core;
import :VeTypes;
import :VeMap;
import :VeMemory;
import :VeTableChunk;
import :VeTableState;


namespace vve {

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
	class VeTableBase {
	public:
		VeTableBase() = default;
		~VeTableBase() = default;
	};


	//-------------------------------------------------------------------------------
	//main table class

	template< typename... Types> struct VeTable;

	#define VeTableStateType VeTableState< Typelist < TypesOne... >, Typelist < TypesTwo... > >
	#define VeTableType VeTable< Typelist < TypesOne... >, Typelist < TypesTwo... > >

	template< typename... TypesOne, typename... TypesTwo>
	class VeTableType : VeTableBase {

		using tuple_type = std::tuple<TypesOne...>;

		enum class VeNumStates { ONE, TWO };
		enum class VeOnSwapDo { CLEAR, COPY };

		VeIndex32							m_thread_idx;
		std::unique_ptr<VeTableStateType>	m_current_state;
		std::unique_ptr<VeTableStateType>	m_next_state;
		VeOnSwapDo							m_on_swap_do;

	public:
		VeTable(VeNumStates num_states = VeNumStates::ONE, VeOnSwapDo on_swap = VeOnSwapDo::COPY);
		~VeTable() = default;

		//-------------------------------------------------------------------------------
		//read operations

		VeIndex32 getThreadIdx();
		auto getCurrentState();
		auto getNextState();
		auto find( VeHandle handle );

		//-------------------------------------------------------------------------------
		//write operations

		void setThreadIdx(VeIndex32 idx);
		void clear();
		auto insert(VeGuid guid, tuple_type entry);
		auto insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle);
		void erase( VeHandle handle );
	};

	template<typename... TypesOne, typename... TypesTwo>
	VeTableType::VeTable(VeNumStates num_states, VeOnSwapDo on_swap ) :  m_on_swap_do(on_swap) {
		m_current_state = std::make_unique<VeTableStateType>();
		if (num_states == VeNumStates::ONE) return;
		m_next_state = std::make_unique<VeTableStateType>();
	}

	//-------------------------------------------------------------------------------
	//read operations

	template<typename... TypesOne, typename... TypesTwo>
	VeIndex32 VeTableType::getThreadIdx() {
		return m_thread_idx;
	}

	template<typename... TypesOne, typename... TypesTwo>
	auto VeTableType::getCurrentState() {
		return m_current_state;
	}

	template<typename... TypesOne, typename... TypesTwo>
	auto VeTableType::getNextState() {
		if( m_next_state ) return m_next_state;
		return m_current_state;
	}


	//-------------------------------------------------------------------------------
	//write operations

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::setThreadIdx(VeIndex32 idx) {
		m_thread_idx = idx;
	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableType::clear() {

	}



};



