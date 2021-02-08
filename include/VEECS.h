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

	enum class VeComponentType : uint32_t {
		Position,
		Orientation,
		Transform,
		Material,
		Geometry,
		Animation,
		CollisionShape,
		Body,
		Last
	};

	template<typename T, auto ID>
	struct VeComponent {
		using type = std::integral_constant<std::size_t, static_cast<std::size_t>(ID)>;
	};

	struct VeComponentPosition : VeComponent<VeComponentPosition, VeComponentType::Position> {
		glm::vec3 m_position;
	};

	struct VeComponentOrientation : VeComponent<VeComponentOrientation, VeComponentType::Orientation> {
		glm::quat m_orientation;
	};

	struct VeComponentTransform : VeComponent<VeComponentTransform, VeComponentType::Transform> {
		glm::mat4 m_transform;
	};

	struct VeComponentMaterial : VeComponent<VeComponentMaterial, VeComponentType::Material> {
	};

	struct VeComponentGeometry : VeComponent<VeComponentGeometry, VeComponentType::Geometry> {
	};

	struct VeComponentAnimation : VeComponent<VeComponentAnimation, VeComponentType::Animation> {
	};

	struct VeComponentCollisionShape : VeComponent<VeComponentCollisionShape, VeComponentType::CollisionShape> {
	};

	struct VeComponentBody : VeComponent<VeComponentBody, VeComponentType::Body> {
	};

	//-------------------------------------------------------------------------
	//entities

	template <typename... Ts>
	struct VeEntity {};

	template <>
	struct VeEntity<> {};
}

//user defined component types and entity types
#include "VEECSUser.h" 


namespace vve {

	//-------------------------------------------------------------------------
	//component type list, pointer, pool

	using VeComponentTypeList = tl::cat<tl::type_list<
		  VeComponentPosition, VeComponentOrientation, VeComponentTransform, VeComponentMaterial, VeComponentGeometry
		, VeComponentAnimation, VeComponentCollisionShape, VeComponentBody >, 	VeComponentTypeListUser>;
	using VeComponentPtr = tl::variant_type<tl::to_ptr<VeComponentTypeList>>;

	template<typename T>
	class VeComponentPool {
	protected:
		using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;
		static inline std::vector<T> m_data;
		static inline std::atomic<uint32_t> m_init_counter = 0;

	public:
		VeComponentPool();
	};

	using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;

	inline VeComponentPool<VeComponentPosition> p1;
	inline VeComponentPoolPtr pp1{ &p1 };


	//-------------------------------------------------------------------------
	//entity type list and pointer

	using VeEntityNode = VeEntity<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityDraw = VeEntity<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityAnimation = VeEntity<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeList = tl::cat<tl::type_list<VeEntity<>, VeEntityNode, VeEntityDraw, VeEntityAnimation>, VeEntityTypeListUser>;
	using VeEntityPtr = tl::variant_type<tl::to_ptr<tl::transform<VeEntityTypeList, VeEntity>>>;


	//-------------------------------------------------------------------------
	//systems use CRTP

	template<typename... Ts>
	class VeSystem;

	template<typename T>
	class VeSystem<T> {
	protected:
		//auto derived = static_cast<T&>(*this);
		static inline std::atomic<uint32_t> m_init_counter = 0;
	public:
		VeSystem();
	};



	struct VeEntityData;

	struct VeHandle {
		VeEntityData* m_ptr = nullptr;
		uint32_t	  m_counter = 0;
	};

	/*struct VeEntityData {
		uint32_t	  m_counter = 0;
		VeEntityData* m_next = nullptr;
	};

	struct VeComponentHandle {
		VeComponentPoolPtr m_pool;
		index_t m_index;
		VeHandle m_entity;
	};*/


	template <typename Seq>
	struct VeEntityManager;

	template<template <typename...> typename Seq, typename... Ts>
	class VeEntityManager<Seq<Ts...>> {
	public:
		VeEntityManager( size_t reserve) {
			if (m_init_counter > 0) return;
			auto cnt = m_init_counter.fetch_add(1);
			if (cnt > 0) return;
			m_data.reserve(1<<10);
		};
		std::optional<VeHandle> create();
		void erase(VeHandle& h);

	protected:
		static inline std::atomic<uint32_t> m_init_counter = 0;
		static inline VeTable<VeEntityData> m_data;
		static inline std::atomic<bool>		m_init = false;
	};

}

#endif
