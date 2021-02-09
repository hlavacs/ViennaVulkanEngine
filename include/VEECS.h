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
			, VeComponentBody >,
		VeComponentTypeListUser>;
	using VeComponentPtr = tl::variant_type<tl::to_ptr<VeComponentTypeList>>;

	//-------------------------------------------------------------------------
	//entity type list and pointer

	using VeEntityNode = VeEntity<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityDraw = VeEntity<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityAnimation = VeEntity<VeComponentMaterial, VeComponentGeometry>;

	using VeEntityTypeList = tl::cat<tl::type_list<VeEntity<>, VeEntityNode, VeEntityDraw, VeEntityAnimation>, VeEntityTypeListUser>;
	using VeEntityPtr = tl::variant_type<tl::to_ptr<tl::transform<VeEntityTypeList, VeEntity>>>;

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
	//systems use CRTP

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
			counter_t	m_counter{0};	//generation counter
			index_t		m_next{};		//next free slot
		};

		static inline std::vector<VeEntityData> m_entity;
		static inline index_t m_next_free{};

	public:
		VeEntityManager(size_t reserve = 1 << 10);

		template<typename T, typename... Ts>
		requires tl::is_same<T, Ts...>::value
		VeHandle_t<T> create(T&& e, Ts&&... args);

		template<typename T>
		void erase(VeHandle_t<T>& h);
	};

	template<typename T, typename... Ts>
	requires tl::is_same<T, Ts...>::value
	inline VeHandle_t<T> VeEntityManager::create(T&& e, Ts&&... args) {
		index_t idx{};
		if (!m_next_free.is_null()) {
			idx = m_next_free;
			m_next_free = m_entity[m_next_free.value].m_next;
		}
		else {
			idx.value = m_entity.size();	//index of new entity
			m_entity.push_back({}); //start with counter 0
		}
		VeHandle_t<T> h{ idx, counter_t{0} };
		(VeComponentPool<Ts>().add(VeHandle_t{ h }, std::forward<Ts>(args)), ...);
		return h;
	};

}

#endif
