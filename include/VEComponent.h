#ifndef VECOMPONENT_H
#define VECOMPONENT_H

#include "VETypeList.h"


namespace vve {

	//-------------------------------------------------------------------------
	//components

	template<typename T>
	struct VeComponent {};

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

#endif

