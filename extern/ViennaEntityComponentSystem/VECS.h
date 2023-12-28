#ifndef VECS_H
#define VECS_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <array>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include <ranges>
#include "VTLL.h"
#include "VECSTable.h"

using namespace std::chrono_literals;


/**
*
* This macro declares a VECS partition.
*
*/
#define VECS_DECLARE_PARTITION( NAME, ETL, TAGMAP, SIZEMAP, LAYOUTMAP ) \
static_assert(vtll::are_unique<ETL>::value, "The elements of" #ETL " are not unique!");\
using PARTITION = vtll::type_list<ETL, TAGMAP, SIZEMAP, LAYOUTMAP>;\
using Vecs##NAME##Handle = vecs::VecsHandleT<PARTITION>;\
template<typename CTL> using Vecs##NAME##Iterator = vecs::VecsIteratorT<PARTITION, CTL>;\
template<typename... Ts> using Vecs##NAME##Range = vecs::VecsRangeT<PARTITION, Ts...>;\
\
template<typename E = vtll::tl<>>\
class Vecs##NAME##Registry : public vecs::VecsRegistryT<PARTITION,E> {\
public:\
	using entity_type_list = typename vecs::VecsRegistryBaseClass<PARTITION>::entity_type_list; \
	using entity_tag_list = typename vecs::VecsRegistryBaseClass<PARTITION>::entity_tag_list; \
	using component_type_list = typename vecs::VecsRegistryBaseClass<PARTITION>::component_type_list; \
};\
\
template<>\
class Vecs##NAME##Registry<vtll::tl<>> : public vecs::VecsRegistryBaseClass<PARTITION> {};


namespace vecs {

	/**
	* \brief Struct to expand tags to all possible tag combinations and append them to their entity type component lists.
	*/
	template<typename TagMap, typename T>
	struct expand_tags_for_one {
		using type =
			vtll::transform_front<	///< Cat all partial tag sets after the elements of Ts
				vtll::power_set<	///< Create the set of all partial tag sets
					vtll::map<TagMap, T, vtll::type_list<> > >	///< Get tags for entity type Ts from tag map
				, vtll::cat			///< Use cat function on each partial tag set
				, T					///< And put contents of Ts (the entity components) in front of the partial tag list
			>;						///< Result of this expression
	};

	/**
	* \brief Expand the entity types of a list for all their tags.
	*
	*/
	template<typename TagMap, typename ETL>
	using expand_tags = vtll::remove_duplicates< vtll::flatten< vtll::function2< TagMap, ETL, expand_tags_for_one > > >;


	//-------------------------------------------------------------------------
	//declaration of Vecs classes

	/**
	* Declarations of the main VECS classes
	*/
	class VecsReadLock;
	class VecsWriteLock;

	//handle
	template<typename P> class VecsHandleT;

	//component table
	template<typename P, typename E> class VecsComponentAccessor;
	template<typename P, typename E, size_t I> class VecsComponentAccessorDerived;
	template<typename P, typename E> class VecsComponentTable;

	//registry
	template<typename P> class VecsRegistryBaseClass;
	template<typename P, typename E> class VecsRegistryT;

	//iterators and ranges
	template<typename P, typename CTL> class VecsIteratorT;
	template<typename P, typename ETL, typename CTL> class VecsRangeBaseClass;
	template<typename P, typename... Ts> class VecsRangeT;


	//-------------------------------------------------------------------------
	//concepts

	template<typename P, typename C>
	concept is_component_type = (vtll::has_type<typename VecsRegistryBaseClass<P>::component_type_list, std::decay_t<C>>::value); ///< C is a component

	template<typename P, typename... Cs>
	concept are_component_types = (is_component_type<P, Cs> && ...);	///< Cs are all components

	template<typename P, typename T>
	concept is_entity_tag = (vtll::has_type<typename VecsRegistryBaseClass<P>::entity_tag_list, std::decay_t<T>>::value); ///< T is a tag

	template<typename P, typename... Ts>
	concept are_entity_tags = (is_entity_tag<P, Ts> && ...);	///< Ts are all tags

	template<typename P, typename E>
	concept is_entity_type = (vtll::has_type<typename VecsRegistryBaseClass<P>::entity_type_list, std::decay_t<E>>::value); ///< E is an entity type

	template<typename P, typename... Es>
	concept are_entity_types = (is_entity_type<P, Es> && ...);		///< Es are all entity types

	/// Primary template for testing whether a list contains entity types.
	template<typename P, typename ETL>
	struct is_entity_type_list;									///< Es are all entity types

	/// Specialization for testing whether a list contains entity types.
	template<typename P, template<typename...> typename ETL, typename... Es>
	struct is_entity_type_list<P, ETL<Es...>> {					///< A list of entity types
		static const bool value = (is_entity_type<P,Es> && ...);
	};

	template<typename P, typename E, typename C>
	concept is_component_of = (vtll::has_type<E, std::decay_t<C>>::value);	///< C is a component of E

	template<typename P, typename E, typename... Cs>
	concept are_components_of = (is_component_of<P, E, Cs> && ...);	///< Cs are all components of E

	template<typename P, typename E, typename... Cs>
	concept is_composed_of = (vtll::is_same<P, E, std::decay_t<Cs>...>::value);	///< E is composed of Cs

	template<typename P, typename ET, typename E = vtll::front<ET>>
	concept is_tuple = (is_entity_type<P, E> && std::is_same_v<std::decay_t<ET>, vtll::to_tuple<E>>); ///< ET is a std::tuple

	template<typename P, typename E, typename... Cs>
	concept is_iterator = (std::is_same_v<std::decay_t<E>, VecsIteratorT<P, Cs...>>);	///< E is composed of Cs


	//-------------------------------------------------------------------------
	//entity handle

	/**
	* \brief Handles are IDs of entities. Use them to access entitites.
	*
	* VecsHandle are used to ID entities of type E by storing their type as an index.
	*/
	template<typename P>
	class VecsHandleT {

		//friends
		friend class VecsReadLock;
		friend class VecsWriteLock;
		friend VecsRegistryBaseClass<P>;
		template<typename P, typename E> friend class VecsRegistryT;
		template<typename P, typename E> friend class VecsComponentTable;

	protected:
		map_index_t	m_map_index{};				///< The slot of the entity in the entity map
		counter_t	m_generation_counter{};		///< Generation counter

	public:
		VecsHandleT() noexcept = default;		///< Empty constructor of class VecsHandle

		/**
		* \brief Constructor of class VecsHandle.
		* 
		* \param[in] idx The index of the handle in the aggregate slot map of class VecsRegistryBaseClass
		* \param[in] cnt Current generation counter identifying this entity on in the slot.
		* \param[in] type Type index for the entity type E.
		*/
		VecsHandleT(map_index_t idx, counter_t cnt) noexcept : m_map_index{ idx }, m_generation_counter{ cnt } {};

		inline auto is_valid() noexcept	-> bool;	///< The data in the handle is non null
		auto has_value() noexcept		-> bool;	///< The entity that is pointed to exists in the ECS

		template<typename C>
		requires is_component_type<P, C>
		auto has_component() noexcept	-> bool;	///< Return true if the entity type has a component C

		template<typename C>
		requires is_component_type<P, C>
		auto component() noexcept		-> C& { 	///< \returns component of type C of the entity (first found is copied) 
			return *component_ptr<C>(); 
		};

		template<typename C>
		requires is_component_type<P, C>
		auto component_ptr() noexcept	-> C*;		///< Get a pointer to component of type C of the entity (first found is copied) 

		template<typename... Cs>					///< Update components of type C
		requires are_component_types<P, Cs...>		///< \param[in] args Arguments to write over
		auto update(Cs&&... args) noexcept -> bool;	///< \returns true of successful

		auto erase(bool destruct=false) noexcept -> bool;							///< Erase the entity 
		auto destruct() noexcept				 -> bool { return erase(true); };	///< Destruct the entity 

		auto map_index() noexcept	-> map_index_t { ///< \returns index of entity in the map
			return m_map_index; 
		}; 

		auto type() noexcept		-> type_index_t;	///< Get type of this entity

		auto mutex_ptr() noexcept	-> std::atomic<uint32_t>*;	///< \returns address of the VECS mutex for this entity

		bool operator==(const VecsHandleT<P>& rhs) {	///< Equality operator
			return m_map_index == rhs.m_map_index && m_generation_counter == rhs.m_generation_counter;
		}
	};


	//-------------------------------------------------------------------------
	//component vector - each entity type has them

	/**
	* \brief Base class for dispatching accesses to entity components if the entity type is not known.
	* 
	* This is used by component tables to access components of any type. See dispatch table of class VecsComponentTable
	* 
	*/
	template<typename P, typename E>
	class VecsComponentAccessor {
	public:
		VecsComponentAccessor() noexcept {};	///<Constructor
		virtual auto updateC(table_index_t index, size_t compidx, void* ptr, size_t size, bool move = false) noexcept	-> bool = 0;	///< Empty update
		virtual auto componentE_ptr(table_index_t entidx, size_t compidx) noexcept	-> void* = 0;	///< Empty component read
		virtual auto has_componentE()  noexcept										-> bool = 0;	///< Test for component
	};


	/**
	* \brief This class stores all components of entities of type E. It as basically an adaptor for class VecsTable.
	*
	* Since templated functions cannot be virtual, the class emulates virtual functions to access components
	* by creating a dispatch table. Each component from the component table gets its own entry, even if the
	* entity type does not contain this component type.
	*/
	template<typename P, typename E>
	class VecsComponentTable : public VecsMonostate<VecsComponentTable<P, E>> {

		//friends
		template<typename P, typename E, size_t I> friend class VecsComponentAccessorDerived;
		template<typename P> friend class VecsRegistryBaseClass;
		template<typename P, typename E> friend class VecsRegistryT;
		template<typename P, typename CTL> friend class VecsIteratorT;
		template<typename P, typename ETL, typename CTL> friend class VecsRangeBaseClass;

	protected:

		using entity_tag_map = typename VecsRegistryBaseClass<P>::entity_tag_map;			///< Tag map of this instance
		using table_size_map = typename VecsRegistryBaseClass<P>::table_size_map;			///< Size map of this instance
		using table_layout_map = typename VecsRegistryBaseClass<P>::table_layout_map;		///< Layout map of this instance
		using table_size_default = typename VecsRegistryBaseClass<P>::table_size_default;	///< Default size
		using component_type_list = typename VecsRegistryBaseClass<P>::component_type_list;	///< List of the component types

		using tuple_value_t = vtll::to_tuple<E>;		///< A tuple storing all components of entity of type E
		using tuple_ref_t = vtll::to_ref_tuple<E>;		///< A tuple storing references to all components of entity of type E
		using tuple_ptr_t = vtll::to_ptr_tuple<E>;		///< A tuple storing pointers to all components of entity of type E
		using layout_type = vtll::map<table_layout_map, E, VECS_LAYOUT_DEFAULT>; ///< ROW or COLUMN

		struct handle_wrapper_t {
			using handle_type = std::atomic<VecsHandleT<P>>;
			handle_type m_handle{};

			handle_wrapper_t() = default;
			handle_wrapper_t(const VecsHandleT<P>& t) { m_handle.store(t); }
			handle_wrapper_t(const handle_wrapper_t& t) { m_handle.store(t.m_handle.load()); }
			auto operator=(const handle_wrapper_t& t) { m_handle.store(t.m_handle.load()); }
		};

		struct mutex_wrapper_t {
			std::atomic<uint32_t>* m_mutex{ nullptr };
		};

		using info_types = vtll::type_list<mutex_wrapper_t, handle_wrapper_t>;	///< List of management data per entity (handle and mutex)
		static const size_t c_mutex = 0;		///< Component index of the handle info
		static const size_t c_handle = 1;		///< Component index of the handle info
		static const size_t c_info_size = 2;	///< Index where the entity data starts

		using data_types = vtll::cat< info_types, E >;						///< List with management (info) and component (data) types
		using types_deleted = vtll::type_list< table_index_t >;	///< List with types for holding info about erased entities 
		
		static const size_t c_segment_size = vtll::front_value< vtll::map<table_size_map, E, table_size_default > >::value;///Size of a segment
		static inline VecsTable<P, data_types, c_segment_size, layout_type::value>			m_data;		///< Data per entity
		static inline VecsTable<P, types_deleted, c_segment_size, VECS_LAYOUT_ROW::value>	m_erased;	///< Table holding the indices of erased entities
		static inline VecsTable<P, types_deleted, c_segment_size, VECS_LAYOUT_ROW::value>	m_destructed;	///< Table holding the indices of deleted entities
		static const size_t c_row_size = layout_type::value ? sizeof(vtll::to_tuple<data_types>) : 0;					///< Size of a row

		///One instance for each component type
		using array_type = std::array<std::unique_ptr<VecsComponentAccessor<P, E>>, vtll::size<component_type_list>::value>;
		static inline array_type m_dispatch;	///< Each component type C of the entity type E gets its own specialized class instance

		template<typename CTL>
		static inline auto accessor(table_index_t index) {
			return std::tuple_cat(
				std::make_tuple(
					&m_data.component_ptr<c_mutex>(index)->m_mutex, &m_data.component_ptr<c_handle>(index)->m_handle)
					, vtll::subtype_tuple<vtll::to_ptr<CTL>>(m_data.tuple_ptr(index)
				)
			);
		}

		//-------------------------------------------------------------------------

		auto updateC(table_index_t entidx, size_t compidx, void* ptr, size_t size, bool move = false) noexcept	-> bool;	///< For dispatching
		auto componentE_ptr(table_index_t entidx, size_t compidx) noexcept						-> void*;	///< For dispatching
		auto has_componentE(size_t compidx)  noexcept											-> bool;	///< Test for component
		auto remove_deleted_tail() noexcept														-> void;	///< Remove empty slots at the end of the table
		auto destruct(table_index_t index) noexcept -> void;

		VecsComponentTable() noexcept;			///< Protected constructor that does not allocate the dispatch table

		//-------------------------------------------------------------------------
		//insert

		template<typename... Cs>
		requires are_components_of<P, E, Cs...> [[nodiscard]]
		auto insert(std::atomic<uint32_t>* mutex, VecsHandleT<P> handle, Cs&&... args) noexcept	-> table_index_t; ///< Insert new entity

		//-------------------------------------------------------------------------
		//read data

		template<typename C>				///< \brief Get reference to a component
		requires is_component_of<P, E, C>	///< \param[in] index Index in the component table
		auto component(const table_index_t index) noexcept -> C& { return *component_ptr<C>(index); };	///< \returns reference to a component

		template<typename C>
		requires is_component_of<P, E, C>
		auto component_ptr(const table_index_t index) noexcept	-> C*;		///< Get pointer to a component

		auto tuple(const table_index_t index) noexcept			-> tuple_ref_t;	///< \returns tuple with copies of the components
		auto tuple_ptr(const table_index_t index) noexcept		-> tuple_ptr_t;	///< \returns tuple with pointers to the components

		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs>
		requires are_components_of<P, E, Cs...>
		auto update(const table_index_t index, Cs&&... args) noexcept -> bool;	///< Update a component

		//-------------------------------------------------------------------------
		//erase data

		auto erase(const table_index_t idx, bool destruct) noexcept	-> bool;	///< Erase an entity from the table
		auto clear(bool destruct) noexcept							-> size_t;	///< Mark all rows as erased

		//-------------------------------------------------------------------------
		//utilities

		auto handle(const table_index_t index) noexcept			-> VecsHandleT<P>;	///< \returns ptr to handle for an index in the table
		auto mutex_ptr(const table_index_t index) noexcept		-> std::atomic<uint32_t>*;	///< \returns pointer to the mutex for a given index
		auto size() const noexcept -> size_t { 	///< \returns the number of entries currently in the table, can also be invalid ones
			return m_data.size(); 
		};
		auto compress() noexcept						-> void;	///< Compress the table
		auto capacity() noexcept						-> size_t;	///< \returns the max number of entities that can be stored
		auto swap(table_index_t n1, table_index_t n2)	-> bool { return m_data.swap(n1, n2); }	///< Swap places for two rows
	};

	/**
	* \brief Dispatch an update call to the correct specialized class instance.
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \param[in] ptr Pointer to the component data to use.
	* \param[in] size Size of the component data.
	* \param[in] move If true then the data should be moved.
	* \returns true if the update was successful
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::updateC(table_index_t entidx, size_t compidx, void* ptr, size_t size, bool move) noexcept -> bool {
		return m_dispatch[compidx]->updateC(entidx, compidx, ptr, size, move);
	}

	/**
	* \brief Test whether an entity has a component with an index
	*
	* \param[in] compidx Index of the component of the entity.
	* \returns true if the entity contains the component
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::has_componentE(size_t compidx)  noexcept -> bool {
		return m_dispatch[compidx]->has_componentE();
	}

	/**
	* \brief Get pointer to a component.
	*
	* \param[in] entidx Entity index in the component table.
	* \param[in] compidx Index of the component of the entity.
	* \returns pointer to the component.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::componentE_ptr(table_index_t entidx, size_t compidx)  noexcept -> void* {
		return m_dispatch[compidx]->componentE_ptr(entidx, compidx);
	}

	/**
	* \brief Insert data for a new entity into the component table.
	*
	* \param[in] handle The handle of the new entity.
	* \param[in] mutex Pointer to the mutex used by this entity.
	* \param[in] args The entity components to be stored.
	* \returns the index of the new entry in the component table.
	*/
	template<typename P, typename E>
	template<typename... Cs>
	requires are_components_of<P, E, Cs...> [[nodiscard]]
	inline auto VecsComponentTable<P, E>::insert(std::atomic<uint32_t>* mutex, VecsHandleT<P> handle, Cs&&... args) noexcept -> table_index_t {
		std::tuple<table_index_t> tup;
		if (m_destructed.pop_back(&tup)) {				/// Can recycle an old row?
			table_index_t index = std::get<0>(tup);
			m_data.update(index, mutex_wrapper_t{ mutex }, handle_wrapper_t{ handle }, std::forward<Cs>(args)...);

			using types = vtll::not_in_list<E,vtll::tl<std::decay_t<Cs>...>>; //initilialize the other components
			vtll::static_for<size_t, 0, vtll::size<types>::value >([&](auto i) { m_data.update(index, vtll::Nth_type<types, i>{}); });
			return index;
		}
		return m_data.push_back(mutex_wrapper_t{ mutex }, handle_wrapper_t{ handle }, std::forward<Cs>(args)... );			///< Allocate space a the end of the table
	};

	/**
	* \brief Get pointer to a component type C given as template parameter
	* 
	* \param[in] index Index of the entity in the component table.
	* \returns the component of type C for an entity.
	*/
	template<typename P, typename E>
	template<typename C>
	requires is_component_of<P, E, C>
	inline auto VecsComponentTable<P, E>::component_ptr(const table_index_t index) noexcept -> C* {
		return m_data.component_ptr<c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index); ///< Get ref to the entity and return component
	}

	/**
	* \brief Get a tuple containing pointers to the components of an entity.
	* 
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::tuple_ptr(const table_index_t index) noexcept -> tuple_ptr_t {
		assert(index < m_data.size());
		return vtll::sub_tuple< c_info_size, vtll::size<data_types>::value >(m_data.tuple_ptr(index));	///< Return only entity components in a subtuple
	}

	/**
	* \brief Get a tuple containing references to the components of an entity.
	* 
	* \param[in] index The index of the data in the component table.
	* \returns a tuple holding the components of an entity.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::tuple(const table_index_t index) noexcept -> tuple_ref_t {
		assert(index < m_data.size());
		return vtll::sub_ref_tuple< c_info_size, vtll::size<data_types>::value >(m_data.tuple(index));	///< Return only entity components in a subtuple
	}

	/**
	* \brief Update a component of types Cs... for an entity of type E.
	*
	* \param[in] index The index of the data in the component table.
	* \param[in] args The compnent data.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename E>
	template<typename... Cs>
	requires are_components_of<P, E, Cs...>
	inline auto VecsComponentTable<P, E>::update(const table_index_t index, Cs&&... args) noexcept -> bool {
		return (m_data.update<c_info_size + vtll::index_of<E, Cs>::value>(index, std::forward<Cs>(args)) && ... && true );
	}

	/**
	* \brief Erase the component data for an entity.
	*
	* The data is not really erased but the handle is invalidated. The index is also pushed to the deleted table.
	* \param[in] index The index of the entity data in the component table.
	* \param[in] destruct If true, the component destructors are called.
	* \returns true if the data was set to invalid.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::erase(const table_index_t index, bool destr) noexcept -> bool {
		assert(index < m_data.size());
		m_data.component<c_handle>(index).m_handle.store({});	///< Invalidate handle	

		if (destr) {
			destruct(index);
			m_destructed.push_back(index);	///< Push the index to the deleted table.
		}
		else {
			m_erased.push_back(index);	///< Push the index to the deleted table.
		}

		return true;
	}

	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::destruct(table_index_t index) noexcept -> void {
		vtll::static_for<size_t, 0, vtll::size<E>::value >(			///< Loop over all components
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				if constexpr (std::is_destructible_v<type> && !std::is_trivially_destructible_v<type>) {
					m_data.component_ptr<c_info_size + i>(index)->~type(); 	///< Call destructor
				}
			}
		);
	}


	/**
	* \brief Get pointer to a handle.
	* 
	* \param[in] index The index of the entity in the component table.
	* \returns the handle of an entity from the component table.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::handle(const table_index_t index) noexcept -> VecsHandleT<P> {
		if (index >= m_data.size()) return {};
		return m_data.component<c_handle>(index).m_handle.load();	///< Get ref to the handle and return it
	}

	/**
	* \brief Get a pointer to a mutex.
	* 
	* \param[in] index The index of the entity in the component table.
	* \returns pointer to the sync mutex of the entity.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::mutex_ptr(const table_index_t index) noexcept -> std::atomic<uint32_t>* {
		if (index >= m_data.size()) return nullptr;
		return m_data.component<c_mutex>(index).m_mutex;		///< Get ref to the mutex and return it
	}

	/**
	* \brief Set the max number of entries that can be stored in the component table.
	*
	* If the new max is smaller than the previous one it will be ignored. Tables cannot shrink.
	*
	* \param[in] r New max number or entities that can be stored.
	* \returns the current max number, or the new one.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::capacity() noexcept -> size_t {
		return m_data.capacity();
	}

	/**
	* \brief Remove invalid erased entity data at the end of the component table.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::remove_deleted_tail() noexcept -> void {
		while (m_data.size() > 0) {
			auto handle = m_data.component<c_handle>(table_index_t{ m_data.size() - 1 }).m_handle.load();
			if (handle.is_valid()) return;	///< is last entity is valid, then return
			m_data.remove_back();			///< Else remove it and continue the loop
		}
	}



	//-------------------------------------------------------------------------
	//component accessor derived class

	/**
	* \brief For dispatching accesses to entity components if the entity type is not known at compile time by the caller.
	*
	* It is used to access component I of entity type E. This is called through a dispatch table, one entry
	* for each component of E.
	*/
	template<typename P, typename E, size_t I>
	class VecsComponentAccessorDerived : public VecsComponentAccessor<P, E> {
		using component_type_list = typename VecsRegistryBaseClass<P>::component_type_list;

	public:
		using C = vtll::Nth_type<component_type_list, I>;	///< Component type
		VecsComponentAccessorDerived() noexcept : VecsComponentAccessor<P, E>() {} ///< Constructor of class VecsComponentAccessorDerived 

	protected:

		/**
		* \brief Update the component C of entity E.
		* 
		* \param[in] index Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \param[in] ptr Pointer to where the data comes from.
		* \param[in] size Size of the component data.
		* \param[in] move If true then the data should be moved.
		* \returns true if the update was successful.
		*/
		auto updateC(table_index_t index, size_t compidx, void* ptr, size_t size, bool move) noexcept -> bool {
			if constexpr (is_component_of<P, E, C>) {
				if (move) {
					if constexpr (std::is_move_assignable_v<C>) {
						VecsComponentTable<P, E>().m_data.component<VecsComponentTable<P, E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = std::move(*((C*)ptr));
					}
				}
				else {
					if constexpr (std::is_copy_assignable_v<C>) {
						VecsComponentTable<P, E>().m_data.component<VecsComponentTable<P, E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = *((C*)ptr);
					}
					else {
						if constexpr (std::is_move_assignable_v<C>) {
							VecsComponentTable<P, E>().m_data.component<VecsComponentTable<P, E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(index) = std::move(*((C*)ptr));
						}
					}
				}
				return true;
			}
			return false;
		}

		/**
		* \brief Test whether an entity of type E contains a component of type C.
		* \returns true if the retrieval was successful.
		*/
		auto has_componentE()  noexcept -> bool {
			if constexpr (is_component_of<P, E, C>) {
				return true;
			}
			return false;
		}

		/**
		* \brief Get pointer to component data.
		* 
		* \param[in] entidx Index of the entity in the component table.
		* \param[in] compidx Component index incomponent type list.
		* \returns pointer to the component.
		*/
		auto componentE_ptr(table_index_t entidx, size_t compidx)  noexcept -> void* {
			if constexpr (is_component_of<P, E, C>) {
				return (void*)VecsComponentTable<P, E>().m_data.component_ptr<VecsComponentTable<P, E>::c_info_size + vtll::index_of<E, std::decay_t<C>>::value>(entidx);
			}
			return nullptr;
		}
	};

	/**
	* \brief Constructor of class VecsComponentTable.
	*
	* Initializes the class and reserve memory for the tables, and initialize
	* the component dispatch. Every possible component gets its own entry in the dispatch
	* array.
	*
	*/
	template<typename P, typename E>
	inline VecsComponentTable<P, E>::VecsComponentTable() noexcept {
		if (!this->init()) return;

		vtll::static_for<size_t, 0, vtll::size<component_type_list>::value >( ///< Create dispatch table for all component types
			[&](auto i) {
				m_dispatch[i] = std::make_unique<VecsComponentAccessorDerived<P, E, i>>();
			}
		);
	};


	//-------------------------------------------------------------------------
	//entity registry base class

	/**
	* \brief This class stores all generalized handles of all entities, and can be used
	* to insert, update, read and delete all entity types.
	*
	* The base class does not depend on template parameters. If a member function is invoked,
	* the base class forwards the call to the respective derived class with the correct entity type E.
	* Since types have indexes, it needs an array holding instances of the derived classes for dispatching
	* the call to the correct subclass.
	*
	* The base class has a mapping table storing information about all currently valid entities. From there
	* the respective subclass for type E can find the component data in the component table of type E.
	* 
	* An entity type is a collection of component types. In the tag map, tags can be defined for an entity type.
	* Thus there are variants of this entity type that also hold one or more tags as components.
	* expand_tags takes a list of entity types, uses the tags from the tag map and creates all variants of each
	* entity type, holding zero to all tags for this entity type.
	*
	* An entity tag map defines the possible tags for each entity type. Tags are special components that
	* can be appended to the end of the component list of entities. Entries in the map are type_lists of tags possible for
	* each entry type.
	*
	* A table size map is a VTLL map specifying the default sizes for component tables.
	* It is the sum of the maps of the engine part, and the maps as defined by the engine user
	* Every entity type can have an entry, but does not have to have one.
	* An entry is alwyas a vtll::value_list<A,B>, where A defines the default size of segments, and B defines
	* the max number of entries allowed. Both A and B are actually exponents with base 2. So segemnts have size 2^A.
	*
	* A layout map efines whether the data layout for entity type E is row or column wise. The default is columns wise.
	*/
	template<typename P>
	class VecsRegistryBaseClass : public VecsMonostate<VecsRegistryBaseClass<P>> {

		//friends
		template<typename P> friend class VecsHandleT;
		template<typename P, typename E> friend class VecsComponentTable;
		template<typename P, typename E, size_t I> friend class VecsComponentAccessorDerived;
		template<typename P, typename E> friend class VecsRegistryT;

	public:
		using etl_wo_tags		= vtll::Nth_type<P, 0>;		///< Entity type list without tags
		using entity_tag_map	= vtll::Nth_type<P, 1>;		///< Entity tag map
		using table_size_map	= vtll::Nth_type<P, 2>;		///< Table segment size map 
		using table_layout_map	= vtll::Nth_type<P, 3>;		///< Table layout map
		using entity_type_list = expand_tags< entity_tag_map, etl_wo_tags >;

		using entity_tag_list = vtll::flatten< vtll::transform< entity_tag_map, vtll::back > >;
		using component_type_list_wo_tags = vtll::remove_types< vtll::remove_duplicates< vtll::flatten<entity_type_list> >, entity_tag_list>;
		using component_type_list = vtll::cat<component_type_list_wo_tags, entity_tag_list>;
		using table_size_default = vtll::value_list< 1 << 10 >;

		using table_constants = vtll::transform < vtll::apply_map< table_size_map, entity_type_list, table_size_default>, vtll::value_to_type>;
		using table_max_seg = vtll::max< vtll::transform< table_constants, vtll::front > >;	///< Get max size from map

		using types_deleted = vtll::type_list< map_index_t >;	///< List with types for holding erased map entries

	protected:
		/// Types for the table: index in the component table, generation counter, entity type, mutex for locking entity
		using types = vtll::type_list<table_index_t, counter_t, type_index_t, std::atomic<uint32_t>>;
		static const uint32_t c_index{ 0 };		///< Index for accessing the index to next free or entry in component table
		static const uint32_t c_counter{ 1 };	///< Index for accessing the generation counter
		static const uint32_t c_type{ 2 };		///< Index for accessing the type index
		static const uint32_t c_mutex{ 3 };		///< Index for accessing the lock mutex
		static const size_t	  c_segment_size = table_max_seg::value;
		static const uint32_t c_uint32_max = std::numeric_limits<uint32_t>::max();

		static inline VecsTable<P, types, c_segment_size, VECS_LAYOUT_ROW::value>			 m_map_table;	///< The main mapping table
		static inline VecsTable<P, types_deleted, c_segment_size, VECS_LAYOUT_COLUMN::value> m_erased;		///< Table holding the indices of erased map entries
		static inline std::atomic<uint32_t>	m_size{ 0 };					///< Number of valid entities in the map

		/// Every subclass for entity type E has an entry in this table.
		static inline std::array<std::unique_ptr<VecsRegistryBaseClass<P>>, vtll::size<entity_type_list>::value> m_dispatch;

		/// Virtual function for dispatching component updates to the correct subclass for entity type E.
		virtual auto updateC(VecsHandleT<P> handle, size_t compidx, void* ptr, size_t size, bool move = false) noexcept -> bool { return false; };

		/// Virtual function for dispatching component reads to the correct subclass for entity type E.
		virtual auto componentE_ptr(VecsHandleT<P> handle, size_t compidx) noexcept -> void* { return nullptr; };

		/// Virtual function for dispatching whether an entity type E contains a component type C
		virtual auto has_componentE(VecsHandleT<P> handle, size_t compidx) noexcept -> bool { return false; };

		/// Virtual function for dispatching erasing an entity from a component table 
		virtual auto eraseE(table_index_t index, bool destruct) noexcept	-> void { };

		virtual auto sizeE() noexcept -> std::atomic<uint32_t>& { return m_size; };

		auto table_index_ptr(map_index_t index) noexcept	-> table_index_t*;			///< \returns row index in component table
		auto counter_ptr(map_index_t index) noexcept		-> counter_t*;				///< \returns generation counter value
		auto type_ptr(map_index_t index) noexcept			-> type_index_t*;			///< \returns type index in map
		auto mutex_ptr(map_index_t index) noexcept			-> std::atomic<uint32_t>*;	///< \returns mutex from map

	public:
		static VecsRegistryBaseClass<P> m_registry;

		VecsRegistryBaseClass() noexcept;

		//-------------------------------------------------------------------------
		//get data

		template<typename C>
		requires is_component_type<P, C>
		auto has_component(VecsHandleT<P> handle) noexcept	-> bool;	///< \returns true if the entity of type E has a component of type C

		template<typename C>
		requires is_component_type<P, C>
		auto component(VecsHandleT<P> handle) noexcept -> C& { return *component_ptr<C>(handle); };	///< Get a component of type C

		template<typename C>
		requires is_component_type<P, C>
		auto component_ptr(VecsHandleT<P> handle) noexcept -> C*;		///< Get a component of type C

		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs>
		requires are_component_types<P, Cs...>
		auto update(VecsHandleT<P> handle, Cs&&... args) noexcept -> bool;	///< Update component of type C of an entity

		//-------------------------------------------------------------------------
		//erase data

		virtual auto erase(VecsHandleT<P> handle, bool destruct=false) noexcept -> bool;				///< Erase an entity
		virtual auto destruct(VecsHandleT<P> handle) noexcept -> bool { return erase(handle, true); };	///< Mark an entity as erased

		template<typename... Es>
		requires (are_entity_types<P, Es...>)
		auto clear(bool destruct=true) noexcept	-> size_t;		///< Clear the whole ECS

		//-------------------------------------------------------------------------
		//utility

		template<typename... Es>
		requires (are_entity_types<P, Es...>)
		auto size() const noexcept -> size_t;		///< \returns the total number of valid entities of types Es

		auto compress() noexcept						-> void;			///< Compress all component tables
		auto type(VecsHandleT<P> h) noexcept			-> type_index_t;	///< \returns type of entity
		auto has_value(VecsHandleT<P> handle) noexcept	-> bool;			///< \returns true if the ECS still holds this entity 

		virtual auto swap(VecsHandleT<P> h1, VecsHandleT<P> h2) noexcept	-> bool;	///< Swap places of two entities in the component table
		virtual auto print_type(VecsHandleT<P> handle)						-> void {
			auto t = type(handle);
			m_dispatch[t]->print_type(handle);
		};

		virtual auto print_entities()					-> void {
			for (auto& t : m_dispatch) {
				t->print_entities();
			}
		};

	};

	template<typename P>
	inline VecsRegistryBaseClass<P> VecsRegistryBaseClass<P>::m_registry{};	///< Create instance for the VecsRegistryBaseClass class

	/**
	* \brief Test whether an entity (pointed to by the handle) of type E contains a component of type C.
	* The call is dispatched to the correct subclass for entity type E.
	* 
	* \param[in] handle The handle of the entity to get the data from.
	* \returns true if entity type E contains component type C
	*/
	template<typename P>
	template<typename C>
	requires is_component_type<P, C>
		inline auto VecsRegistryBaseClass<P>::has_component(VecsHandleT<P> handle) noexcept -> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[type(handle)]->has_componentE(handle, vtll::index_of<component_type_list, std::decay_t<C>>::value);
	}

	/**
	* \brief Get pointer to a component of type C for a given handle.
	* 
	* \param[in] handle The handle of the entity to get the data from.
	* \returns pointer to a component of type C.
	*/
	template<typename P>
	template<typename C>
	requires is_component_type<P, C>
		inline auto VecsRegistryBaseClass<P>::component_ptr(VecsHandleT<P> handle) noexcept -> C* {
		if (!handle.is_valid()) return nullptr;
		/// Dispatch to the correct subclass and return result
		return (C*)m_dispatch[type(handle)]->componentE_ptr(handle, vtll::index_of<component_type_list, std::decay_t<C>>::value);
	}

	/**
	* \brief Update a component of an entity. The call is dispatched to the correct subclass for entity type E.
	* 
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the update was successful.
	*/
	template<typename P>
	template<typename... Cs>
	requires are_component_types<P, Cs...>
		inline auto VecsRegistryBaseClass<P>::update(VecsHandleT<P> handle, Cs&&... args) noexcept -> bool {
		if (!handle.is_valid()) return false;
		/// Dispatch the call to the correct subclass and return result
		(m_dispatch[type(handle)]->updateC(handle, vtll::index_of<component_type_list, std::decay_t<Cs>>::value, (void*)&args, sizeof(Cs), std::is_rvalue_reference<decltype(args)>::value), ...);
		return true;
	}

	/**
	* \brief Erase an entity from the ECS. The call is dispatched to the correct subclass for entity type E.
	* 
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::erase(VecsHandleT<P> handle, bool destruct) noexcept -> bool {
		if (!handle.is_valid()) return false;
		return m_dispatch[type(handle)]->erase(handle, destruct); ///< Dispatch to the correct subclass for type E and return result
	}


	/**
	* \brief Return table index of an entity in the component table.
	* 
	* \param[in] index The map index.
	* \returns the index of the entity in the component table.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::table_index_ptr(map_index_t index) noexcept -> table_index_t* {
		if (!index.has_value()) return nullptr;
		return m_map_table.component_ptr<VecsRegistryBaseClass::c_index>(table_index_t{ index.value });
	}

	/**
	* \brief Return pointer to the generation counter of an entity.
	*
	* \param[in] index The map index.
	* \returns pointer to the generation counter of an entity.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::counter_ptr(map_index_t index) noexcept	-> counter_t* {
		return m_map_table.component_ptr<VecsRegistryBaseClass<P>::c_counter>(table_index_t{ index.value });
	}

	/**
	* \brief Return pointer to the type index of an entity.
	*
	* \param[in] index The map index.
	* \returns pointer to the type index of an entity.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::type_ptr(map_index_t index) noexcept		-> type_index_t* {
		return m_map_table.component_ptr<VecsRegistryBaseClass<P>::c_type>(table_index_t{ index.value });
	}

	/**
	* \brief Return pointer to the mutex of an entity.
	*
	* \param[in] index The map index.
	* \returns pointer to the mutex of an entity.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::mutex_ptr(map_index_t index) noexcept		-> std::atomic<uint32_t>* {
		return m_map_table.component_ptr<VecsRegistryBaseClass<P>::c_mutex>(table_index_t{ index.value });
	}

	/**
	* \brief Return index of an entity in the component table.
	* 
	* \param[in] handle The entity handle.
	* \returns the index of the entity in the component table.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::type(VecsHandleT<P> h) noexcept -> type_index_t {
		if (!h.is_valid()) return {};
		return *type_ptr( h.m_map_index );
	}

	/**
	* \brief Swap the rows of two entities of the same type.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] h1 First entity.
	* \param[in] h2 Second entity.
	* \returns true if operation was successful.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::swap(VecsHandleT<P> h1, VecsHandleT<P> h2) noexcept	-> bool {
		if (!h1.is_valid() || !h1.is_valid() || type(h1) != type(h2)) return false;
		return m_dispatch[type(h1)]->swap(h1, h2);				///< Dispatch to the correct subclass for type E and return result
	}

	/**
	* \brief Entry point for checking whether a handle is still valid.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] handle The handle to check.
	* \returns true if the ECS contains this entity.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::has_value(VecsHandleT<P> handle) noexcept	-> bool {
		if (!handle.is_valid()) return false;
		return (handle.m_generation_counter == *counter_ptr(handle.m_map_index));
	}


	//-------------------------------------------------------------------------
	//VecsRegistryT<E>

	/**
	* \brief VecsRegistryT<E> is used as access interface for all entities of type E.
	*/
	template<typename P, typename E = vtll::type_list<>>
	class VecsRegistryT : public VecsRegistryBaseClass<P> {

		friend class VecsRegistryBaseClass<P>;
		template<typename P, typename ETL, typename CTL> friend class VecsRangeBaseClass;

	protected:
		using entity_type_list = typename VecsRegistryBaseClass<P>::entity_type_list;
		using component_type_list = typename VecsRegistryBaseClass<P>::component_type_list;

		static inline VecsComponentTable<P, E>	m_component_table;	///< The component table for entity type E
		static inline std::atomic<uint32_t>		m_sizeE{ 0 };		///< Store the number of valid entities of type E currently in the ECS

		/// Implementations of functions that receive dispatches from the base class.
		auto updateC(VecsHandleT<P> handle, size_t compidx, void* ptr, size_t size, bool move = false) noexcept	-> bool; ///< Dispatch from base class
		auto componentE_ptr(VecsHandleT<P> handle, size_t compidx) noexcept	-> void*;///< Get a pointer to a component
		auto has_componentE(VecsHandleT<P> handle, size_t compidx) noexcept	-> bool; ///< Check if E has a component
		auto eraseE(table_index_t index, bool destruct) noexcept	-> void { m_component_table.erase(index, destruct); };	 ///< Erase some entity
		auto compressE() noexcept	-> void { return m_component_table.compress(); };	///< Compress the table
		auto clearE(bool destruct) noexcept	-> size_t { return m_component_table.clear(destruct); };	///< Erase all entities from table
		auto sizeE() noexcept		-> std::atomic<uint32_t>& { return m_sizeE; };

	public:
		VecsRegistryT() noexcept : VecsRegistryBaseClass<P>() {};					///< Constructor of class VecsRegistryT<E>

		//-------------------------------------------------------------------------
		//insert data

		template<typename... Cs>
		requires are_components_of<P, E, Cs...> [[nodiscard]]
		auto insert(Cs&&... args) noexcept				-> VecsHandleT<P>;	///< Insert new entity of type E into VECS

		template<typename... Cs>
		auto transform(VecsHandleT<P> handle, Cs&&... args) noexcept -> bool;	///< transform entity into new type

		//-------------------------------------------------------------------------
		//get data

		auto tuple(VecsHandleT<P> handle) noexcept		-> vtll::to_ref_tuple<E>;	///< Return a tuple with copies of the components
		auto tuple_ptr(VecsHandleT<P> handle) noexcept	-> vtll::to_ptr_tuple<E>;	///< Return a tuple with pointers to the components

		template<typename C>
		requires is_component_type<P, C>
		auto has_component() noexcept					-> bool {	///< Return true if the entity type has a component C
			return is_component_of<P, E, C>;
		}

		template<typename C>
		requires is_component_of<P, E, C>
		auto component(VecsHandleT<P> handle) noexcept -> C& { return *component_ptr<C>(handle); };	///< Get to a component

		template<typename C>
		requires is_component_of<P, E, C>
		auto component_ptr(VecsHandleT<P> handle) noexcept	-> C*;	///< Get pointer to a component
			
		//-------------------------------------------------------------------------
		//update data

		template<typename... Cs> 
		requires are_components_of<P, E, Cs...>
		auto update(VecsHandleT<P> handle, Cs&&... args) noexcept	-> bool;	///< Update one component of an entity
		
		//-------------------------------------------------------------------------
		//erase

		auto erase(VecsHandleT<P> handle, bool destruct=false) noexcept	-> bool;		///< Erase an entity from VECS
		auto clear(bool destruct = false) noexcept						-> size_t { return clearE(destruct); };	///< Clear entities of type E
		auto destruct(VecsHandleT<P> handle) noexcept					-> bool { return erase(handle, true); };	///< Mark an entity as erased
		auto destruct_all() noexcept									-> size_t { return clear(true); };	///< Mark an entity as erased

		//-------------------------------------------------------------------------
		//iterate

		auto begin() { return VecsIteratorT<P, E>{m_component_table, 0}; };
		auto end() { return VecsIteratorT<P, E>{m_component_table, m_component_table.size()}; };

		//-------------------------------------------------------------------------
		//utility

		auto size() const noexcept -> size_t { return m_sizeE.load(); };		///< \returns the number of valid entities of type E
		auto compress() noexcept -> void { compressE(); }						///< Remove erased rows from the component table
		auto capacity(size_t) noexcept								-> size_t;	///< Set max number of entities of this type
		auto swap(VecsHandleT<P> h1, VecsHandleT<P> h2) noexcept	-> bool;	///< Swap rows in component table
		auto has_value(VecsHandleT<P> handle) noexcept				-> bool;	///< \returns true if the ECS still holds this entity 
		void print_type(VecsHandleT<P> handle) {
			std::cout << "<" << typeid(E).name() << ">"; 
		};
		auto print_entities()					-> void {
			//if (size() == 0) return;
			std::cout << "<" << typeid(E).name() << ">\n";
			for (size_t i = 0; i < m_component_table.m_data.size(); ++i) {
				std::cout << i << " " << m_component_table.handle(table_index_t{ i }).m_map_index << " " << m_component_table.handle(table_index_t{ i }).m_generation_counter << "\n";
			}
		};

	};


	/**
	* \brief Update a component of an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the component type list.
	* \param[in] ptr Pointer to the data to be used.
	* \param[in] size Size of the component.
	* \param[in] move If true then the data should be moved.
	* \returns true if the update was sucessful.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::updateC(VecsHandleT<P> handle, size_t compidx, void* ptr, size_t size, bool move ) noexcept -> bool {
		if (!this->has_value(handle)) return false;
		return m_component_table.updateC(*this->table_index_ptr(handle.m_map_index), compidx, ptr, size, move);
	}

	/**
	* \brief Retrieve component data for an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the component type list.
	* \returns true if the retrieval was sucessful.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::has_componentE(VecsHandleT<P> handle, size_t compidx) noexcept -> bool {
		if (!this->has_value(handle)) return false;
		return m_component_table.has_componentE(compidx);
	}

	/**
	* \brief Retrieve pointer to component data for an entity of type E.
	* This call has been dispatched from base class using the entity type index.
	*
	* \param[in] handle The entity handle.
	* \param[in] compidx The index of the component in the component type list..
	* \returns pointer to a component.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::componentE_ptr(VecsHandleT<P> handle, size_t compidx) noexcept -> void* {
		if (!this->has_value(handle)) return nullptr;	///< Return the empty component
		return m_component_table.componentE_ptr(*this->table_index_ptr(handle.m_map_index), compidx);	///< Return the component
	};

	/**
	* \brief Insert a new entity into the ECS.
	* This call has been dispatched from base class using the parameter list.
	*
	* \param[in] args The component arguments.
	* \returns the handle of the new entity.
	*/
	template<typename P, typename E>
	template<typename... Cs>
	requires are_components_of<P, E, Cs...> [[nodiscard]]
	inline auto VecsRegistryT<P, E>::insert(Cs&&... args) noexcept	-> VecsHandleT<P> {
		std::tuple<map_index_t> tup;
		map_index_t				map_idx{};	///< index in the map
		
		if (this->m_erased.pop_back(&tup)) {						
			map_idx = std::get<0>(tup);					///< success -> Reuse old slot
		} else {
			map_idx = map_index_t{ this->m_map_table.push_back().value };	///< Create a new slot
			if (!map_idx.has_value()) return {};
			*this->counter_ptr(map_idx) = counter_t{ 0 };			//start with counter 0
		}

		*this->type_ptr(map_idx) = type_index_t{ vtll::index_of<entity_type_list, E>::value }; ///< Entity type index
		
		VecsHandleT<P> handle{ map_idx, *this->counter_ptr(map_idx) }; ///< The new handle

		*this->table_index_ptr(map_idx) 
			= m_component_table.insert(this->mutex_ptr(map_idx), handle, std::forward<Cs>(args)...);	///< add data into component table

		this->m_size++;
		this->m_sizeE++;
		return handle;
	};

	/**
	* \brief Transform an entity into a new type.
	* 
	* First all components of the old entity that can be moved to the new form are moved. Then
	* the new parameters given are moved over the components that are fitting.
	*
	* \param[in] handle Handle of the entity to transform.
	* \param[in] args The component arguments.
	* \returns the handle of the entity.
	*/
	template<typename P, typename E>
	template<typename... Cs>
	inline auto VecsRegistryT<P, E>::transform(VecsHandleT<P> handle, Cs&&... args) noexcept	-> bool {

		if (!VecsRegistryBaseClass<P>::has_value(handle)) return false;	///< See if the entity is there

		auto& map_type = *this->type_ptr(handle.m_map_index);	///< Old type index

		if (map_type == vtll::index_of<entity_type_list, E>::value) return true;	///< New type = old type -> do nothing

		auto index = m_component_table.insert(this->mutex_ptr(handle.m_map_index), handle );	///< add data into component table
			
		vtll::static_for<size_t, 0, vtll::size<E>::value >(	///< Move old components to new entity type
			[&](auto i) {
				using type = vtll::Nth_type<E, i>;
				auto ptr = componentE_ptr(handle, vtll::index_of<component_type_list, type>::value);
				if( ptr != nullptr ) {
					m_component_table.update<type>(index, std::move(*static_cast<type*>(ptr)));
				}
			}
		);

		if constexpr (sizeof...(Cs) > 0) {
			m_component_table.update<Cs>(index, std::forward<Cs>(args)...);		///< Move the arguments to the new entity type
		}

		auto& map_index = *this->table_index_ptr(handle.m_map_index);			///< Index of old component table in the map
		this->m_dispatch[map_type]->eraseE(map_index, true);					///< Erase the entity from old component table

		this->sizeE()++;
		this->m_dispatch[map_type]->sizeE()--;

		map_index = index;														///< Index in new component table
		map_type = vtll::index_of<entity_type_list,E>::value;					///< New type index

		return true;
	}
	
	/**
	* \brief Return a tuple with pointers to the components. 
	*
	* \param[in] handle Entity handle.
	* \returns tuple with pointers to the components.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::tuple_ptr(VecsHandleT<P> handle) noexcept -> vtll::to_ptr_tuple<E> {
		if (!this->has_value(handle)) return {};	///< Return the empty component
		return m_component_table.tuple_ptr(*this->table_index_ptr(handle.m_map_index));
	}

	/**
	* \brief Return a tuple with copies of the components.
	*
	* \param[in] handle Entity handle.
	* \returns tuple with copies of the components.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::tuple(VecsHandleT<P> handle) noexcept -> vtll::to_ref_tuple<E> {
		return m_component_table.tuple(*this->table_index_ptr(handle.m_map_index));
	}

	/**
	* \brief Swap the rows of two entities of the same type.
	* The call is dispatched to the correct subclass for entity type E.
	*
	* \param[in] h1 First entity.
	* \param[in] h2 Second entity.
	* \returns true if operation was successful.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::swap(VecsHandleT<P> h1, VecsHandleT<P> h2) noexcept -> bool {
		if (h1 == h2) return false;
		if (!h1.is_valid() || !h1.is_valid() || this->type(h1) != this->type(h2)) return false;
		if (this->type(h1) != vtll::index_of<entity_type_list, E>::value) return false;
		
		table_index_t& i1 = *this->table_index_ptr(h1.m_map_index);
		table_index_t& i2 = *this->table_index_ptr(h2.m_map_index);
		std::swap(i1, i2);								///< Swap in map
		auto res = m_component_table.swap(i1, i2);		///< Swap in component table
		return res;
	}


	/**
	* \brief Test whether VECS contains an entity.
	*
	* \param[in] handle The entity handle.
	* \returns true if VECS contains the entity AND the type is E.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::has_value(VecsHandleT<P> handle) noexcept -> bool {
		return VecsRegistryBaseClass<P>::has_value(handle) && handle.type() == vtll::index_of<entity_type_list, E>::value;
	}

	/**
	* \brief Retrieve pointer to a component of type C from an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \returns the pointer to the component.
	*/
	template<typename P, typename E>
	template<typename C>
	requires is_component_of<P, E, C>
	auto VecsRegistryT<P, E>::component_ptr(VecsHandleT<P> handle) noexcept -> C* {
		if (!has_value(handle)) return nullptr;	///< Return the empty component
		return m_component_table.component_ptr<C>(*this->table_index_ptr(handle.m_map_index));	///< Return the component
	}

	/**
	* \brief Update a component of type C for an entity of type E.
	*
	* \param[in] handle The entity handle.
	* \param[in] comp The component data.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename E>
	template<typename... Cs>
	requires are_components_of<P, E, Cs...>
	inline auto VecsRegistryT<P, E>::update( VecsHandleT<P> handle, Cs&&... args) noexcept -> bool {
		if (!this->has_value(handle)) return false;
		m_component_table.update(*this->table_index_ptr(handle.m_map_index), std::forward<Cs>(args)...);
		return true;
	}

	/**
	* \brief Erase an entity from the ECS.
	*
	* \param[in] handle The entity handle.
	* \returns true if the operation was successful.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::erase( VecsHandleT<P> handle, bool destruct) noexcept -> bool {
		if (!this->has_value(handle)) return false;
		m_component_table.erase(*this->table_index_ptr(handle.m_map_index), destruct);	///< Erase from comp table
		(*this->counter_ptr(handle.m_map_index))++;										///< Invalidate the entity handle

		this->m_size--;	///< Decrease sizes
		this->m_sizeE--;

		this->m_erased.push_back(handle.m_map_index);
		return true; 
	}

	/**
	* \brief Set the max number of entries that can be stored in the component table.
	*
	* If the new max is smaller than the previous one it will be ignored. Tables cannot shrink.
	*
	* \param[in] r New max number or entities that can be stored.
	* \returns the current max number, or the new one.
	*/
	template<typename P, typename E>
	inline auto VecsRegistryT<P, E>::capacity(size_t r) noexcept -> size_t {
		if( r == 0) return m_component_table.capacity(0);

		auto old_maxE = m_component_table.capacity(0);
		auto new_maxE = m_component_table.capacity(r);
		auto diff = new_maxE > old_maxE ? new_maxE - old_maxE : 0;
		if (diff > 0) {
			this->m_map_table.capacity(this->m_map_table.capacity(0) + diff);
		}
		return new_maxE;
	}


	//-------------------------------------------------------------------------
	//partial specialization for empty template parameter list

	/**
	* \brief Partial specialization for default empty entity type = alias for the base class.
	*/
	template<typename P>
	class VecsRegistryT<P, vtll::tl<>> : public VecsRegistryBaseClass<P> {};


	//-------------------------------------------------------------------------
	//left over implementations that depend on definition of classes



	//-------------------------------------------------------------------------
	//VecsComponentTable

	/**
	* \brief Remove all invalid entities from the component table.
	* 
	* The algorithm makes sure that there are no deleted entities at the end of the table.
	* Then it runs trough all deleted entities (m_destructed table) and tests whether it lies
	* inside the table. if it does, it is swapped with the last entity, which must be valid.
	* Then all invalid entities at the end are again removed.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::compress() noexcept -> void {
		VecsRegistryBaseClass<P> reg;
		std::tuple<table_index_t> tup;
		table_index_t index;

		while (m_erased.remove_back(&tup)) {
			destruct(std::get<0>(tup));
			m_destructed.push_back(std::get<0>(tup));
		}

		while (m_destructed.remove_back(&tup)) {
			remove_deleted_tail();											///< Remove invalid entities at end of table
			auto index = std::get<0>(tup);									///< Get next deleted entity from deleted table
			if (index.value < m_data.size()) {								///< Is it inside the table still?		
				m_data.move(index, table_index_t{ m_data.size() - 1 });		///< Yes, move last entity to this position
				auto handle = m_data.component<c_handle>(index).m_handle.load();		///< Handle of moved entity
				*reg.table_index_ptr(handle.m_map_index) = index;			///< Change map entry of moved last entity
			}
		}
		m_data.compress();
		m_erased.compress();
		m_destructed.compress();
	}

	/**
	* \brief Erase all entries of type E from the component table.
	* 
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the number of erased entities.
	*/
	template<typename P, typename E>
	inline auto VecsComponentTable<P, E>::clear(bool destruct) noexcept -> size_t {
		size_t num = 0;
		VecsHandleT<P> handle;
		for (size_t i = 0; i < m_data.size(); ++i) {
			handle = m_data.component<c_handle>(table_index_t{ i }).m_handle.load();
			if( handle.is_valid() && VecsRegistryT<P, E>().erase(handle, destruct) ) ++num;
		}
		return num;
	}

	//-------------------------------------------------------------------------
	//VecsRegistryBaseClass

	/**
	* \brief Constructor of class VecsRegistryBaseClass.
	* Creates the dispatch table, one entry for each VecsRegistryT<E> sub type.
	*
	* \param[in] r Max size of all entities stored in the ECS.
	*/
	template<typename P>
	inline VecsRegistryBaseClass<P>::VecsRegistryBaseClass() noexcept {
		if (!this->init()) [[likely]] return;

		vtll::static_for<size_t, 0, vtll::size<entity_type_list>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_type_list, i>;
				m_dispatch[i] = std::make_unique<VecsRegistryT<P, type>>();
			}
		);
	}

	/**
	* \brief Return the total number of valid entities of types Es
	* \returns the total number of valid entities of types Es 
	*/
	template<typename P>
	template<typename... Es>
	requires (are_entity_types<P, Es...>)
	inline auto VecsRegistryBaseClass<P>::size() const noexcept	-> size_t {
		using entity_list = std::conditional_t< (sizeof...(Es) > 0), expand_tags< typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<Es...> >, entity_type_list >;

		size_t sum = 0;
		vtll::static_for<size_t, 0, vtll::size<entity_list>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_list, i>;
				sum += VecsRegistryT<P, type>().size();
			}
		);
		return sum;
	}

	/**
	* \brief Erase all entities from the ECS.
	* 
	* The entities are not really removed, but rather invalidated. Call compress()
	* to actually remove the entities.
	* 
	* \returns the total number of erased entities.
	*/
	template<typename P>
	template<typename... Es>
	requires (are_entity_types<P, Es...>)
	inline auto VecsRegistryBaseClass<P>::clear(bool destruct) noexcept	 -> size_t {
		using entity_list = std::conditional_t< (sizeof...(Es) > 0), expand_tags< typename VecsRegistryBaseClass<P>::entity_tag_map, vtll::tl<Es...> >, entity_type_list >;

		size_t num = 0;
		vtll::static_for<size_t, 0, vtll::size<entity_list>::value >(
			[&](auto i) {
				using type = vtll::Nth_type<entity_list, i>;
				num += VecsRegistryT<P, type>().clearE(destruct);
			}
		);
		return num;
	}

	/**
	* \brief Compress all component tables. This removes all invalidated entities.
	*/
	template<typename P>
	inline auto VecsRegistryBaseClass<P>::compress() noexcept		-> void {
		vtll::static_for<size_t, 0, vtll::size<entity_type_list>::value >(
			[&](auto i) {
				VecsRegistryT<P, vtll::Nth_type<entity_type_list, i>>().compressE();
			}
		);
	}

	//-------------------------------------------------------------------------
	//VecsHandleT<P>

	/**
	* \brief Check whether the data in a handle is non null
	* \returns true if the data in the handle is not null
	*/
	template<typename P>
	inline auto VecsHandleT<P>::is_valid() noexcept				-> bool {
		return m_map_index.has_value() && m_generation_counter.has_value();
	}

	/**
	* \brief Check whether a handle belongs to an entity that is still in the ECS.
	* \returns true if the entity this handle belongs to is still in the ECS.
	*/
	template<typename P>
	inline auto VecsHandleT<P>::has_value() noexcept			-> bool {
		return VecsRegistryBaseClass<P>::m_registry.has_value(*this);
	}

	/**
	* \brief Test whether an entity of type E contains a component of type C.
	* \returns true if the entity type E has a component C
	*/
	template<typename P>
	template<typename C>
	requires is_component_type<P, C>
	inline auto VecsHandleT<P>::has_component() noexcept		-> bool {
		return VecsRegistryBaseClass<P>::m_registry.has_component<C>(*this);
	}

	/**
	* \brief Get the component of type C from the entity this handle belongs to.
	* \returns the component, or an empty component.
	*/
	template<typename P>
	template<typename C>
	requires is_component_type<P, C>
	inline auto VecsHandleT<P>::component_ptr() noexcept		-> C* {
		return VecsRegistryBaseClass<P>::m_registry.component_ptr<C>(*this);
	}

	/**
	* \brief Update a component of type C for the entity that this handle belongs to.
	*
	* \param[in] comp Universal reference to the component data.
	* \returns true if the operation was successful.
	*/
	template<typename P>
	template<typename... Cs>
	requires are_component_types<P, Cs...>
	inline auto VecsHandleT<P>::update(Cs&&... args) noexcept	-> bool {
		return VecsRegistryBaseClass<P>::m_registry.update(*this, std::forward<Cs>(args)...);
	}

	/**
	* \brief Erase the entity this handle belongs to, from the ECS.
	*
	* \returns true if the operation was successful.
	*/
	template<typename P>
	inline auto VecsHandleT<P>::erase(bool destruct) noexcept				-> bool {
		return VecsRegistryBaseClass<P>::m_registry.erase(*this, destruct);
	}

	/**
	* \brief Get index of this entity in the component table.
	*
	* \returns index of this entity in the component table.
	*/
	template<typename P>
	inline auto VecsHandleT<P>::type() noexcept			-> type_index_t {
		return VecsRegistryBaseClass<P>::m_registry.type(*this);
	}

	/**
	* \brief Return a pointer to the mutex of this entity.
	*
	* \returns a pointer to the mutex of this entity.
	*/
	template<typename P>
	inline auto VecsHandleT<P>::mutex_ptr() noexcept				-> std::atomic<uint32_t>* {
		if (!is_valid()) return nullptr;
		return VecsRegistryBaseClass<P>::m_registry.mutex_ptr(m_map_index);
	}

}

#include "VECSIterator.h"

#endif

