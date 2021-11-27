

#ifndef CLINTERSECT_H
#define CLINTERSECT_H

#include "CLInclude.h"

namespace cl
{
	//------------------------------------------------------------------------

	/**
		*
		* \brief Test whether the projection of a point to a quad plane intersects with the quad
		*
		* The point does not have to be on the quad plane. Instead it is projected onto the quadplane
		* first. Then it is tested whether the projection intersects with the quad.
		*
		* \param[in] p Point in 3D space
		* \param[in] q Quad consisting of 4 vertex points
		* \returns whether the point projection to the quad plane intersects with the quad
		*
		*/
	bool clIntersect(glm::vec3 &p, clQuad &q)
	{
		clQuad nq1 = clQuad(q.points[0], q.points[1],
			q.points[1] + q.plane.normal, q.points[0] + q.plane.normal);
		clQuad nq2 = clQuad(q.points[1], q.points[2],
			q.points[2] + q.plane.normal, q.points[1] + q.plane.normal);
		clQuad nq3 = clQuad(q.points[2], q.points[3],
			q.points[3] + q.plane.normal, q.points[2] + q.plane.normal);
		clQuad nq4 = clQuad(q.points[3], q.points[0],
			q.points[0] + q.plane.normal, q.points[3] + q.plane.normal);

		struct clHalfspace halfspaces[4] = {
			{nq1.plane, 1},
			{nq2.plane, 1},
			{nq3.plane, 1},
			{nq4.plane, 1} };

		return clIntersect(p, halfspaces[0]) &&
			clIntersect(p, halfspaces[1]) &&
			clIntersect(p, halfspaces[2]) &&
			clIntersect(p, halfspaces[3]);
	}

	/**
		*
		* \brief Test whether a point and a sphere intersect
		*
		* \param[in] p Point in 3D space
		* \param[in] s Sphere
		* \returns whether point p lies in sphere s
		*
		*/
	bool clIntersect(glm::vec3 &p, clSphere &s)
	{
		glm::vec3 diff = s.center - p;
		return (glm::dot(diff, diff) <= s.radius * s.radius);
	};

	/**
		*
		* \brief Tests whether a point lies in a halfspace.
		*
		* \param[in] p Point in 3D space
		* \param[in] h Halfspace
		* \returns whether the point p lies in halfspace h
		*
		*/
	bool clIntersect(glm::vec3 &p, clHalfspace &h)
	{
		float ord = glm::dot(p, h.plane.normal);
		int sign = std::signbit(ord) ? -1 : 1;
		return (sign == h.sign);
	}

	/**
		*
		* \brief Tests whether a point intersects with a frustum
		*
		* \param[in] p Point in 3D space
		* \param[in] f Frustum
		* \returns whether the point p lies in frustum f
		*
		*/
	bool clIntersect(glm::vec3 &p, clFrustum &f)
	{
		struct clHalfspace halfspaces[6] = {
			{f.quads[0].plane, 1},
			{f.quads[1].plane, 1},
			{f.quads[2].plane, 1},
			{f.quads[3].plane, 1},
			{f.quads[4].plane, 1},
			{f.quads[5].plane, 1} };
		return clIntersect(p, halfspaces[0]) &&
			clIntersect(p, halfspaces[1]) &&
			clIntersect(p, halfspaces[2]) &&
			clIntersect(p, halfspaces[3]) &&
			clIntersect(p, halfspaces[4]) &&
			clIntersect(p, halfspaces[5]);
	}

	//------------------------------------------------------------------------

	/**
		* \brief Tests whether an edge and a sphere intersect
		*
		* \param[in] e Edge (line segment) in 3D space
		* \param[in] s Sphere
		* \returns whether edge e and sphere s intersect
		*
		*/
	bool clIntersect(clEdge &e, clSphere &s)
	{
		glm::vec3 diff = e.points[1] - e.points[0]; //vector from p0 to p1
		glm::vec3 center = s.center - e.points[0]; //use p0 as reference point
		float ordc = glm::dot(diff, center); //ordinate of center along diff vector
		glm::vec3 res = center - ordc * diff; //residual when subtracting projection

		if (glm::dot(res, res) > s.radius * s.radius) //does the sphere touch the line?
			return false;

		if (clIntersect(e.points[0], s) || clIntersect(e.points[1], s)) //Sphere touches any vertex?
			return true;

		if (ordc >= 0.0f && ordc <= 1.0)
			return true; //sphere touches line segment?

		return false; //touch is outside the edge points
	}

	/**
		* \brief
		*
		* \param[in]
		* \param[in]
		* \returns whether
		*
		*/
	bool clIntersect(clEdge &e, clHalfspace &h)
	{
		return true;
	}

	/**
		* \brief
		*
		* \param[in]
		* \param[in]
		* \returns whether
		*
		*/
	bool clIntersect(clEdge &e, clFrustum &f)
	{
		return true;
	}

	//------------------------------------------------------------------------

	/**
		* \brief Tests whether a quad and a sphere intersect
		*
		* \param[in] q Quad given as 4 points
		* \param[in] s Sphere
		* \returns whether the quad q intersects with sphere s
		*
		*/
	bool clIntersect(clQuad &q, clSphere &s)
	{
		if (!clIntersect(s, q.plane))
			return false; //does the sphere touch the quad plane?

		for (uint32_t i = 0; i < 4; i++)
		{ //is a vertex in the sphere?
			if (clIntersect(q.points[i], s))
				return true;
		}

		//if (clIntersect(clEdge(q.points[0], q.points[1]), s)) return true;	//intersection with one of the quad edges?
		//if (clIntersect(clEdge(q.points[1], q.points[2]), s)) return true;
		//if (clIntersect(clEdge(q.points[2], q.points[3]), s)) return true;
		//if (clIntersect(clEdge(q.points[3], q.points[0]), s)) return true;

		return clIntersect(s.center, q); //intersection between projected center and quad in plane?
	}

	/**
		* \brief Tests whether a quad intersects with a half space
		*
		* \param[in] q Quad given as 4 points
		* \param[in] h Halfspace
		* \returns whether the quad q intersects with halfspace h
		*
		*/
	bool clIntersect(clQuad &q, clHalfspace &h)
	{
		for (uint32_t i = 0; i < 4; i++)
		{ //test if any of the quad vertices is in
			if (clIntersect(q.points[i], h))
				return true; //the halfspace
		}
		return false;
	}

	/**
		* \brief
		*
		* \param[in]
		* \param[in]
		* \returns whether
		*
		*/
	bool clIntersect(clQuad &e, clFrustum &f)
	{
		return true;
	}

	//------------------------------------------------------------------------

	/**
		* \brief Tests whether a sphere intersects with another sphere
		*
		* \param[in] s0 First sphere
		* \param[in] s1 Second sphere
		* \returns whether the two spheres intersect
		*
		*/
	bool clIntersect(clSphere &s0, clSphere &s1)
	{
		glm::vec3 diff = s0.center - s1.center;
		float sumRad = s0.radius + s1.radius;
		return (glm::dot(diff, diff) <= sumRad * sumRad);
	}

	/**
		* \brief Tests whether a sphere intersects with a plane
		*
		* \param[in] s Sphere
		* \param[in] p Plane
		* \returns whether sphere s intersects with plane p
		*
		*/
	bool clIntersect(clSphere &s, clPlane &p)
	{
		float ordc = glm::dot(s.center, p.normal); //distance of sphere center to origin along normal
		return (fabs(ordc - p.d) <= s.radius); //d ist the same for the plane
	}

	/**
		* \brief Tests whether a sphere intersects with a frustum
		*
		* \param[in] s Sphere
		* \param[in] f Frustum
		* \returns whether sphere s intersects with frustum f
		*
		*/
	bool clIntersect(clSphere &s, clFrustum &f)
	{
		if (clIntersect(s.center, f))
			return true; //is sphere center in the frustum?

		for (uint32_t i = 0; i < 6; i++)
		{
			if (clIntersect(f.quads[i], s)) //collision of a quad with the sphere?
				return true;
		}

		return false;
	}

}; // namespace cl

#endif
