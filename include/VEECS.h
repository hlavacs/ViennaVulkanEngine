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
	struct VeEntity;

	template <>
	struct VeEntity<>;
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

	template<typename T>
	class VeComponentPool : crtp<VeComponentPool<T>, VeComponentPool> {
	protected:
		using base_crtp		= crtp<VeComponentPool<T>, VeComponentPool>;
		using VeComponentPoolPtr = tl::variant_type<tl::to_ptr<tl::transform<VeComponentTypeList, VeComponentPool>>>;
		static inline std::vector<T> m_data;

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

	template<typename T, typename Seq = tl::type_list<>>
	class VeSystem : public crtp<T, VeSystem> {
	protected:
	public:
		VeSystem() = default;
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


	//-------------------------------------------------------------------------
	//entity manager

	template<typename T>
	class VeEntityManager : public VeSystem<VeEntityManager<T>> {
	protected:
		using base_crtp		= crtp<VeEntityManager<T>, VeEntityManager>;
		using base_system	= VeSystem<VeComponentPool<T>>;

		static inline VeTable<VeEntityData> m_entity;

	public:
		VeEntityManager(size_t reserve = 1 << 10);
		VeHandle	create();
		void		erase(VeHandle& h);
	};

}

#endif
