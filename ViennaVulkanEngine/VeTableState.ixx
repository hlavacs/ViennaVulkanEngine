export module VVE:VeTableState;

import std.core;
import :VeTypes;
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
		//using map_type = std::tuple <TypesTwo... > ;
		using map_type = std::unordered_map<std::size_t, VeIndex>;

		//chunks
		std::vector<chunk_ptr>		m_chunks;					///pointers to table chunks
		std::stack<VeChunkIndex>	m_full_chunks;				///chunks that are used and full
		std::stack<VeChunkIndex>	m_free_chunks;				///chunks that are used and not full
		std::stack<VeChunkIndex>	m_deleted_chunks;			///chunks that have been deleted -> empty slot

		//maps
		std::unordered_map<VeHandle, VeTableIndex, VeHandle_hash>	m_slot_map;
		std::array<map_type,sizeof...(TypesTwo)>	m_maps;

	public:

		VeTableState();
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

	//----------------------------------------------------------------------------------
	template<typename... TypesOne, typename... TypesTwo>
	VeTableStateType::VeTableState() : m_maps() {
		m_chunks.emplace_back(std::make_unique<chunk_type>());
	};


	//-------------------------------------------------------------------------------
	//write operations

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::operator=(const VeTableStateType& rhs) {

	}

	template<typename... TypesOne, typename... TypesTwo>
	void VeTableStateType::clear() {

	}



};

