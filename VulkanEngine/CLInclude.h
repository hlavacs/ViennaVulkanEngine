#ifndef CLINCLUDE_H
#define CLINCLUDE_H

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>

#include "CLShape.h"

namespace cl
{
	//---------------------------------------------------------------------
	// Intersection tests

	bool clIntersect(glm::vec3 &p, clQuad &q);

	bool clIntersect(glm::vec3 &p, clSphere &s);

	bool clIntersect(glm::vec3 &p, clHalfspace &h);

	bool clIntersect(glm::vec3 &p, clFrustum &f);

	bool clIntersect(clEdge &e, clSphere &s);

	bool clIntersect(clEdge &e, clHalfspace &h);

	bool clIntersect(clEdge &e, clFrustum &f);

	bool clIntersect(clQuad &e, clSphere &s);

	bool clIntersect(clQuad &e, clHalfspace &h);

	bool clIntersect(clQuad &e, clFrustum &f);

	bool clIntersect(clSphere &s0, clSphere &s1);

	bool clIntersect(clSphere &s, clPlane &p);

	bool clIntersect(clSphere &s, clFrustum &f);
}; // namespace cl

#endif
