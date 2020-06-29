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
		using value_type = std::tuple<TypesOne..., VeHandle >;
		using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

		using iterator = VeTableStateIterator< false, Size, Typelist<TypesOne...>, Maplist<TypesTwo...> >;
		using const_iterator = VeTableStateIterator< true, Size, Typelist<TypesOne...>, Maplist<TypesTwo...> >;
		using range = std::pair<iterator, iterator>;
		using crange = std::pair<const_iterator, const_iterator>;
		friend iterator;
		friend const_iterator;

		VeIndex								d_thread_idx;
		std::unique_ptr<VeTableStateType>	d_current_state;
		std::unique_ptr<VeTableStateType>	d_next_state;

	public:
		VeTable(allocator_type alloc = {});
		~VeTable() = default;

		//-------------------------------------------------------------------------------
		//state operations

		VeIndex getThreadIdx() { return d_thread_idx; };
		void setThreadIdx(VeIndex idx) { d_thread_idx = idx; };
		auto getCurrentState() { return d_current_state.get(); };
		auto getNextState() { return d_next_state.get(); };
		void commitAndClear();
		void commitAndCopy();

		//-------------------------------------------------------------------------------
		//read operations

		tuple_type	at(VeHandle& handle) { return d_current_state->at(handle); };
		std::size_t	size() { return d_current_state->size(); };
		template<int map, typename... Args>
		value_type	find(Args... args) { return d_current_state->find<map>(args...); };
		template<int map, typename... Args>
		range		equal_range(Args... args) { return d_current_state->equal_range<map>(args...); };

		//-------------------------------------------------------------------------------
		//write operations

		VeHandle	insert(TypesOne... args);
		VeHandle	insert(std::promise<VeHandle> prom, TypesOne... args);
		bool		update(VeHandle handle, TypesOne... args);
		bool		erase(VeHandle handle);
		void		operator=(const VeTableStateType& rhs);
		void		clear();
	};

	template< int Size, typename... TypesOne, typename... TypesTwo>
	VeTableType::VeTable(allocator_type alloc ) {
		d_current_state = std::unique_ptr<VeTableStateType>(new VeTableStateType(alloc));
		d_next_state = std::unique_ptr<VeTableStateType>(new VeTableStateType(alloc));
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

	//-------------------------------------------------------------------------------
	//write operations


	template< int Size, typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableType::insert(TypesOne... args) {
		return d_next_state->insert(args...);
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableType::insert(std::promise<VeHandle> prom, TypesOne... args) {
		return d_next_state->insert(std::forward<VeHandle>(prom), args...);
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	bool VeTableType::update(VeHandle handle, TypesOne... args) {
		return d_next_state->update(handle, args...);
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	bool VeTableType::erase(VeHandle handle) {
		return d_next_state->erase(handle);
	}

	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::operator=(const VeTableStateType& rhs) {

	}


	template< int Size, typename... TypesOne, typename... TypesTwo>
	void VeTableType::clear() {
		d_current_state->clear();
		d_next_state->clear();
	}

};
