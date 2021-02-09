#ifndef VEECS_H
#define VEECS_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include <variant>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VGJS.h"
#include "VEContainer.h"
#include "VEUtil.h"

namespace vve {

	//-------------------------------------------------------------------------
	//components

	template<typename T>
	struct VeComponent : crtp<T, VeComponent> {
	};

	struct VeComponentPosition : VeComponent<VeComponentPosition> {
		glm::vec3 m_position;
	};

	struct VeComponentOrientation : VeComponent<VeComponentOrientation> {
		glm::quat m_orientation;
	};

	struct VeComponentTransform : VeComponent<VeComponentTransform> {
		glm::mat4 m_transform;
	};

	struct VeComponentMaterial : VeComponent<VeComponentMaterial> {
	};

	struct VeComponentGeometry : VeComponent<VeComponentGeometry> {
	};

	struct VeComponentAnimation : VeComponent<VeComponentAnimation> {
	};

	struct VeComponentCollisionShape : VeComponent<VeComponentCollisionShape> {
	};

	struct VeComponentBody : VeComponent<VeComponentBody> {
	};

	//-------------------------------------------------------------------------
	//entities

	template <typename... Ts>
	using VeEntity = tl::type_list<Ts...>;

	template<typename T>
	struct VeHandle_t;
}

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
			, VeComponentBody 
			//, ...
		>,
		VeComponentTypeListUser
	>;
	using VeComponentPtr = tl::variant_type<tl::to_ptr<VeComponentTypeList>>;


	//-------------------------------------------------------------------------
	//entity type list and pointer

	using VeEntityNode = VeEntity<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityDraw = VeEntity<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityAnimation = VeEntity<VeComponentAnimation>;
	//...

	using VeEntityTypeList_1 = tl::type_list<
		VeEntity<>
		, VeEntityNode
		, VeEntityDraw
		, VeEntityAnimation
		// ,... 
	>;

	using VeEntityTypeList = tl::cat< VeEntityTypeList_1, VeEntityTypeListUser >;
	using VeEntityPtr = tl::variant_type<tl::to_ptr<tl::transform<VeEntityTypeList, VeEntity>>>;


	//-------------------------------------------------------------------------
	//entity handle

	template<typename E>
	struct VeHandle_t {
		index_t		m_index{ typeid(std::decay_t<E>).hash_code() };		//the slot of the entity in the entity list
		counter_t	m_counter{};	//generation counter
	};

	using VeHandle = tl::variant_type<tl::transform<VeEntityTypeList, VeHandle_t>>;


	//-------------------------------------------------------------------------
	//component pool

	template<typename C>
	class VeComponentVector : public crtp<VeComponentVector<C>, VeComponentVector> {
	protected:

		struct entry_t {
			C		 m_component;
			VeHandle m_handle;
			index_t& m_ref_index;
		};

		static inline std::vector<entry_t> m_component;

	public:
		VeComponentVector() = default;
		void add(VeHandle&& h, C&& component);
		void erase(VeHandle& h, index_t comp_index);
	};

	template<typename C>
	inline void VeComponentVector<C>::add(VeHandle&& h, C&& component) {
		int i = 0;
	}

	template<typename C>
	inline void VeComponentVector<C>::erase(VeHandle& h, index_t comp_index) {
		if (m_component.size() == 0) return;
		if (comp_index.value < m_component.size()-1) {
			m_component[comp_index.value] = m_component[m_component.size() - 1];
			m_component[comp_index.value].m_ref_index = comp_index;
		}
		m_component.pop_back();
	}


	using VeComponentVectorPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentVector>>>;

	inline VeComponentVector<VeComponentPosition> p1;
	inline VeComponentVectorPtr pp1{ &p1 };


	//-------------------------------------------------------------------------
	//systems

	template<typename E>
	class VeComponentReferenceTable : public crtp<VeComponentReferenceTable<E>, VeComponentReferenceTable> {
	protected:
		using tuple_type = typename tl::to_ref_tuple<E>::type;

		struct entry_t {
			tuple_type m_entry;
			index_t m_next{};
		};

		static inline std::vector<entry_t>	m_ref_index;
		static inline index_t				m_first_free{};

	public:
		VeComponentReferenceTable(size_t r = 1 << 10);

		template<typename U >
		requires std::is_same_v<std::decay_t<U>, typename VeComponentReferenceTable<E>::tuple_type>
		index_t add(VeHandle& h, U&& ref);
		tuple_type& get(index_t index);
		void erase(index_t idx);
	};


	template<typename T>
	inline VeComponentReferenceTable<T>::VeComponentReferenceTable(size_t r) {
		if (!this->init()) return;
		m_ref_index.reserve(r);
	};


	template<typename E>
	template<typename U>
	requires std::is_same_v<std::decay_t<U>, typename VeComponentReferenceTable<E>::tuple_type>
	index_t VeComponentReferenceTable<E>::add(VeHandle& h, U&& ref) {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_ref_index[m_first_free.value].m_next;
		}
		else {
			idx.value = m_ref_index.size();	//index of new entity
			m_ref_index.push_back({}); //start with counter 0
		}
		return idx;
	};

	template<typename E>
	inline typename VeComponentReferenceTable<E>::tuple_type& VeComponentReferenceTable<E>::get(index_t index) {
		return m_ref_index[index.value].m_entry;
	}

	template<typename E>
	void VeComponentReferenceTable<E>::erase(index_t index) {
		m_ref_index[index.value].m_next = m_first_free;
		m_first_free = index;
	}


	//-------------------------------------------------------------------------
	//system

	template<typename T, typename Seq = tl::type_list<>>
	class VeSystem : public crtp<T, VeSystem> {
	protected:
	public:
		VeSystem() = default;
	};


	//-------------------------------------------------------------------------
	//entity manager

	class VeEntityManager : public VeSystem<VeEntityManager> {
	protected:

		struct entry_t {
			VeHandle	m_handle{};	//entity handle
			index_t		m_index{};	//next free slot or index of reference table
		};

		static inline std::vector<entry_t>	m_entity_table;
		static inline index_t				m_first_free{};

		index_t get_ref_pool_index(VeHandle& h);

	public:
		VeEntityManager(size_t reserve = 1 << 10);

		template<typename E, typename... Ts>
		requires tl::is_same<E, Ts...>::value
		VeHandle create(E&& e, Ts&&... args);

		void erase(VeHandle& h);
	};


	inline VeEntityManager::VeEntityManager(size_t r) : VeSystem() {
		if (!this->init()) return;
		m_entity_table.reserve(r);
	}


	inline index_t VeEntityManager::get_ref_pool_index(VeHandle& h) {
		return {};
	}


	template<typename E, typename... Ts>
	requires tl::is_same<E, Ts...>::value
	inline VeHandle VeEntityManager::create(E&& e, Ts&&... args) {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_entity_table[m_first_free.value].m_index;
		}
		else {
			idx.value = m_entity_table.size();	//index of new entity
			m_entity_table.push_back({}); //start with counter 0
		}
		VeHandle_t<E> h{ idx, counter_t{0} };
		(VeComponentVector<Ts>().add(VeHandle{ h }, std::forward<Ts>(args)), ...);
		return { h };
	};


	inline void VeEntityManager::erase(VeHandle& handle) {
		auto erase_components = [&]<typename... Ts>( std::tuple<Ts&...>& reftup, VeHandle_t<VeEntity<Ts...>> &h) {
			
			//(VeComponentVector<Ts>().erase(h), ...);
		};

		auto erase_references = [&]<typename E>(VeHandle_t<E> & h) {
			VeComponentReferenceTable<E> reftable;
			erase_components(reftable.get(m_entity_table[h.m_index.value].m_index), h);
			reftable.erase(m_entity_table[h.m_index.value].m_index);
		};

		std::visit(erase_references, handle);
	}


}

#endif
