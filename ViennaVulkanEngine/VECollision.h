#ifndef CLINCLUDE_H
#define CLINCLUDE_H

/**
*
* \file
* \brief
*
* Details
*
*/


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>


namespace vve::cl {


	///An edge connects 2 points 
	struct clEdge {
		glm::vec3 points[2];	///<2 edge points

		///Constructor of struct clEdge
		clEdge() {};
		///Constructor of struct clEdge
		clEdge(glm::vec3 p0, glm::vec3 p1) {
			points[0] = p0;
			points[1] = p1;
		}
	};

	///A plane consists of a normalized normal vector and a distance from the origin
	struct clPlane {
		glm::vec3 normal;			///<normalized normal vector of the plane
		float d;					///<distance of the plane to origin along the normal

		///Constructor of struct clPlane
		clPlane() {};
		///Constructor of struct clPlane
		clPlane(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) {	//construct from 3 points
			glm::vec3 d1 = p1 - p0;
			glm::vec3 d2 = p2 - p1;
			normal = glm::normalize(glm::cross(d1, d2));	//normal vector
			d = glm::dot(normal, p0);						//can use any point on the plane
		};
	};

	///A quad consists of 4 points connected by 4 edges. It also defines a plane.
	struct clQuad {
		glm::vec3 points[4];		///<4 points defining a quad, must lie on the same plane
		struct clPlane plane;		///<The plane defined by the 4 points

		///Constructor of struct clQuad
		clQuad() {};
		///Constructor of struct clQuad
		clQuad(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3) {
			points[0] = p0;
			points[1] = p1;
			points[2] = p2;
			points[3] = p3;

			plane = clPlane(p0, p1, p2);
		}
	};

	///A sphere consists of a center and a radius
	struct clSphere {
		glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);	///<center of the sphere
		float radius;									///<sphere radius
	};

	////A halfspace is defined by a plane cutting space in two half planes,
	///and a sign identifying which half is meant. The normal vector
	///points to the positive half.
	struct clHalfspace {
		struct clPlane plane;		///<plane intersecting the two half spaces
		int32_t sign;				///<either +1 or -1
	};

	///A frustum consist of 8 points, and 6 quads
	struct clFrustum {
		glm::vec3 vertices[8];		///<near plane and far plane points
		struct clQuad quads[6];		///<6 quads bounding the frustum		

		///Constructor of struct clFrustum
		clFrustum(glm::vec3 vert[8]) {
			for (uint32_t i = 0; i < 8; i++) vertices[i] = vert[i];

			quads[0] = clQuad(vert[0], vert[1], vert[2], vert[3]);	//near plane
			quads[1] = clQuad(vert[5], vert[4], vert[7], vert[6]);	//far plane

			quads[2] = clQuad(vert[0], vert[3], vert[7], vert[4]);	//left
			quads[3] = clQuad(vert[2], vert[1], vert[5], vert[6]);	//right

			quads[4] = clQuad(vert[3], vert[2], vert[6], vert[7]);	//top
			quads[5] = clQuad(vert[1], vert[0], vert[4], vert[5]);	//bottom
		};
	};









	//---------------------------------------------------------------------
	//Intersection tests

	bool clIntersect(glm::vec3 &p, clQuad & q);
	bool clIntersect(glm::vec3 &p, clSphere & s);
	bool clIntersect(glm::vec3 &p, clHalfspace &h);
	bool clIntersect(glm::vec3 &p, clFrustum &f);

	bool clIntersect(clEdge &e, clSphere & s);
	bool clIntersect(clEdge &e, clHalfspace &h);
	bool clIntersect(clEdge &e, clFrustum &f);

	bool clIntersect(clQuad &e, clSphere & s);
	bool clIntersect(clQuad &e, clHalfspace &h);
	bool clIntersect(clQuad &e, clFrustum &f);

	bool clIntersect(clSphere &s0, clSphere &s1);
	bool clIntersect(clSphere &s, clPlane &p);
	bool clIntersect(clSphere &s, clFrustum &f);
};


#endif
