export module VVE:VETable;

import std.core;
import :VEUtil;
import :VETypes;
import :VEMap;
import :VEMemory;
import :VETableChunk;
import :VETableState;



export namespace vve {

	//-------------------------------------------------------------------------------
	//main table class

	template< int Size, typename... Types> class VeTable;

	#define VeTableStateType VeTableState< Size, Typelist < TypesOne... >, Typelist < TypesTwo... > >

	#define VeTableType VeTable< Size, Typelist < TypesOne... >, Typelist < TypesTwo... > >

	template<  int Size, typename... TypesOne, typename... TypesTwo>
	class VeTableType  {

		using tuple_type = std::tuple<TypesOne...>;
		using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

		VeIndex								d_thread_idx;
		std::unique_ptr<VeTableStateType>	d_current_state;
		std::unique_ptr<VeTableStateType>	d_next_state;

	public:
		VeTable(allocator_type alloc = {});
		~VeTable() = default;

		//-------------------------------------------------------------------------------
		//read operations

		VeIndex getThreadIdx();
		auto getCurrentState();
		auto getNextState();
		auto find( VeHandle handle );

		//-------------------------------------------------------------------------------
		//write operations

		void setThreadIdx(VeIndex idx);
		void clear();
		auto insert(VeGuid guid, tuple_type entry);
		auto insert(VeGuid guid, tuple_type entry, std::shared_ptr<VeHandle> handle);
		void erase( VeHandle handle );

		//-------------------------------------------------------------------------------
		//commit

		void commitAndClear();
		void commitAndCopy();
	};

	template< int Size, typename... TypesOne, typename... TypesTwo>
	VeTableType::VeTable(allocator_type alloc ) {
		d_current_state = std::unique_ptr<VeTableStateType>(new VeTableStateType(alloc));
		d_next_state = std::unique_ptr<VeTableStateType>(new VeTableStateType(alloc));
	}

	//-------------------------------------------------------------------------------
	//read operations

	template< int Size, typename... TypesOne, typename... TypesTwo>
	VeIndex VeTableType::getThreadIdx() {
		return d_thread_idx;
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	auto VeTableType::getCurrentState() {
		return d_current_state;
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	auto VeTableType::getNextState() {
		if( d_next_state ) return d_next_state;
		return d_current_state;
	}


	//-------------------------------------------------------------------------------
	//write operations

	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::setThreadIdx(VeIndex idx) {
		d_thread_idx = idx;
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::clear() {

	}

	//-------------------------------------------------------------------------------
	//commit

	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::commitAndClear() {
		std::swap(d_current_state, d_next_state);
		d_next_state->clear();
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::commitAndCopy() {
		std::swap(d_current_state, d_next_state);
		d_next_state = d_current_state;
	}


};
