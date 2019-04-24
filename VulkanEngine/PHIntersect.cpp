
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>


#include "PHShape.h"


namespace ph {

	//------------------------------------------------------------------------

	bool phIntersect(glm::vec3 &p, phQuad & q) {

		phQuad nq1 = phQuad(q.points[0], q.points[1],
							q.points[1] + q.plane.normal, q.points[0] + q.plane.normal);
		phQuad nq2 = phQuad(q.points[1], q.points[2],
							q.points[2] + q.plane.normal, q.points[1] + q.plane.normal);
		phQuad nq3 = phQuad(q.points[2], q.points[3],
							q.points[3] + q.plane.normal, q.points[2] + q.plane.normal);
		phQuad nq4 = phQuad(q.points[3], q.points[0],
							q.points[0] + q.plane.normal, q.points[3] + q.plane.normal);

		struct phHalfspace halfspaces[4] = {
			{ nq1.plane, 1 },
			{ nq2.plane, 1 },
			{ nq3.plane, 1 },
			{ nq4.plane, 1 }
		};

		return	phIntersect(p, halfspaces[0]) &&
				phIntersect(p, halfspaces[1]) &&
				phIntersect(p, halfspaces[2]) &&
				phIntersect(p, halfspaces[3]);
	}

	bool phIntersect(glm::vec3 &p, phSphere & s) {
		glm::vec3 diff = s.center - p;
		if (glm::dot(diff, diff) <= s.radius*s.radius) return true;
		return false;
	};

	bool phIntersect(glm::vec3 &p, phHalfspace &h) {
		float ord = glm::dot(p, h.plane.normal);
		int sign = signbit(ord) ? -1 : 1;
		return (sign == h.sign);
	}

	bool phIntersect(glm::vec3 &p, phFrustum &f) {
		struct phHalfspace halfspaces[6] = {
			{ f.quads[0].plane, 1 },
			{ f.quads[1].plane, 1 },
			{ f.quads[2].plane, 1 },
			{ f.quads[3].plane, 1 },
			{ f.quads[4].plane, 1 },
			{ f.quads[5].plane, 1 }
		};
		return	phIntersect(p, halfspaces[0]) &&
			phIntersect(p, halfspaces[1]) &&
			phIntersect(p, halfspaces[2]) &&
			phIntersect(p, halfspaces[3]) &&
			phIntersect(p, halfspaces[4]) &&
			phIntersect(p, halfspaces[5]);
	}


	//------------------------------------------------------------------------

	bool phIntersect(phEdge &e, phSphere & s) {

		glm::vec3 diff = e.points[1] - e.points[0];		//vector from p0 to p1
		glm::vec3 center = s.center - e.points[0];		//use p0 as reference point
		float ordc = glm::dot(diff, center);			//ordinate of center along diff vector
		glm::vec3 res = center - ordc * diff;			//residual when subtracting projection

		if (glm::dot(res, res) > s.radius*s.radius)		//does the sphere touch the line?
			return false;

		if (phIntersect(e.points[0], s) || phIntersect(e.points[1], s)) //sphere touches any vertex?
			return true;

		if (ordc >= 0.0f && ordc <= 1.0) return true;	//sphere touches line segment?

		return false;									//touch is outside the edge points
	}


	bool phIntersect(phEdge &e, phHalfspace &h) {
		return true;
	}

	bool phIntersect(phEdge &e, phFrustum &f) {
		return true;
	}


	//------------------------------------------------------------------------

	bool phIntersect(phQuad &q, phSphere & s) {

		if (!phIntersect(s, q.plane)) return false;		//does the sphere touch the quad plane?

		for (uint32_t i = 0; i < 4; i++) {					//is a vertex in the sphere?
			if (phIntersect(q.points[i], s)) return true;
		}

		if (phIntersect(phEdge(q.points[0], q.points[1]), s)) return true;	//collision with one of the quad edges?
		if (phIntersect(phEdge(q.points[1], q.points[2]), s)) return true;
		if (phIntersect(phEdge(q.points[2], q.points[3]), s)) return true;
		if (phIntersect(phEdge(q.points[3], q.points[0]), s)) return true;

		return phIntersect(s.center, q );
	}

	bool phIntersect(phQuad &e, phHalfspace &h) {
		for (uint32_t i = 0; i < 4; i++) {
			if (phIntersect(e.points[i], h)) return true;
		}
		return false;
	}

	bool phIntersect(phQuad &e, phFrustum &f) {
		return true;
	}


	//------------------------------------------------------------------------

	bool phIntersect(phSphere &s0, phSphere &s1) {
		glm::vec3 diff = s0.center - s1.center;
		float sumRad = s0.radius + s1.radius;
		if (glm::dot(diff, diff) <= sumRad*sumRad) return true;
		return false;
	}

	bool phIntersect(phSphere &s, phPlane &p) {
		float ordc = glm::dot( s.center, p.normal );
		if (fabs(ordc - p.d) <= s.radius) return true;
		return false;
	}

	bool phIntersect(phSphere &s, phFrustum &f) {

		if (phIntersect(s.center, f)) return true;	//is sphere center in the frustum?

		for (uint32_t i = 0; i < 6; i++) {
			if (phIntersect(f.quads[i], s)) 		//collision of a quad with the sphere?
				return true;
		}

		return false;
	}


};


