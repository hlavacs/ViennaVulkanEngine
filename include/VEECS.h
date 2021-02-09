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
	using VeEntityAnimation = VeEntity<VeComponentMaterial, VeComponentGeometry>;
	//...

	using VeEntityTypeList = tl::cat<tl::type_list<
			VeEntity<>
			, VeEntityNode
			, VeEntityDraw
			, VeEntityAnimation
			// ,... 
		>
		, VeEntityTypeListUser
	>;
	using VeEntityPtr = tl::variant_type<tl::to_ptr<tl::transform<VeEntityTypeList, VeEntity>>>;


	//-------------------------------------------------------------------------
	//entity handle

	template<typename T>
	struct VeHandle_t {
		index_t		m_index{};		//the slot of the entity in the entity list
		counter_t	m_counter{};	//generation counter
	};

	using VeHandle = tl::variant_type<tl::transform<VeEntityTypeList, VeHandle_t>>;


	//-------------------------------------------------------------------------
	//component pool

	template<typename T>
	class VeComponentPool : public crtp<VeComponentPool<T>, VeComponentPool> {
	protected:
		using base_crtp		= crtp<VeComponentPool<T>, VeComponentPool>;
		using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;

		static inline VeSlotMap<T, VeHandle_t<T>> m_component;

	public:
		VeComponentPool() = default;
		void add(VeHandle&& h, T&& component);
	};

	template<typename T>
	inline void VeComponentPool<T>::add(VeHandle&& h, T&& component) {
		int i = 0;
	}

	using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;

	inline VeComponentPool<VeComponentPosition> p1;
	inline VeComponentPoolPtr pp1{ &p1 };


	//-------------------------------------------------------------------------
	//systems

	template<typename T >
	class VeComponentReferencePool : public crtp<VeComponentReferencePool<T>, VeComponentReferencePool> {
	protected:
		using base_crtp = crtp<VeComponentReferencePool<T>, VeComponentReferencePool>;
		using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;
		using tuple_type = typename tl::to_ref_tuple<T>::type;

		struct VeRefPoolEntry {
			tuple_type m_entry;
			index_t m_next{};
		};

		static inline std::vector<VeRefPoolEntry>	m_ref_index;
		static inline index_t						m_first_free{};

	public:
		VeComponentReferencePool(size_t r);

		template<typename T >
		requires std::is_same_v<std::decay_t<T>, tuple_type>
		index_t add(VeHandle& h, T&& ref ) {
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
	};


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
		using base_system = VeSystem<VeEntityManager>;

		struct VeEntityData {
			VeHandle	m_handle{};	//entity handle
			index_t		m_next{};	//next free slot or index of reference pool
		};

		static inline std::vector<VeEntityData> m_entity;
		static inline index_t m_first_free{};

	public:
		VeEntityManager(size_t reserve = 1 << 10);

		template<typename T, typename... Ts>
		requires tl::is_same<T, Ts...>::value
		VeHandle create(T&& e, Ts&&... args);

		template<typename T>
		void erase(VeHandle& h);
	};


	template<typename T, typename... Ts>
	requires tl::is_same<T, Ts...>::value
	inline VeHandle VeEntityManager::create(T&& e, Ts&&... args) {
		index_t idx{};
		if (!m_first_free.is_null()) {
			idx = m_first_free;
			m_first_free = m_entity[m_first_free.value].m_next;
		}
		else {
			idx.value = m_entity.size();	//index of new entity
			m_entity.push_back({}); //start with counter 0
		}
		VeHandle_t<T> h{ idx, counter_t{0} };
		(VeComponentPool<Ts>().add(VeHandle_t{ h }, std::forward<Ts>(args)), ...);
		return { h };
	};


	template<typename T>
	inline void VeEntityManager::erase(VeHandle& handle) {
		auto erase_handle = [this]<typename T>(T & h) {
			VeComponentReferencePool<T>().erase(h.m_next);
		};

		std::visit( erase_handle, handle);
	}


}

#endif
