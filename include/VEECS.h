#ifndef VEECS_H
#define VEECS_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <variant>
#include "VGJS.h"
#include "VEUtil.h"
#include "VETypeList.h"
#include "VEComponent.h"

//user defined component types and entity types
#include "VEECSUser.h" 


namespace vve {

	//-------------------------------------------------------------------------
	//component type list, pointer, pool

	using VeComponentTypeList = tl::cat<tl::type_list<
			  VeComponentPosition
			, VeComponentOrientation
			, VeComponentTransform
			, VeComponentMaterial
			, VeComponentGeometry
			, VeComponentAnimation
			, VeComponentCollisionShape
			, VeComponentRigidBody
			//, ...
		>
		, VeComponentTypeListUser	//components defined by the user
	>;
	using VeComponentTypePtr = tl::variant_type<tl::to_ptr<VeComponentTypeList>>;


	//-------------------------------------------------------------------------
	//entity type list and pointer

	using VeEntityTypeNode		= VeEntityType<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw		= VeEntityType<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = VeEntityType<VeComponentAnimation>;
	//...

	using VeEntityTypeList = tl::cat< tl::type_list<
			VeEntityType<>
			, VeEntityTypeNode
			, VeEntityTypeDraw
			, VeEntityTypeAnimation
			// ,... 
		>
		, VeEntityTypeListUser >;


	//-------------------------------------------------------------------------
	//entity handle

	template<typename E>
	struct VeHandle_t {
		index_t		m_entity_index{};			//the slot of the entity in the entity list
		counter_t	m_generation_counter{};		//generation counter
	};

	using VeHandle = tl::variant_type<tl::transform<VeEntityTypeList, VeHandle_t>>;


	template <typename E>
	struct VeEntity_t {
		using tuple_type = typename tl::to_tuple<E>::type;
		VeHandle	m_handle;
		tuple_type	m_tuple;

		template<typename C>
		C& get() {
			return std::get<tl::index_of<C, E>::value>(m_tuple);
		};

		template<typename C>
		void set(C&& comp ) {
			std::get<tl::index_of<C, E>::value>(m_tuple) = comp;
		};

		std::string name() {
			return typeid(E).name();
		};
	};

	using VeEntityTypePtr = tl::variant_type<tl::to_ptr<tl::transform<VeEntityTypeList, VeEntity_t>>>;

	using VeEntity = tl::variant_type<tl::transform<VeEntityTypeList, VeEntity_t>>;


	//-------------------------------------------------------------------------
	//component vector

	class VeEntityManager;

	template<typename C>
	class VeComponentVector : public VeMonostate {
	protected:
		friend VeEntityManager;

		struct entry_t {
			C			m_component;
			counter_t	m_generation_counter;
			index_t*	m_map_pointer;
		};

		static inline std::vector<entry_t> m_component_vector;

	public:
		VeComponentVector() = default;
		index_t	add( counter_t, C&& component );
		C		get( counter_t, index_t comp_index);
		void	erase(counter_t, index_t comp_index);
	};


	template<typename C>
	inline index_t VeComponentVector<C>::add(counter_t counter, C&& component) {
		m_component_vector.push_back({ component, counter, nullptr });
		return index_t{ static_cast<typename index_t::type_name>(m_component_vector.size() - 1) };
	}


	template<typename C>
	C VeComponentVector<C>::get(counter_t counter, index_t comp_index) {
		assert(counter == m_component_vector[comp_index.value].m_generation_counter);
		return m_component_vector[comp_index.value].m_component;
	}


	template<typename C>
	inline void VeComponentVector<C>::erase(counter_t counter, index_t comp_index) {
		if (m_component_vector.size() == 0) return;
		if (comp_index.value < m_component_vector.size()-1) {
			assert(counter == m_component_vector[comp_index.value].m_generation_counter);
			m_component_vector[comp_index.value] = m_component_vector[m_component_vector.size() - 1];
			*m_component_vector[comp_index.value].m_map_pointer = comp_index;
		}
		m_component_vector.pop_back();
	}


	using VeComponentVectorPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentVector>>>;

	inline VeComponentVector<VeComponentPosition> p1;
	inline VeComponentVectorPtr pp1{ &p1 };


	//-------------------------------------------------------------------------
	//references to components - each entity has them

	template<typename E>
	class VeComponentMapTable : public VeMonostate {
	public:
		using tuple_type = typename tl::N_tuple<index_t, tl::size_of<E>::value>::type;

	protected:
		struct entry_t {
			tuple_type	m_index_tuple;
			index_t		m_next{};
		};

		static inline std::vector<entry_t>	m_index_component;
		static inline index_t				m_first_free{};

	public:
		VeComponentMapTable(size_t r = 1 << 10);

		std::tuple<tuple_type&, index_t> add();
		tuple_type&						 get(index_t index);
		void							 erase(index_t idx);
	};


	template<typename E>
	inline VeComponentMapTable<E>::VeComponentMapTable(size_t r) {
		if (!this->init()) return;
		m_index_component.reserve(r);
	};


	template<typename E>
	std::tuple<typename VeComponentMapTable<E>::tuple_type&, index_t> VeComponentMapTable<E>::add() {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_index_component[m_first_free.value].m_next;
		}
		else {
			idx.value = static_cast<typename index_t::type_name>(m_index_component.size());			//
			m_index_component.push_back({ {}, {} });	//
		}
		return std::make_tuple( std::ref(m_index_component[idx.value].m_index_tuple), idx );
	};


	template<typename E>
	inline typename VeComponentMapTable<E>::tuple_type& VeComponentMapTable<E>::get(index_t index) {
		return m_index_component[index.value].m_index_tuple;
	}


	template<typename T>
	void VeComponentMapTable<T>::erase(index_t index) {
		m_index_component[index.value].m_next = m_first_free;
		m_first_free = index;
	}


	//-------------------------------------------------------------------------
	//system

	template<typename T, typename VeSystemComponentTypeList = tl::type_list<>>
	class VeSystem : public VeMonostate, public VeCRTP<T, VeSystem> {
	protected:
	public:
		VeSystem() = default;
	};


	//-------------------------------------------------------------------------
	//entity manager is a system, and thus uses CRTP

	class VeEntityManager : public VeSystem<VeEntityManager> {
	protected:

		struct entry_t {
			counter_t			m_generation_counter{0};	//generation counter starts with 0
			index_t				m_next_free_or_map_index{};	//next free slot or index of reference table
			VeReadWriteMutex	m_mutex;					//per entity synchronization

			entry_t() {};
			entry_t(const entry_t& other) {};
			entry_t& operator=(const entry_t& other) {};
		};

		static inline std::vector<entry_t>	m_entity_table;
		static inline index_t				m_first_free{};

	public:
		VeEntityManager(size_t reserve = 1 << 10);

		template<typename E, typename... Ts>
		requires tl::is_same<E, Ts...>::value
		VeHandle insert(Ts&&... args);

		//------------------------------------------------------------

		template<typename T>
		VeEntity get(T& handle);

		template<typename E>
		VeEntity_t<E> get(VeHandle& h);

		template<>
		VeEntity get<>(VeHandle& h);

		//------------------------------------------------------------

		template<typename T>
		void erase(T& handle);

		template<typename E>
		void erase(VeHandle& handle);

		template<>
		void erase<>(VeHandle& handle);
	};
	

	inline VeEntityManager::VeEntityManager(size_t r) : VeSystem() {
		if (!this->init()) return;
		m_entity_table.reserve(r);
	}


	template<typename E, typename... Ts>
	requires tl::is_same<E, Ts...>::value
	inline VeHandle VeEntityManager::insert(Ts&&... args) {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_entity_table[m_first_free.value].m_next_free_or_map_index;
		}
		else {
			idx.value = static_cast<typename index_t::type_name>(m_entity_table.size());	//index of new entity
			m_entity_table.emplace_back();		//start with counter 0
		}

		VeHandle_t<E> handle{ idx, m_entity_table[idx.value].m_generation_counter };
		auto [map, mapidx]	= VeComponentMapTable<E>().add();										//add map data, a tuple of index_t
		m_entity_table[idx.value].m_next_free_or_map_index = mapidx;	//index in component map 
		auto tup			= std::make_tuple( VeComponentVector<Ts>().add( handle.m_generation_counter, std::forward<Ts>(args) )... ); //tuple with indices of the components

		tl::static_for<int, 0, sizeof...(Ts) >(
			[&](auto i) { 
				std::get<i>(map) = std::get<i>(tup);
				using type = tl::Nth_type<i, E>; //  typename std::tuple_element<i, std::tuple<Ts...>>::type;
				VeComponentVector<type>().m_component_vector[std::get<i>(tup).value].m_map_pointer = &std::get<i>(map);
			} 
		);
		using type = tl::Nth_type<0, E>;
		auto comp = VeComponentVector<type>().m_component_vector[std::get<0>(tup).value];

		return {handle};
	};


	template<typename E>
	inline VeEntity_t<E> VeEntityManager::get( VeHandle& h) {
		VeHandle_t<E> handle = std::get<VeHandle_t<E>>(h);
		VeEntity_t<E> e;
		VeComponentMapTable<E> map;
		auto mapidx = m_entity_table[handle.m_entity_index.value].m_next_free_or_map_index;
		auto indextup = map.get(mapidx);

		tl::static_for<size_t, 0, tl::size_of<E>::value >(
			[&](auto i) {
				using type = tl::Nth_type<i, E>;
				e.set(VeComponentVector<type>().get(handle.m_generation_counter, std::get<i>(indextup)));
			}
		);

		return e;
	}


	template<>
	inline VeEntity VeEntityManager::get(VeHandle& handle) {
		VeEntity e;

		std::cout << tl::size_of<VeEntityTypeList>::value << std::endl;

		tl::static_for<size_t, 0, tl::size_of<VeEntityTypeList>::value >(
			[&](auto i) {
				using type = tl::Nth_type<i, VeEntityTypeList>;
				if (std::holds_alternative<VeHandle_t<type>>(handle)) {
					e = VeEntity{ get<type>(handle) };
				}
			}
		);
		return e;
	}


	template<typename E>
	inline void VeEntityManager::erase(VeHandle& h) {
		VeHandle_t<E> handle = std::get<VeHandle_t<E>>(h);
		
		m_entity_table[handle.m_entity_index.value].m_generation_counter.value++;		//>invalidate the entity handle

		VeComponentMapTable<E> map;
		auto mapidx = m_entity_table[handle.m_entity_index.value].m_next_free_or_map_index;
		auto indextup = map.get(mapidx);

		tl::static_for<size_t, 0, tl::size_of<E>::value >(
			[&](auto i) {
				using type = tl::Nth_type<i, E>;
				VeComponentVector<type>().erase( handle.m_generation_counter, std::get<i>(indextup) );
			}
		);

		map.erase(mapidx);
	}


	template<>
	inline void VeEntityManager::erase<>(VeHandle& handle) {

		tl::static_for<size_t, 0, tl::size_of<VeEntityTypeList>::value >(
			[&](auto i) {
				using type = tl::Nth_type<i, VeEntityTypeList>;
				if (std::holds_alternative<VeHandle_t<type>>(handle)) {
					erase<type>(handle);
				}
			}
		);
	}


}

#endif
