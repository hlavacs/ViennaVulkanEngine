#ifndef VECOMPONENT_H
#define VECOMPONENT_H

#include "glm.hpp"
#include "gtc/quaternion.hpp"

#include "VETypeList.h"


namespace vve {

	//-------------------------------------------------------------------------
	//component types

	struct VeComponent {};

	struct VeComponentPosition {
		glm::vec3 m_position;
	};

	struct VeComponentOrientation {
		glm::quat m_orientation;
	};

	struct VeComponentTransform {
		glm::mat4 m_transform;
	};

	struct VeComponentMaterial {
	};

	struct VeComponentGeometry {
	};

	struct VeComponentAnimation {
	};

	struct VeComponentCollisionShape {
	};

	struct VeComponentRigidBody {
	};


	//-------------------------------------------------------------------------
	//entity types

	template <typename... Ts>
	struct VeEntityType {};


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

