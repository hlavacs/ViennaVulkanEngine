#pragma once



namespace ph {

	struct phEdge {
		glm::vec3 points[2];

		phEdge() {};
		phEdge(glm::vec3 p0, glm::vec3 p1) {
			points[0] = p0;
			points[1] = p1;
		}
	};

	struct phPlane {
		glm::vec3 normal;			///<normalized normal vector of the plane
		float d;					///<distance of the plane to origin along the normal

		phPlane() {};
		phPlane(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2) {
			glm::vec3 d1 = p1 - p0;
			glm::vec3 d2 = p2 - p1;
			normal = glm::normalize(glm::cross(d1, d2));
			d = glm::dot(normal, p0);
		};
	};

	struct phQuad {
		glm::vec3 points[4];
		struct phPlane plane;

		phQuad() {};
		phQuad( glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3 ) {
			points[0] = p0;
			points[1] = p1;
			points[2] = p2;
			points[3] = p3;

			plane = phPlane( p0, p1, p2 );
		}
	};

	struct phSphere {
		glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);	///<center of the sphere
		float radius;									///<sphere radius
	};


	struct phHalfspace {
		struct phPlane plane;		///<plane intersecting the two half spaces
		int32_t sign;				///<either +1 or -1
	};

	struct phFrustum {
		glm::vec3 vertices[8];		///<near plane and far plane points
		struct phQuad quads[6];		///<6 quads bounding the frustum		

		phFrustum( glm::vec3 vert[8] ) {
			for (uint32_t i = 0; i < 8; i++) vertices[i] = vert[i];

			quads[0] = phQuad(vert[0], vert[1], vert[2], vert[3]);	//near plane
			quads[1] = phQuad(vert[7], vert[6], vert[5], vert[4]);	//far plane

			quads[2] = phQuad(vert[0], vert[3], vert[7], vert[4]);	//left
			quads[3] = phQuad(vert[2], vert[1], vert[5], vert[6]);	//right

			quads[4] = phQuad(vert[3], vert[2], vert[6], vert[7]);	//top
			quads[5] = phQuad(vert[1], vert[0], vert[4], vert[5]);	//bottom
		};
	};


	bool phIntersect(glm::vec3 &p, phQuad & q);
	bool phIntersect(glm::vec3 &p, phSphere & s);
	bool phIntersect(glm::vec3 &p, phHalfspace &h);
	bool phIntersect(glm::vec3 &p, phFrustum &f);

	bool phIntersect(phEdge &e, phSphere & s);
	bool phIntersect(phEdge &e, phHalfspace &h);
	bool phIntersect(phEdge &e, phFrustum &f);

	bool phIntersect(phQuad &e, phSphere & s);
	bool phIntersect(phQuad &e, phHalfspace &h);
	bool phIntersect(phQuad &e, phFrustum &f);

	bool phIntersect(phSphere &s0, phSphere &s1);
	bool phIntersect(phSphere &s, phPlane &p);
	bool phIntersect(phSphere &s, phFrustum &f);

};


