#ifndef VECOMPONENT_H
#define VECOMPONENT_H

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VETypeList.h"


namespace vve {

	//-------------------------------------------------------------------------
	//component types

	struct VeComponent {};

	struct VeComponentPosition : VeComponent {
		glm::vec3 m_position;
	};

	struct VeComponentOrientation : VeComponent {
		glm::quat m_orientation;
	};

	struct VeComponentTransform : VeComponent {
		glm::mat4 m_transform;
	};

	struct VeComponentMaterial : VeComponent {
	};

	struct VeComponentGeometry : VeComponent {
	};

	struct VeComponentAnimation : VeComponent {
	};

	struct VeComponentCollisionShape : VeComponent {
	};

	struct VeComponentRigidBody : VeComponent {
	};


	//-------------------------------------------------------------------------
	//entity types

	template <typename... Ts>
	using VeEntityType = tl::type_list<Ts...>;


	//-------------------------------------------------------------------------
	//engine entity types

	using VeEntityTypeNode = VeEntityType<VeComponentPosition, VeComponentOrientation, VeComponentTransform>;
	using VeEntityTypeDraw = VeEntityType<VeComponentMaterial, VeComponentGeometry>;
	using VeEntityTypeAnimation = VeEntityType<VeComponentAnimation>;
	//...

	template<typename T>
	struct VeHandle_t;
}

#endif

