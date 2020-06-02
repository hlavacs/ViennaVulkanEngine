export module VVE:VeTableState;

import std.core;
import std.memory;

import :VeTypes;
import :VeUtil;
import :VeMap;
import :VeMemory;
import :VeTableChunk;

export namespace vve {

	//----------------------------------------------------------------------------------
	// Delare VeTableState
	template< typename... Types> struct VeTableState;

	#define VeTableStateType VeTableState< Typelist < TypesOne... >, Typelist < TypesTwo... > >

	//----------------------------------------------------------------------------------
	// Specialization of VeTableState
	template< typename... TypesOne, typename... TypesTwo>
	class VeTableStateType {

		using tuple_type = std::tuple<TypesOne...>;
		using chunk_type = VeTableChunk<TypesOne...>;
		static_assert(sizeof(chunk_type) <= VE_TABLE_CHUNK_SIZE);
		using chunk_ptr  = std::unique_ptr<chunk_type>;
		using map_type = decltype(TupleOfLists<TypesTwo...>());
		using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

		//chunks
		std::pmr::vector<chunk_ptr>		d_chunks;					///pointers to table chunks
		std::pmr::vector<VeChunkIndex>	d_full_chunks;				///chunks that are used and full
		std::pmr::vector<VeChunkIndex>	d_free_chunks;				///chunks that are used and not full
		std::pmr::vector<VeChunkIndex>	d_deleted_chunks;			///chunks that have been deleted -> empty slot

		//maps
		VeSlotMap									d_slot_map;
		std::array<VeHashMap,sizeof...(TypesTwo)>	d_maps;
		map_type									d_indices;

	public:

		VeTableState(allocator_type alloc = {});
		~VeTableState() = default;

		//-------------------------------------------------------------------------------
		//read operations

		std::optional<tuple_type> at(VeHandle handle);

		//-------------------------------------------------------------------------------
		//write operations

		VeHandle	insert(tuple_type entry);
		VeHandle	insert(tuple_type entry, std::shared_ptr<VeHandle> handle);
		void		update(VeHandle handle, tuple_type entry);
		void		erase(VeHandle handle);
		void		operator=(const VeTableStateType& rhs);
		void		clear();
	};

	//----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableStateType::VeTableState(allocator_type alloc) : 
		d_chunks(alloc), d_full_chunks(alloc), d_free_chunks(alloc), d_deleted_chunks(alloc), d_maps(), d_indices() {
		d_chunks.emplace_back(std::make_unique<chunk_type>());

		d_indices = TupleOfLists<TypesTwo...>();

	};


	//-------------------------------------------------------------------------------
	//read operations

	template< typename... TypesOne, typename... TypesTwo>
	std::optional<std::tuple<TypesOne...>> VeTableStateType::at(VeHandle handle) {
		
	}


	//-------------------------------------------------------------------------------
	//write operations

	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(tuple_type entry) {

	}

	template< typename... TypesOne, typename... TypesTwo>
	VeHandle VeTableStateType::insert(tuple_type entry, std::shared_ptr<VeHandle> handle) {

	}

	template< typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::update(VeHandle handle, tuple_type entry) {

	}

	template< typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::erase(VeHandle handle) {

	}


	//-------------------------------------------------------------------------------
	//write operations

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::operator=(const VeTableStateType& rhs) {

	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::clear() {

	}



};


