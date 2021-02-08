#ifndef VECOMPONENT_H
#define VECOMPONENT_H

#include <limits>
#include <typeinfo>
#include <typeindex>
#include "glm.hpp"
#include "gtc/quaternion.hpp"
#include "VEUtil.h"
#include "VEContainer.h"

namespace vve {

	enum class VeComponentType {
		Position,
		Orientation,
		Transform,
		Material,
		Geometry,
		Last
	};

	template<typename T, auto ID>
	struct VeComponent {
		using type = std::integral_constant<std::size_t, static_cast<std::size_t>(ID)>;
	};

	struct VePosition : VeComponent<VePosition, VeComponentType::Position> {
		glm::vec3 m_position;
	};

	struct VeOrientation : VeComponent<VeOrientation, VeComponentType::Orientation> {
		glm::quat m_orientation;
	};

	struct VeTransform : VeComponent<VeTransform, VeComponentType::Transform> {
		glm::mat4 m_transform;
	};

	struct VeMaterial : VeComponent<VeMaterial, VeComponentType::Material> {
	};

	struct VeGeometry : VeComponent<VeGeometry, VeComponentType::Geometry> {
	};

	using VeComponentTypeList = type_list<VePosition, VeOrientation, VeTransform, VeMaterial, VeGeometry>;

}


#include "VeComponentUser.h" 

namespace vve {

	using VeComponentPtr = variant_type<to_ptr<VeComponentTypeList>>;

}

#endif

