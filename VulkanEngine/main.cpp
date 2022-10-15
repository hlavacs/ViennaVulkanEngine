/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include <algorithm>
#include <vector>
#include <cstdio>
#include <iterator>
#include <ranges>
#include <string>
#include <iostream>
#include <cmath>
#include <cstdlib>

#include "VEInclude.h"

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"
#include "glm/gtx/matrix_operation.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/matrix_cross_product.hpp"


//If DOUBLE_ACCURACY is defined then computations are done with double accuracy. 
//If SINGLE_ACCURACY is defined then use single accuracy
//Time is always in double.

#define SINGLE_ACCURACY
//#define DOUBLE_ACCURACY

#ifdef DOUBLE_ACCURACY
using real = double;
using int_t = int64_t;
using uint_t = uint64_t;
#define glmvec2 glm::dvec2
#define glmvec3 glm::dvec3
#define glmmat3 glm::dmat3
#define glmvec4 glm::dvec4
#define glmmat4 glm::dmat4
#define glmquat glm::dquat
const real c_eps = 1.0e-12;
#else
#ifdef SINGLE_ACCURACY
using real = float;
using int_t = int32_t;
using uint_t = uint32_t;
#define glmvec2 glm::vec2
#define glmvec3 glm::vec3
#define glmvec4 glm::vec4
#define glmmat3 glm::mat3
#define glmmat4 glm::mat4
#define glmquat glm::quat
const real c_eps = 1.0e-8f;
#else
#error Must choose accuracy!
#endif

#endif
constexpr real operator "" _real(long double val) { return (real)val; };	//define _real 


//Pairs of data
using intpair_t = std::pair<int_t, int_t>;		//Pair of integers
using voidppair_t = std::pair<void*, void*>;	//Pair of void pointers

//Algorithms from namespace geometry, defined below this file
namespace geometry {
	std::pair<glmvec3, glmvec3> closestPointsLineLine(glmvec3 p1, glmvec3 p2, glmvec3 p3, glmvec3 p4);
	real distancePointLine(glmvec3 p, glmvec3 a, glmvec3 b);
	real distancePointLinesegment(glmvec3 p, glmvec3 a, glmvec3 b);
	void computeBasis(const glmvec3& a, glmvec3& b, glmvec3& c);

	template<typename T>
	void SutherlandHodgman(T& subjectPolygon, T& clipPolygon, T& newPolygon);
}


//-------------------------------------------------------------------------------------------------------------
//Hash functions for storing stuff in maps

template <typename T>
inline void hash_combine(std::size_t& seed, T const& v) {		//For combining hashes
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
	template <>
	struct hash<intpair_t> {
		std::size_t operator()(const intpair_t& p) const {
			size_t seed = std::hash<int_t>()(p.first);
			hash_combine(seed, p.second);
			return seed;
		}
	};

	template <>
	struct hash<voidppair_t> {
		std::size_t operator()(const voidppair_t& p) const {
			size_t seed = std::hash<void*>()(std::min(p.first,p.second));	//Makes sure the smaller one is always first
			hash_combine(seed, std::max(p.first, p.second));				//Larger one second
			return seed;
		}
	};
	template<>
	struct equal_to<voidppair_t> {
		constexpr bool operator()(const voidppair_t& l, const voidppair_t& r) const {
			return (l.first == r.first && l.second == r.second) || (l.first == r.second && l.second == r.first);
		}
	};

	template <>
	struct hash<glmvec2> {
		std::size_t operator()(const glmvec2& p) const {
			size_t seed = std::hash<real>()(p.x);
			hash_combine(seed, p.y);
			return seed;
		}
	};
	template<>
	struct less<glmvec2> {
		bool operator()(const glmvec2& l, const glmvec2& r) const {
			return (std::hash<glmvec2>()(l) < std::hash<glmvec2>()(r));
		}
	};

	//For outputting vectors/matrices to a string stream
	ostream& operator<<(ostream& os, const glmvec3& v) {
		os << "(" << v.x << ',' << v.y << ',' << v.z << ")";				//output 3D vector
		return os;
	}

	ostream& operator<<(ostream& os, const glmquat& q) {
		os << "(" << q.x << ',' << q.y << ',' << q.z << ',' << q.w << ")";	//output quaternion
		return os;
	}

	ostream& operator<<(ostream& os, const glmmat3& m) {
		os << "(" << m[0][0] << ',' << m[0][1] << ',' << m[0][2] << ")\n";	//Output a 3x3 matrix
		os << "(" << m[1][0] << ',' << m[1][1] << ',' << m[1][2] << ")\n";
		os << "(" << m[2][0] << ',' << m[2][1] << ',' << m[2][2] << ")\n";
		return os;
	}

	std::string to_string(const glmvec3 v) {
		return std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z);	//Turn vector into a string
	}
}

//-------------------------------------------------------------------------------------------------------------

//These are short hand notations to save on typing for transforming points/vectors from one coordinate space into another. 
//Naming follows this code:
//R...coordinate space of reference object
//I...coordinate space of incident object
//RT...tangent space of a face of the reference object
//W...world space
//P...point
//V...vector
//N...normal vector
#define ITORP(X) glmvec3{contact.m_body_inc.m_to_other * glmvec4{X, 1.0_real}}
#define ITORV(X) glmmat3{contact.m_body_inc.m_to_other} * (X)
#define ITORN(X) contact.m_body_inc.m_to_other_it * (X)
#define ITORTP(X) glmvec3{face_ref->m_LtoT * contact.m_body_inc.m_to_other * glmvec4{X, 1.0_real}}
#define ITTOWP(X) glmvec3{contact.m_body_inc.m_body->m_model * face_inc->m_TtoL * glmvec4{X, 1.0_real}}

#define ITOWP(X) glmvec3{contact.m_body_inc.m_body->m_model * glmvec4{X, 1.0_real}}
#define ITOWN(X) contact.m_body_inc.m_body->m_model_it * (X)

#define RTOIP(X) glmvec3{contact.m_body_ref.m_to_other * glmvec4{X, 1.0_real}}
#define RTOIN(X) contact.m_body_ref.m_to_other_it*(X)
#define RTOWP(X) glmvec3{contact.m_body_ref.m_body->m_model * glmvec4{X,1.0_real}}
#define RTOWN(X) contact.m_body_ref.m_body->m_model_it*(X)
#define RTORTP(X) glmvec3{face_ref->m_LtoT * glmvec4{X, 1.0_real}}

#define WTOTIP(X) glmvec3{face_inc->m_LtoT * contact.m_body_inc.m_body->m_model_inv * glmvec4{X, 1.0_real} }
#define WTORN(X)  glmmat3{ contact.m_body_ref.m_body->m_model_inv } * (X)
#define WTOIN(X)  glmmat3{ contact.m_body_inc.m_body->m_model_inv } * (X)

#define RTTORP(X) glmvec3{face_ref->m_TtoL * glmvec4{(X), 1.0_real}}
#define RTTOWP(X) glmvec3{contact.m_body_ref.m_body->m_model * face_ref->m_TtoL * glmvec4{(X), 1.0_real}}


//-------------------------------------------------------------------------------------------------------------


namespace ve {

	static std::default_random_engine rnd_gen{ 12345 };					//Random numbers
	static std::uniform_real_distribution<> rnd_unif{ 0.0f, 1.0f };		//Random numbers

	/// <summary>
	/// This is a callback that is called in each loop. It implements a simple rigid body 
	/// physics engine.
	/// </summary>
	class VEEventListenerPhysics : public VEEventListener {

	public:

		//--------------------------------------------------------------------------------------------------
		//constants
		const real		c_gravity = -9.81_real;				//Gravity acceleration
		const double	c_small = 0.01_real;				//A small value
		const double	c_very_small = c_small / 50.0_real;	//Even smaller value

		//--------------------------------------------------------------------------------------------------
		//Basic geometric objects

		/// <summary>
		/// This struct stores forces that can act on bodies.
		/// </summary>
		struct Force {
			glmvec3 m_accelW{ 0,0,0 };	//Acceleration in world space (default is earth gravity)
			glmvec3 m_positionL{ 0,0,0 };	//Position in local space
			glmvec3 m_forceL{ 0,0,0 };	//Force vector in local space
			glmvec3 m_forceW{ 0,0,0 };	//Force vector in world space 
		};

		struct Face;
		struct Edge;

		/// <summary>
		/// This struct holds information about a vertex of a polytope.
		/// </summary>
		struct Vertex {
			uint_t			m_id;				//Unique number within a polytope
			glmvec3			m_positionL;		//vertex position in local space
			std::set<Face*>	m_vertex_face_ptrs;	//pointers to faces this vertex belongs to
			std::set<Edge*>	m_vertex_edge_ptrs;	//pointers to edges this vertex belongs to
		};

		/// <summary>
		/// Holds information about an edge in the polytope. An edge connects two vertices.
		/// </summary>
		struct Edge {
			uint_t			m_id;					//Unique number within a polytope
			Vertex& m_first_vertexL;		//first edge vertex
			Vertex& m_second_vertexL;		//second edge vertex
			glmvec3			m_edgeL{};				//edge vector in local space 
			std::set<Face*>	m_edge_face_ptrs{};		//pointers to the two faces this edge belongs to
		};

		/// <summary>
		/// A signed edge refers to an edge, but can use a factor (-1) to invert the edge vector.
		/// This gives a unique winding order of edges belonging to a face.
		/// This struct is used to construct the polytope.
		/// </summary>
		struct signed_edge_t {
			uint32_t m_edge_idx;
			real	 m_factor{ 1.0_real };
		};

		/// <summary>
		/// Used to store the signed edge in a container.
		/// </summary>
		using SignedEdge = std::pair<Edge*, real>;

		/// <summary>
		/// A polytope consists of a number of faces. Each face consists of a sequence of edges,
		/// with the endpoint of the last edge being the starting point of the first edge.
		/// </summary>
		struct Face {
			uint_t					m_id;						//Unique number within a polytope
			std::vector<Vertex*>	m_face_vertex_ptrs{};		//pointers to the vertices of this face in correct orientation
			std::vector<glmvec2>	m_face_vertex2D_T{};	//vertex 2D coordinates in tangent space
			std::vector<SignedEdge>	m_face_edge_ptrs{};			//pointers to the edges of this face and orientation factors
			glmvec3					m_normalL{};				//normal vector in local space
			glmmat4					m_LtoT;						//local to face tangent space
			glmmat4					m_TtoL;						//face tangent to local space
		};

		/// <summary>
		/// Find the face of a polytope whose normal vector is maximally aligned with a given vector.
		/// There are two cases: we are either only interested into the absolute values, or we
		/// are interested into the true values. To decide this you can specify a function 
		/// for cvalculating this.
		/// </summary>
		/// <typeparam name="T">C++ class holding alist of faces. Can be face or edge.</typeparam>
		/// <param name="dirL">Direction of vector in local space</param>
		/// <param name="faces">A vector of pointers to faces.</param>
		/// <param name="fct">Can either be f(x)=x (default) or f(x)=fabs(x).</param>
		/// <returns></returns>
		template<typename T>
		Face* maxFaceAlignment(const glmvec3 dirL, const T& faces, real(*fct)(real) = [](real x) { return x; }) {
			assert(faces.size() > 0);
			auto compare = [&](Face* a, Face* b) { return fct(glm::dot(dirL, a->m_normalL)) < fct(glm::dot(dirL, b->m_normalL)); };
			return *std::ranges::max_element(faces, compare);
		}

		/// <summary>
		/// Find the edge of a polytope whose normal vector is minimally aligned with a given vector.
		/// </summary>
		/// <typeparam name="T">Class holding a list of pointers to faces.</typeparam>
		/// <param name="dirL">The vector in local space.</param>
		/// <param name="signed_edges"></param>
		/// <returns></returns>
		template<typename T>
		Edge* minEdgeAlignment(const glmvec3 dirL, const T& signed_edges) {
			assert(signed_edges.size() > 0);
			auto compare = [&](Edge* a, Edge* b) { return fabs(glm::dot(dirL, a->m_edgeL)) < fabs(glm::dot(dirL, b->m_edgeL)); };
			return *std::ranges::min_element(signed_edges, compare);
		}

		struct Collider {};		//Base class for all classes that can collide

		/// <summary>
		/// A polytope is a convex polyhedron. It consists of faces, made of edges, with each edge connecting two vertices.
		/// </summary>
		struct Polytope : public Collider {
			std::vector<Vertex>	m_vertices{};		//positions of vertices in local space
			std::vector<Edge>	m_edges{};			//list of edges
			std::vector<Face>	m_faces{};			//list of faces

			real m_bounding_sphere_radius{ 1.0_real };	//Bounding sphere radius for a quick contact test

			using inertia_tensor_t = std::function<glmmat3(real, glmvec3&)>;
			inertia_tensor_t inertiaTensor;			//Function for computing the inertia tensor of this polytope

			/// <summary>
			/// Constructor for Polytope. Take in a list of vector positions and indices and create the polygon data struct.
			/// </summary>
			/// <param name="vertices">Vertex positions in local space.</param>
			/// <param name="edgeindices">One pair of vector indices for each edge. </param>
			/// <param name="face_edge_indices">For each face a list of pairs edge index - orientation factor (1.0 or -1.0)</param>
			/// <param name="inertia_tensor"></param>
			Polytope(const std::vector<glmvec3>& vertices,
				const std::vector<std::pair<uint_t, uint_t>>&& edgeindices,
				const std::vector < std::vector<signed_edge_t> >&& face_edge_indices,
				inertia_tensor_t inertia_tensor)
				: Collider{}, m_vertices{}, m_edges{}, m_faces{}, inertiaTensor{ inertia_tensor } {

				//add vertices
				uint_t id = 0;
				m_vertices.reserve(vertices.size());
				std::ranges::for_each(vertices, [&](const glmvec3& v) {
					m_vertices.emplace_back(id++, v);
					if (real l = glm::length(v) > m_bounding_sphere_radius) m_bounding_sphere_radius = l;
					}
				);

				//add edges
				id = 0;
				m_edges.reserve(edgeindices.size());
				for (auto& edgepair : edgeindices) {	//compute edges from indices
					Edge* edge = &m_edges.emplace_back(id++, m_vertices[edgepair.first], m_vertices[edgepair.second], m_vertices[edgepair.second].m_positionL - m_vertices[edgepair.first].m_positionL);
					m_vertices[edgepair.first].m_vertex_edge_ptrs.insert(edge);
					m_vertices[edgepair.second].m_vertex_edge_ptrs.insert(edge);
				}

				//add faces
				id = 0;
				m_faces.reserve(face_edge_indices.size());
				for (auto& face_edge_idx : face_edge_indices) {	//compute faces from edge indices belonging to this face
					assert(face_edge_idx.size() >= 2);
					auto& face = m_faces.emplace_back(id++);	//add new face to face vector

					for (auto& edge : face_edge_idx) {		//add references to the edges belonging to this face
						Edge* pe = &m_edges.at(edge.m_edge_idx);
						face.m_face_edge_ptrs.push_back(SignedEdge{ pe, edge.m_factor });	//add new face to face vector
						if (edge.m_factor > 0) { face.m_face_vertex_ptrs.push_back(&pe->m_first_vertexL); } //list of vertices of this face
						else { face.m_face_vertex_ptrs.push_back(&pe->m_second_vertexL); }

						m_edges[edge.m_edge_idx].m_edge_face_ptrs.insert(&face);	//we touch each face only once, no need to check if face already in list
						m_edges[edge.m_edge_idx].m_first_vertexL.m_vertex_face_ptrs.insert(&face);	//sets cannot hold duplicates
						m_edges[edge.m_edge_idx].m_second_vertexL.m_vertex_face_ptrs.insert(&face);
					}
					glmvec3 edge0 = m_edges[face_edge_idx[0].m_edge_idx].m_edgeL * face_edge_idx[0].m_factor;
					glmvec3 edge1 = m_edges[face_edge_idx[1].m_edge_idx].m_edgeL * face_edge_idx[1].m_factor;

					glmvec3 tangent = glm::normalize(edge0);						//tangent space coordinate axes
					face.m_normalL = glm::normalize(glm::cross(tangent, edge1));	//for clipping this face againts another face
					glmvec3 bitangent = glm::cross(face.m_normalL, tangent);

					//Transform from local space to tangent space and back
					face.m_TtoL = glm::translate(glmmat4(1), face.m_face_vertex_ptrs[0]->m_positionL) * glmmat4 { glmmat3{ bitangent, face.m_normalL, tangent } };
					face.m_LtoT = glm::inverse(face.m_TtoL);

					for (auto& vertex : face.m_face_vertex_ptrs) {	//precompute tangent space coordinates of face's vertices
						glmvec4 pt = face.m_LtoT * glmvec4{ vertex->m_positionL, 1.0_real };
						face.m_face_vertex2D_T.emplace_back(pt.x, pt.z);
					}
				}
			};
		};

		/// <summary>
		/// This is the template for each cube. It contains the basic geometric properties of a cube:
		/// Vertices, edges and faces.
		/// </summary>
		Polytope g_cube{
			{ { -0.5_real,-0.5_real,-0.5_real }, { -0.5_real,-0.5_real,0.5_real }, { -0.5_real,0.5_real,0.5_real }, { -0.5_real,0.5_real,-0.5_real },
			{ 0.5_real,-0.5_real,0.5_real }, { 0.5_real,-0.5_real,-0.5_real }, { 0.5_real,0.5_real,-0.5_real }, { 0.5_real,0.5_real,0.5_real } },
			{ {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,0}, {5,0}, {1,4}, {3,6}, {7,2} }, //edges
			{	{ {0}, {1}, {2}, {3} },							//face 0
				{ {4}, {5}, {6}, {7} },							//face 1
				{ {0,-1.0}, {8,-1.0}, {4,-1.0}, {9,-1.0} },		//face 2
				{ {2,-1.0}, {11,-1.0}, {6,-1.0}, {10,-1.0} },	//face 3
				{ {3,-1.0}, {10}, {5,-1.0}, {8} },				//face 4
				{ {1,-1.0}, {9}, {7,-1.0}, {11} }				//face 5
			},
			[&](real mass, glmvec3& s) { //callback for calculating the inertia tensor of this polytope
				return mass * glmmat3{ {s.y * s.y + s.z * s.z,0,0}, {0,s.x * s.x + s.z * s.z,0}, {0,0,s.x * s.x + s.y * s.y} } / 12.0_real;
			}
		};

		//--------------------------------------------------------------------------------------------------
		//Physics engine stuff

		class Body;
		using body_callb = std::function<void(double, std::shared_ptr<Body>)>; //call this function when the body moves

		/// <summary>
		/// This class implements the basic physics properties of a rigid body.
		/// </summary>
		class Body {

		public:
			VEEventListenerPhysics* m_physics;			//Pointer to the physics world to access parameters
			std::string	m_name;							//The name of this body

			//-----------------------------------------------------------------------------
			//Physics parameters of the body

			void* m_owner = nullptr;				//pointer to owner of this body, must be unique (owner is called if body moves)
			Polytope* m_polytope = nullptr;			//geometric shape
			glmvec3		m_scale{ 1,1,1 };				//scale factor in local space
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmquat		m_orientationLW{ 1, 0, 0, 0 };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };	//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space
			body_callb* m_on_move = nullptr;			//called if the body moves
			real		m_mass_inv{ 0 };				//1 over mass
			real		m_restitution{ 0 };				//coefficient of restitution eps
			real		m_friction{ 1 };				//coefficient of friction mu
			uint64_t	m_loop_last_active{ 0 };			//The loop number in which this body was last time active

			std::unordered_map<uint64_t, Force> m_forces;//forces acting on this body

			//-----------------------------------------------------------------------------
			//These members are computed by the class, only change if you know what you are doing

			glmmat3		m_inertiaL{ 1 };				//computed when the body is created
			glmmat3		m_inertia_invL{ 1 };			//computed when the body is created

			//-----------------------------------------------------------------------------
			//computed when the body moves

			glmmat4		m_model{ glmmat4{1} };			//model matrix at time slots
			glmmat4		m_model_inv{ glmmat4{1} };		//model inverse matrix at time slots
			glmmat3		m_model_it;						//orientation inverse transpose for bringing normal vector to world
			glmmat3		m_inertiaW{ glmmat4{1} };		//inertia tensor in world frame
			glmmat3		m_inertia_invW{ glmmat4{1} };	//inverse inertia tensor in world frame
			int_t		m_grid_x{ 0 };					//grid coordinates for broadphase
			int_t		m_grid_z{ 0 };
			glmvec3		m_pbias{ 0, 0, 0 };				//extra energy if body overlaps with another body
			uint32_t	m_num_resting{ 0 };				//number resting contact points 
			real		m_damping{ 0 };					//damping velocity of resting contact points

			/// <summary>
			/// Constructor of class Body. Uses ony default parameters.
			/// </summary>
			/// <param name="physics">Pointer to the physics world.</param>
			Body(VEEventListenerPhysics* physics) : m_physics{ physics } { m_polytope = &m_physics->g_cube; inertiaTensorL(); updateMatrices(); };

			/// <summary>
			/// Constructor of class Body
			/// </summary>
			/// <param name="physics">Pointer to the physics world.</param>
			/// <param name="name">Name of the body.</param>
			/// <param name="owner">Pointer to the owner. The callback knows how to use this pointer.</param>
			/// <param name="polytope">Pointer to the polytope.</param>
			/// <param name="scale">3D scaling.</param>
			/// <param name="positionW">3D position of the body.</param>
			/// <param name="orientationLW">Body orientation as quaternion.</param>
			/// <param name="on_move">Callback that is called when the body moves.</param>
			/// <param name="linear_velocityW">Starting linear velocity.</param>
			/// <param name="angular_velocityW">Starting angular velovity as axis vector. Length of vector is the speed.</param>
			/// <param name="mass_inv">1 / mass. If zero, then mass is infinite.</param>
			/// <param name="restitution">Bounciness, between 0 and 1.</param>
			/// <param name="friction">Friction coefficient, usually larger than 0.5.</param>
			Body(VEEventListenerPhysics* physics, std::string name, void* owner, Polytope* polytope,
				glmvec3 scale, glmvec3 positionW, glmquat orientationLW = { 1,0,0,0 },
				body_callb* on_move = nullptr, glmvec3 linear_velocityW = glmvec3{ 0,0,0 }, glmvec3 angular_velocityW = glmvec3{ 0,0,0 },
				real mass_inv = 0, real restitution = 0.2_real, real friction = 1) :
				m_physics{ physics }, m_name{ name }, m_owner{ owner }, m_polytope{ polytope },
				m_scale{ scale }, m_positionW{ positionW }, m_orientationLW{ orientationLW },
				m_on_move{ on_move }, m_linear_velocityW{ linear_velocityW }, m_angular_velocityW{ angular_velocityW },
				m_mass_inv{ mass_inv }, m_restitution{ restitution }, m_friction{ friction } {
				m_scale *= m_physics->m_collision_margin_factor;
				inertiaTensorL();
				updateMatrices();
				m_loop_last_active = m_physics->m_loop;
			};

			/// <summary>
			/// Euler step for position. Can be used for integration or extrapolation in between time steps.
			/// </summary>
			/// <param name="dt">Delta time step.</param>
			/// <param name="pos">Current position that is updated.</param>
			/// <param name="quat">Current orientation that is updated.</param>
			/// <returns></returns>
			bool stepPosition(double dt, glmvec3& pos, glmquat& quat, bool use_pbias = true) {
				bool active = !m_physics->m_deactivate;

				bool dx = fabs(m_linear_velocityW.x) > m_physics->c_small;
				bool dy = fabs(m_linear_velocityW.y) > -m_physics->m_resting_factor * m_physics->c_gravity * m_physics->m_sim_delta_time;
				bool dz = fabs(m_linear_velocityW.z) > m_physics->c_small;

				if (dx || dy || dz || m_num_resting < 3) {
					if (m_physics->m_clamp_position == 1) pos += m_linear_velocityW * (real)dt;
					active = true;
				}
				if (m_physics->m_clamp_position == 0) pos += m_linear_velocityW * (real)dt;
				pos += m_physics->m_pbias_factor * m_pbias * (real)dt;
				if (use_pbias) { m_pbias = glmvec3{ 0,0,0 }; }

				auto avW = glmmat3{ m_model_inv } *m_angular_velocityW;	//The same for orientation
				real len = glm::length(avW);
				if (len > m_physics->c_small) {
					if (m_physics->m_clamp_position == 1) quat = glm::rotate(quat, len * (real)dt, avW / len);	//Euler step
					active = true;
				}
				if (m_physics->m_clamp_position == 0 && len != 0.0_real) quat = glm::rotate(quat, len * (real)dt, avW / len); //Euler step

				if (active) {
					m_damping = 0.0_real;						//If the body moves then no damping
					m_loop_last_active = m_physics->m_loop;		//remember that the body is now active
				}
				else {
					m_damping = std::clamp(m_damping + m_physics->m_damping_incr, 0.0_real, 600.0_real);	//If no motion, increase damping
				}

				return active;
			};

			/// <summary>
			/// Euler step for velocity.
			/// </summary>
			/// <param name="dt">Delta time step.</param>
			/// <returns></returns>
			void stepVelocity(double dt) {
				glmvec3 sum_accelW{ 0 };	//sum of all accelerations
				glmvec3 sum_forcesW{ 0 };	//sum of all forces in world coordinates
				glmvec3 sum_torquesW{ 0 };	//sum of all torques in world coordinates
				for (auto& force : m_forces) {
					sum_accelW += force.second.m_accelW;
					sum_forcesW += glmmat3{ m_model } *force.second.m_forceL + force.second.m_forceW;	//forces in local and world coordinates
					sum_torquesW += glm::cross(glmmat3{ m_model } *force.second.m_positionL, glmmat3{ m_model } *force.second.m_forceL);
				}
				m_linear_velocityW += (real)dt * (m_mass_inv * sum_forcesW + sum_accelW);
				m_angular_velocityW += (real)dt * (m_inertia_invW * (sum_torquesW - glm::cross(m_angular_velocityW, m_inertiaW * m_angular_velocityW)));

				m_linear_velocityW.x *= 1.0_real / (1.0_real + (real)m_physics->m_sim_delta_time * m_damping);	//apply sideways damping if any
				m_linear_velocityW.z *= 1.0_real / (1.0_real + (real)m_physics->m_sim_delta_time * m_damping);
				m_angular_velocityW *= 1.0_real / (1.0_real + (real)m_physics->m_sim_delta_time * m_damping);
			}

			/// <summary>
			/// Compute total velocity of object at a certain point.
			/// </summary>
			/// <param name="positionW">Position of interest in world coordinates.</param>
			/// <returns></returns>
			glmvec3 totalVelocityW(glmvec3 positionW) {
				return m_linear_velocityW + glm::cross(m_angular_velocityW, positionW - m_positionW);
			}

			/// <summary>
			/// Calculate radius of bounding sphere of object.
			/// </summary>
			/// <returns>Bounding sphere radius.</returns>
			real boundingSphereRadius() {
				return std::max(m_scale.x, std::max(m_scale.y, m_scale.z)) * m_polytope->m_bounding_sphere_radius;
			}

			/// <summary>
			/// Calculate mass of object. Is the inverse of the inverse mass. If inverse is 0 then
			/// return a very large number.
			/// </summary>
			/// <returns>Mass of object.</returns>
			real mass() {
				return m_mass_inv <= c_eps ? 1.0_real / (c_eps) : 1.0_real / m_mass_inv;
			}

			/// <summary>
			/// Calculate inertia tensor and inverse in world coordinates. Only done once at start.
			/// </summary>
			/// <returns>Inertia tensor in world coordinates.</returns>
			void inertiaTensorL() {
				m_inertiaL = m_polytope->inertiaTensor(mass(), m_scale);	//Polytope inertia tensor
				m_inertia_invL = glm::inverse(m_inertiaL);					//Inverse inertia tensor
			}

			/// <summary>
			/// Compute the model (world) matrix for transforming points from local space to world space.
			/// Is used to set this in the body callback, thus it is static.
			/// </summary>
			/// <param name="pos">Current position.</param>
			/// <param name="orient">Current orientation.</param>
			/// <param name="scale">Object scale.</param>
			/// <returns></returns>
			static glmmat4 computeModel(glmvec3& pos, glmquat& orient, glmvec3& scale) {
				return glm::translate(glmmat4{ 1.0_real }, pos) * glm::mat4_cast(orient) * glm::scale(glmmat4{ 1.0_real }, scale);
			};

			/// <summary>
			/// Create model, inverse model, an inertia matrices.
			/// </summary>
			void updateMatrices() {
				glmmat3 rot3{ glm::mat4_cast(m_orientationLW) };

				m_model = computeModel(m_positionW, m_orientationLW, m_scale);  //model matrix
				m_model_inv = glm::inverse(m_model);							//inverse model matrix
				m_model_it = glm::transpose(glm::inverse(glmmat3{ rot3 }));		//inverse transpose to transform normal vectors

				m_inertiaW = rot3 * m_inertiaL * glm::transpose(rot3);			//inertia tensor depending on current orientation
				m_inertia_invW = rot3 * m_inertia_invL * glm::transpose(rot3);
			}

			/// <summary>
			/// Support mapping function of polytope.
			/// </summary>
			/// <param name="dirL">Search direction in local space.</param>
			/// <returns>Pointer to supporting vertex of body.</returns>
			Vertex* support(glmvec3 dirL) {
				auto compare = [&](auto& a, auto& b) { return glm::dot(dirL, a.m_positionL) < glm::dot(dirL, b.m_positionL); };
				return std::ranges::max_element(m_polytope->m_vertices, compare)._Ptr;
			};
		};

		//--------------------------------------------------------------------------------------------------
		//Contact between bodies

		/// <summary>
		/// Stable contacts: https://www.gdcvault.com/play/1022193/Physics-for-Game-Programmers-Robust
		/// This struct holds the contact information between a pair of bodies.
		/// </summary>
		struct Contact {

			/// <summary>
			/// A generalize pointer to a body, also including transforms. Used to be able to quickly
			/// swap the meaning of two bodies (reference and incident body)
			/// </summary>
			struct BodyPtr {
				std::shared_ptr<Body> m_body;	//pointer to body
				glmmat4 m_to_other;				//transform to other body
				glmmat3 m_to_other_it;			//inverse transpose of transform to other body, for normal vectors
			};

			/// <summary>
			/// A single point of contact between two bodies.
			/// </summary>
			struct ContactPoint {

				/// <summary>
				/// Characterize which contact this is.
				/// </summary>
				enum type_t {
					none,		//Unknown
					colliding,	//Colliding, bodies are bumping into each other
					resting,	//No collision, bodies are resting, maybe sliding
					separating	//Bodies are separating into different directions
				};

				glmvec3 m_positionW{ 0 };			//Position in world coordinates
				type_t	m_type{ type_t::none };	//Type of contact point
				real	m_restitution;				//Restitution of velcoties after a collision (if reting then = 0)
				real	m_friction;					//Friction coefficient
				glmvec3 m_r0W;						//Position in world coordinates relative to body 0 center
				glmvec3 m_r1W;						//Position in world relative to body 1 center
				glmmat3	m_K;			//mass matrix
				glmmat3	m_K_inv;		//inverse mass matrix
				real	m_vbias{ 0 };	//extra energy if bodies overlap
				real	m_f{ 0 };		//accumulates force impulses along normal (1D) during a loop run
				glmvec2	m_t{ 0 };		//accumulates force impulses along tangent (2D) during a loop run
			};

			uint64_t	m_last_loop{ std::numeric_limits<uint64_t>::max() }; //number of last loop this contact was valid
			BodyPtr		m_body_ref;							//reference body, we will use its local space mostly
			BodyPtr		m_body_inc;							//incident body, we will transfer its points/vectors to the ref space
			uint64_t	m_num_resting{ 0 };					//Number of resting contacts
			bool		m_active{ true };					//if deactive, the contact is ignored
			glmvec3		m_separating_axisW{ 0 };			//Axis that separates the two bodies in world space

			glmvec3					m_normalW{ 0 };			//Contact normal
			std::array<glmvec3, 2>	m_tangentW;				//Contact tangent vector (calculated for each normal)

			std::vector<ContactPoint> m_contact_points{};		//Contact points in contact manifold in world space
			std::vector<ContactPoint> m_old_contact_points{};	//Contact points in contact manifold in world space in prev loop
		};

		/// <summary>
		/// This function adds a new contact point to a contact manifold. 
		/// </summary>
		/// <param name="positionW">Position in world coordinates.</param>
		/// <param name="normalW">Normal vector in world coordinates.</param>
		/// <param name="penetration">Interpenetration depth. If <0 then there is interpenetration.</param>
		void addContactPoint(Contact& contact, glmvec3 positionW, glmvec3 normalW, real penetration) {
			if (contact.m_contact_points.size() == 0) {									//If first contact point
				contact.m_normalW = normalW;											//Use its normal vector
				geometry::computeBasis(normalW, contact.m_tangentW[0], contact.m_tangentW[1]);	//Calculate a tangent base
				contact.m_num_resting = 0;
			}
			auto r0W = positionW - contact.m_body_ref.m_body->m_positionW;	//Position relative to body 0 center in world coordinates
			auto r1W = positionW - contact.m_body_inc.m_body->m_positionW;	//Position relative to body 1 center in world coordinates
			auto vrel = contact.m_body_inc.m_body->totalVelocityW(positionW) - contact.m_body_ref.m_body->totalVelocityW(positionW);
			auto d = glm::dot(vrel, normalW);

			Contact::ContactPoint::type_t type;	//determine the contact point type
			real vbias = 0.0_real;
			if (d > m_sep_velocity) {										//Separating contact
				type = Contact::ContactPoint::type_t::separating;
			}
			else if (d > m_resting_factor * c_gravity * m_sim_delta_time) {	//Resting contact
				type = Contact::ContactPoint::type_t::resting;
				contact.m_body_ref.m_body->m_num_resting++;
				contact.m_body_inc.m_body->m_num_resting++;
				contact.m_num_resting++;
				vbias = (penetration < 0.0_real) ? m_bias * (real)m_sim_frequency * std::max(0.0_real, -penetration - m_slop) : 0.0_real;
			}
			else {
				type = Contact::ContactPoint::type_t::colliding;			//Colliding contact
			}

			auto restitution = std::max(contact.m_body_ref.m_body->m_restitution, contact.m_body_inc.m_body->m_restitution);
			restitution = (type == Contact::ContactPoint::type_t::colliding ? restitution : 0.0_real);
			auto friction = (contact.m_body_ref.m_body->m_friction + contact.m_body_inc.m_body->m_friction) / 2.0_real;

			auto mc0 = matrixCross3(r0W);	//Turn cross product into a matrix multiplication
			auto mc1 = matrixCross3(r1W);

			auto K = glmmat3{ 1.0_real } *contact.m_body_inc.m_body->m_mass_inv - mc1 * contact.m_body_inc.m_body->m_inertia_invW * mc1 + //mass matrix
				glmmat3{ 1.0_real } *contact.m_body_ref.m_body->m_mass_inv - mc0 * contact.m_body_ref.m_body->m_inertia_invW * mc0;

			auto K_inv = glm::inverse(K);	//Inverse of mass matrix (roughly 1/mass)

			contact.m_contact_points.emplace_back(positionW, type, restitution, friction, r0W, r1W, K, K_inv, vbias);
		}


		//--------------------------------------------------------------------------------------------------
		//simulation parameters

		real	m_collision_margin_factor = 1.001_real;		//This factor makes physics bodies a little larger to prevent visible interpenetration
		real	m_collision_margin = 0.005_real;			//Also a little slack for detecting collisions
		real	m_sep_velocity = 0.01_real;					//Limit such that a contact is seperating and not resting
		real	m_bias = 0.2_real;							//A small addon to reduce interpenetration
		real	m_slop = 0.001_real;						//This much penetration does not cause bias
		real	m_resting_factor = 3.0_real;				//Factor for determining when a collision velocity is actually just resting
		double	m_sim_frequency = 60.0;						//Simulation frequency in Hertz
		double	m_sim_delta_time = 1.0 / m_sim_frequency;	//The time to move forward the simulation
		int		m_solver = 0;								//Select which solver to use
		int		m_clamp_position = 1;						//No motions below a certain limit
		int		m_use_vbias = 1;							//If true, the the bias is used for resting contacts
		int		m_align_position_bias = 1;					//if true then look of current position bias is already enough
		real	m_pbias_factor = 0.5_real;					//Add only a fraction of the current position bias.
		int		m_use_warmstart = 1;						//If true then warm start resting contacts
		int		m_use_warmstart_single = 0;					//If true then warm start resting contacts
		int		m_loops = 30;								//Number of loops in each simulation step
		bool	m_deactivate = true;						//Do not move objects that are deactivated
		real	m_num_active{ 0 };							//Number of currently active bodies
		real	m_damping_incr = 10.0_real;					//Damp motion of slowly moving resting objects 
		real	m_restitution = 0.2_real;					//Coefficient of restitution (bounciness)
		real	m_friction = 1.0_real;						//Coefficient of friction
		real	m_fps = 0.0_real;

		//--------------------------------------------------------------------------------------------------
		//simulation state

		/// <summary>
		/// State of the simulation. Can be either real time or debug.
		/// In debug, the simulation does not advance by itself but time can
		/// be advanced from the outside by increasing m_current_time.
		/// </summary>
		enum simulation_mode_t {
			SIMULATION_MODE_REALTIME,	//Normal real time simulation
			SIMULATION_MODE_DEBUG		//Debug mode
		};
		simulation_mode_t m_mode{ SIMULATION_MODE_REALTIME };	//state of the simulation

		bool			m_run = true;					//If false, halt the simulation
		uint64_t		m_loop{ 0L };					//Loop counter
		double			m_current_time{ 0 };			//Current time
		double			m_last_time{ 0 };				//Last time the sim was interpolated
		double			m_last_slot{ 0 };				//Last time the sim was calculated
		double			m_next_slot{ m_sim_delta_time };//Next time for simulation

		/// <summary>
		/// All bodies are stored in the map m_bodies. The key is a void*, which can be used 
		/// to call back an owner if the body moves. With this key, the body can also be found.
		/// So best if there is a 1:1 correspondence. E.g., the owner can be a specific VESceneNode.
		/// </summary>
		using body_map = std::unordered_map<void*, std::shared_ptr<Body>>;
		body_map	m_bodies;						//main container of all bodies
		
		/// <summary>
		/// The broadphase uses a 2D grid of cells, each body is stored in exactly one cell.
		/// Only cells which actually contain bodies are stored in the map.
		/// </summary>
		real		m_width{3};								//grid cell width (m)
		std::unordered_map<intpair_t, body_map > m_grid;	//broadphase grid of cells.

		std::shared_ptr<Body> m_ground = std::make_shared<Body>( Body{ this, "Ground", nullptr, &g_cube, {1000, 1000, 1000}, {0, -500.0_real, 0}, {1,0,0,0} });
		body_map	m_global_cell{ { nullptr, m_ground } };	//cell containing only the ground

		/// <summary>
		/// A contact is a struct that contains contact information for a pair of bodies A and B.
		/// The map hash function alsways uses the smaller of A and B first, so there is only one contact for
		/// A/B and B/A.
		/// </summary>
		std::unordered_map<voidppair_t, Contact> m_contacts;	//possible contacts resulting from broadphase

		//-----------------------------------------------------------------------------------------------------

		/// <summary>
		/// Pick the body that is best aligned with the camera lokking direction.
		/// </summary>
		/// <returns>The picked body.</returns>
		std::shared_ptr<Body> pickBody() {
			glmvec3 pos{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
			glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
			auto compare = [&](auto& a, auto& b) { return glm::dot(glm::normalize(a.second->m_positionW - pos), dir) < glm::dot(glm::normalize(b.second->m_positionW - pos), dir); };
			return std::ranges::max_element(m_bodies, compare)->second;
		}
		std::shared_ptr<Body> m_body; // the body we can move with the debug panel (always the latest body created)


		/// <summary>
		/// This callback is used for updating the visual boy whenever the physics box moves.
		/// It is also used for extrapolating the new position between two simulation slots.
		/// </summary>
		std::function<void(double, std::shared_ptr<Body>)> onMove = [&](double dt, std::shared_ptr<Body> body) {
			VESceneNode* cube = static_cast<VESceneNode*>(body->m_owner);		//Owner is a pointer to a scene node
			glmvec3 pos = body->m_positionW;									//New position of the scene node
			glmquat orient = body->m_orientationLW;								//New orientation of the scende node
			body->stepPosition(dt, pos, orient, false);							//Extrapolate
			cube->setTransform(Body::computeModel(pos, orient, body->m_scale));	//Set the scene node data

		};

		/// <summary>
		/// Add a body to the 2D broadphase grid. This is a separate function, so we can later change the grid width.
		/// </summary>
		/// <param name="pbody">The body to add.</param>
		void addGrid( auto pbody ) {
			pbody->m_grid_x = static_cast<int_t>(pbody->m_positionW.x / m_width);	//2D coordinates in the broadphase grid
			pbody->m_grid_z = static_cast<int_t>(pbody->m_positionW.z / m_width);
			m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].insert({ pbody->m_owner, pbody }); //Put into broadphase grid
		}

		/// <summary>
		/// Add a new body to the physics world.
		/// </summary>
		/// <param name="pbody">The new body.</param>
		void addBody( auto pbody ) {
			m_bodies.insert( { pbody->m_owner, pbody } );	//Put into body container
			addGrid( pbody );	//add to broadphase grid.
		}

		/// <summary>
		/// Create a number of bodies at random places to populate the scene.
		/// </summary>
		/// <param name="n">Number of bodies to create.</param>
		void createRandomBodies( auto n) {
			for (int i = 0; i < n; ++i) {
				glmvec3 pos = { rnd_unif(rnd_gen), 20 * rnd_unif(rnd_gen) + 10.0_real, rnd_unif(rnd_gen) };
				glmvec3 vel = { rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				real angle = (real)rnd_unif(rnd_gen) * 10 * 3 * (real)M_PI / 180.0_real;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5 };
				VESceneNode* cube;
				VECHECKPOINTER(cube = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{this, "Body" + std::to_string(m_bodies.size()), cube, &g_cube, scale, pos, glm::rotate(angle, glm::normalize(orient)), &onMove, vel, vrot, 1.0_real / 100.0_real, m_restitution, m_friction };
				body.m_forces.insert({ 0ul, Force{ {0, c_gravity, 0} } });
				addBody(m_body = std::make_shared<Body>(body));
			}
		}

		/// <summary>
		/// Delete all bodies.
		/// </summary>
		void clear() {
			for (auto& body : m_bodies) getSceneManagerPointer()->deleteSceneNodeAndChildren( ((VESceneNode*) body.second->m_owner)->getName());
			m_bodies.clear();
			m_grid.clear();
		}

		/// <summary>
		/// Erase one body.
		/// </summary>
		/// <param name="body">(Shared) pointer to the body.</param>
		void eraseBody(auto body) {
			getSceneManagerPointer()->deleteSceneNodeAndChildren(((VESceneNode*)body->m_owner)->getName());
			m_bodies.erase(body->m_owner);
			m_grid[intpair_t{ body->m_grid_x, body->m_grid_z }].erase(body->m_owner);
		}
		
		/// <summary>
		/// If a body moves, it may be transferred to another broadphase grid cell.
		/// </summary>
		/// <param name="pbody">The body that moved.</param>
		void moveBodyInGrid(auto pbody) {
			int_t x = static_cast<int_t>(pbody->m_positionW.x / m_width);	//2D grid coordinates
			int_t z = static_cast<int_t>(pbody->m_positionW.z / m_width);	
			if (x != pbody->m_grid_x || z != pbody->m_grid_z) {				//Did they change?
				m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].erase(pbody->m_owner); //Remove body from old cell
				pbody->m_grid_x = x;
				pbody->m_grid_z = z;
				m_grid[intpair_t{ x, z }].insert({ pbody->m_owner, pbody }); //Put body in new cell
			}
		}

		/// <summary>
		/// Add a position bias coming from a contact. If the current bias already is enough do not add.
		/// </summary>
		/// <param name="old_bias">The current bias.</param>
		/// <param name="new_bias">The new bias to be added to the current bias.</param>
		void addPositionBias(glmvec3& current_bias, glmvec3& new_bias ) {
			auto B = new_bias;								//temp value
			if (m_align_position_bias == 1) {				//Should we align thenew bias with the old one?
				auto l = glm::length(current_bias);			//Length of current bias
				if (l > 0.0_real) {							//if there is a current bias
					auto f = std::max( glm::dot(new_bias, current_bias / l) - l, 0.0_real); //how much is already in this direction?
					B = new_bias - f * current_bias / l;	//add only the bias that goes beyong this
				}
			}
			current_bias += B;		//add the new bias
		}

		/// <summary>
		/// Calculate the position bias for both bodies of a contact, and add it to the current biases.
		/// </summary>
		/// <param name="query_separation">Seperation distance according to query.</param>
		/// <param name="face_separation">Separation distance as result of face calculations.</param>
		/// <param name="normalL">Normal vector from query.</param>
		/// <param name="contact">Contact information..</param>
		void positionBias(real query_separation, real face_separation, glmvec3 normalL, Contact &contact ) {
			if (query_separation < m_collision_margin) {
				real weight = 1.0_real / (1.0_real + contact.m_body_inc.m_body->mass() * contact.m_body_ref.m_body->m_mass_inv);
				auto pbias = -RTOWN(normalL) * (-query_separation) * (real)m_sim_frequency * (1.0_real - weight);
				addPositionBias(contact.m_body_ref.m_body->m_pbias, pbias);
				pbias = RTOWN(normalL) * (-query_separation) * (real)m_sim_frequency * weight;
				addPositionBias(contact.m_body_inc.m_body->m_pbias, pbias);
			}
		}

		/// <summary>
		/// Callback for event key stroke. Depending on the key pressed, bodies are created.
		/// </summary>
		/// <param name="event">The keyboard event.</param>
		/// <returns>False, so the key is not consumed.</returns>
		bool onKeyboard(veEvent event) {

			if (event.idata1 == GLFW_KEY_B && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
				glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
				glmvec3 vel = 35.0_real * (real)rnd_unif(rnd_gen) * dir / glm::length(dir);
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				real angle = (real)rnd_unif(rnd_gen) * 10 * 3 * (real)M_PI / 180.0_real;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5 };
				VESceneNode* cube;
				VECHECKPOINTER(cube = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ this, "Body" + std::to_string(m_bodies.size()), cube, &g_cube, scale, positionCamera + 2.0_real * dir, glm::rotate(angle, glm::normalize(orient)), &onMove, vel, vrot, 1.0_real / 100.0_real, m_restitution, m_friction };
				body.m_forces.insert({ 0ul, Force{ {0, c_gravity, 0} } });
				addBody(m_body = std::make_shared<Body>(body));
			}
								
			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
				
				for (int i = 0; i < 1; ++i) {
					VESceneNode* cube0;
					static real dy = 0.5_real;
					VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
					Body body0{ this, "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{ 1.0_real }, glmvec3{positionCamera.x, dy++, positionCamera.z + 4}, glmquat{ 1,0,0,0 }, &onMove, glmvec3{0.0_real}, glmvec3{0.0_real}, 1.0_real / 100.0_real, m_restitution, m_friction };
					body0.m_forces.insert({ 0ul, Force{ {0, c_gravity, 0} } });
					addBody(m_body = std::make_shared<Body>(body0));
				}
			}

			if (event.idata1 == GLFW_KEY_Y && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };

				for (int dy = 0; dy < 10; ++dy) {
					for (int dx = 0; dx < 10 - dy; ++dx) {
						VESceneNode* cube0;
						VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
						Body body0{ this, "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{1.0_real}, glmvec3{dx + 0.4 * dy, 0.2_real + dy, 0.0_real}, glmquat{1,0,0,0}, &onMove, glmvec3{0.0_real}, glmvec3{0.0_real}, 1.0_real / 100.0_real, m_restitution, m_friction };
						body0.m_forces.insert({ 0ul, Force{ {0, c_gravity, 0} } });
						addBody(m_body = std::make_shared<Body>(body0));
					}
				}
			}

			if (event.idata1 == GLFW_KEY_Z && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
				glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
				VESceneNode* cube0;
				VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ this, "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{1.0_real}, positionCamera + 2.0_real * dir, glmquat{1,0,0,0}, &onMove, glmvec3{0.0_real}, glmvec3{0.0_real}, 1.0_real / 100.0_real, m_restitution, m_friction };
				body.m_forces.insert({ 0ul, Force{ {0, c_gravity, 0} } });
				addBody(m_body = std::make_shared<Body>(body));
			}

			return false;
		};

		//--------------------------------------------------------------------------------------------------------
		/// <summary>
		/// Callback for the frame started event. This is the MAIN physics engine entry point!
		/// Once at the start of each new frame this is called.
		/// The function calculates the current collision/contact points, and warmstarts resting contacts.
		/// Time is forwarded in fixed slots. If the clock falls in between two slots (which it always does)
		/// then the state of the world at this time is forward extrapolated.
		/// </summary>
		/// <param name="event">The event data.</param>
		void onFrameStarted(veEvent event) {
			if (m_mode == SIMULATION_MODE_REALTIME) {		//if the engine is in realtime mode, advance time
				m_current_time = m_last_time + event.dt;	//advance time by the time that went by since the last loop
				if( event.dt != 0.0) m_fps = 1.0_real / (real)event.dt; //estimate for frames per second
			}

			auto last_loop = m_loop;
			while (m_current_time > m_next_slot) {	//compute position/vel only at time slots
				++m_loop;				//increase loop counter
				uint_t num_active{ 0 };	//set number currently active objects to 0
				broadPhase();			//run the broad phase
				narrowPhase();			//Run the narrow phase
				warmStart();			//Warm start the resting contacts if possible

				for (auto& body : m_bodies) { body.second->stepVelocity(m_sim_delta_time); }		//Integration step for velocity
				calculateImpulses(m_loops, m_sim_delta_time);	//Calculate and apply impulses

				for (auto& body : m_bodies) {	//integrate positions and update the matrices for the bodies
					if (body.second->stepPosition(m_sim_delta_time, body.second->m_positionW, body.second->m_orientationLW)) ++num_active;
					body.second->updateMatrices();
				}
				m_num_active = 0.9_real * m_num_active + 0.1_real * num_active; //smooth the number of active nodies
				if (m_num_active < c_small) m_num_active = 0;					//If near 0, set to 0
				m_last_slot = m_next_slot;			//Remember last slot
				m_next_slot += m_sim_delta_time;	//Move to next time slot as slong as we do not surpass current time
			}
			if (m_loop > last_loop) {	//if we have entered a new time slot bodies might have moved, so update broadphase grid
				for (auto& body : m_bodies) { moveBodyInGrid(body.second); } //update grid
			}
			for (auto& body : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, this is not stored anywhere
				if (body.second->m_on_move != nullptr) {
					(*body.second->m_on_move)(m_current_time - m_last_slot, body.second); //predict new pos/orient
				}
			}
			m_last_time = m_current_time;	//save last time
		};

		/// <summary>
		/// Given a specific cell and a neighboring cell, create all pairs of bodies, where one body is in the 
		/// cell, and one body is in the neighbor.
		/// </summary>
		/// <param name="cell">The grid cell. </param>
		/// <param name="neigh">The neighbor cell. Can be identical to the cell itself.</param>
		void makeBodyPairs(const body_map& cell, const body_map& neigh) {
			for (auto& coll : cell) {
				for (auto& neigh : neigh) {
					if (coll.second->m_owner != neigh.second->m_owner) {
						auto it = m_contacts.find({ coll.second->m_owner, neigh.second->m_owner }); //if contact exists already
						if (it != m_contacts.end()) { it->second.m_last_loop = m_loop; }			// yes - update loop count
						else { 
							m_contacts.insert({ { coll.second->m_owner, neigh.second->m_owner }, {m_loop, {coll.second}, {neigh.second} } }); //no - make new
						}
					}
				}
			}
		}

		/// <summary>
		/// Create pairs of objects that can touch each other. These are either in the same grid cell,
		/// or in neighboring cells. Go through all pairs of neighboring cells and create possible 
		/// body pairs as contacts. Old contacts an their information can be reused, if they are also 
		/// in the current set of pairs. Otherwise, previous contacts are removed.
		/// </summary>
		void broadPhase() {
			const std::array<intpair_t, 5> c_pairs{ { {0,0}, {1,0}, {-1,-1}, {0,-1}, {1,-1} } }; //neighbor cells

			for (auto& cell : m_grid) {		//loop through all cells that are currently not empty.
				makeBodyPairs(m_global_cell, cell.second);		//test all bodies against the ground
				for (auto& pi : c_pairs) {	//create pairs of neighborig cells and make body pairs.
					intpair_t ni = { cell.first.first + pi.first, cell.first.second + pi.second };
					if (m_grid.count(ni) > 0) { makeBodyPairs(cell.second, m_grid.at(ni)); }
				} 
			}
		}

		/// <summary>
		/// For each pair coming from the broadphase, test whether the bodies touch each other. If so then
		/// compute the contact manifold.
		/// </summary>
		void narrowPhase() {
			for (auto& body : m_bodies) {
				body.second->m_num_resting = 0;
			}
			m_ground->m_num_resting = 0;

			for (auto it = std::begin(m_contacts); it != std::end(m_contacts); ) {
				auto& contact = it->second;
				if (contact.m_last_loop == m_loop) {	//is contact still possible?
					contact.m_old_contact_points = std::move(contact.m_contact_points);

					if (!warmStartContact(contact)) {	//Can we completely warm start the contact?
						if (contact.m_body_ref.m_body->m_owner == nullptr) {
							groundTest(contact);		//is the ref body the ground?
						}
						else {
							glmvec3 diff = contact.m_body_inc.m_body->m_positionW - contact.m_body_ref.m_body->m_positionW;
							real rsum = contact.m_body_ref.m_body->boundingSphereRadius() + contact.m_body_inc.m_body->boundingSphereRadius();
							if (glm::dot(diff, diff) > rsum * rsum) { ++it;  continue; }
							SAT(it->second);						//yes - test it
						}
					}
					++it;
				}
				else { it = m_contacts.erase(it); }				//no - erase from container
			}
		}

		/// <summary>
		/// This function tries to warmstart a single deactivated contact, by using its previous contact points.
		/// This however destabilizes stacking, so do not use.
		/// </summary>
		/// <param name="contact"></param>
		/// <returns></returns>
		bool warmStartContact( Contact& contact ) {
			if (   m_use_warmstart_single ==0
				|| contact.m_body_ref.m_body->m_loop_last_active + 2 > m_loop		//do not set to 1
				|| contact.m_body_inc.m_body->m_loop_last_active + 2 > m_loop
				|| contact.m_old_contact_points.size() < 3
				|| contact.m_num_resting != contact.m_old_contact_points.size() ) {	//do not warmstart if non resting contact points present
				return false; 
			}

			for (auto& cp : contact.m_old_contact_points)  {
				auto F = cp.m_f * contact.m_normalW;
				contact.m_body_ref.m_body->m_linear_velocityW += -F * contact.m_body_ref.m_body->m_mass_inv;
				contact.m_body_ref.m_body->m_angular_velocityW += contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
				contact.m_body_inc.m_body->m_linear_velocityW += F * contact.m_body_inc.m_body->m_mass_inv;
				contact.m_body_inc.m_body->m_angular_velocityW += contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
			}
			contact.m_contact_points = std::move(contact.m_old_contact_points);	//reuse previous contact points.

			contact.m_body_ref.m_body->m_loop_last_active = 0;		//immediately wake up bodies
			contact.m_body_inc.m_body->m_loop_last_active = 0;

			return true;
		}

		/// <summary>
		/// A resting point can be warmstarted with its previous normal force. This
		/// increases stacking stability. We warmstart with old resting points at the same position.
		/// But we need a minimum number of old points, otherwise there is no warm starting.
		/// </summary>
		void warmStart() {
			if (m_use_warmstart == 0) return;
			int num_old_points{ 0 };
			for (auto& c : m_contacts) {
				auto& contact = c.second;
				auto num0 = contact.m_body_ref.m_body->m_num_resting;
				auto num1 = contact.m_body_inc.m_body->m_num_resting;

				if(num0 < 4 || num1 < 4 || contact.m_contact_points.size() == 0 || contact.m_old_contact_points.size() == 0) continue;

				for (auto& cp : contact.m_contact_points) {
					if (cp.m_type != Contact::ContactPoint::resting || cp.m_f != 0.0_real) continue; //warmstart only once
					for (auto& coldp : contact.m_old_contact_points) {
						if (coldp.m_type != Contact::ContactPoint::resting) continue;			//warmstart only resting points
						if (glm::length(cp.m_positionW - coldp.m_positionW) < c_small) {	//if old point is at same position
							cp.m_f = coldp.m_f;		//remember old normal force

							auto F = cp.m_f * contact.m_normalW;
							contact.m_body_ref.m_body->m_linear_velocityW += -F * contact.m_body_ref.m_body->m_mass_inv;
							contact.m_body_ref.m_body->m_angular_velocityW += contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
							contact.m_body_inc.m_body->m_linear_velocityW += F * contact.m_body_inc.m_body->m_mass_inv;
							contact.m_body_inc.m_body->m_angular_velocityW += contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
						}
					}
				}
			}
		}

		/// <summary>
		/// Test if a body collides with the ground. A vertex collides with the ground if its
		/// y world coordinate is negative.
		/// </summary>
		/// <param name="contact">The contact information between the ground and the body.</param>
		void groundTest(Contact& contact) {
			if (contact.m_body_inc.m_body->m_positionW.y > contact.m_body_inc.m_body->boundingSphereRadius()) return; //early out test
			real min_depth{ std::numeric_limits<real>::max() };
			for (auto& vL : contact.m_body_inc.m_body->m_polytope->m_vertices) {
				auto vW = ITOWP(vL.m_positionL);							//world coordinates
				if (vW.y <= m_collision_margin) {							//close to the ground?
					min_depth = std::min(min_depth, vW.y);					//remember smalles y coordinate for calculating bias
					addContactPoint(contact, vW, glmvec3{ 0,1,0 }, vW.y);	//add the contact point
				}
			}
			positionBias(min_depth, min_depth, glmvec3{ 0,1,0 }, contact);	//add position bias if necessary
		}

		/// <summary>
		/// For a given contact, go through all contact points and apply a small impulse to satisfy the 
		/// desired velocity. Since impulses in any loop can be negative, assure that the total impulses in
		/// normal and tangent directions do not get negative.
		/// </summary>
		/// <param name="contact">Contact manifold of two bodies.</param>
		/// <param name="contact_type">Only compute the given type of contacts.</param>
		/// <returns></returns>
		uint64_t calculateContactPointImpules(Contact &contact) {
			uint64_t res = 0;
			int i = -1;
			for (auto& cp : contact.m_contact_points) {
				++i;
				auto vref = contact.m_body_ref.m_body->totalVelocityW(cp.m_positionW);	//Veloity at contact point of reference body
				auto vinc = contact.m_body_inc.m_body->totalVelocityW(cp.m_positionW);	//Veloity at contact point of incident body
				auto vrel = vinc - vref;							//Velocity difference
				auto dN = glm::dot(vrel, contact.m_normalW);		//Closing speed, if negative then there is a collision
				real f{ 0.0_real }, t0{ 0.0_real }, t1{ 0.0_real };	//The impulses to be calculated

				if (m_solver == 0) {	//All in one solver
					auto F = cp.m_K_inv * (-cp.m_restitution * (dN + m_use_vbias * cp.m_vbias) * contact.m_normalW - vrel);
					cp.m_vbias = 0.0_real;
					f = glm::dot(F, contact.m_normalW);
					auto Fn = f * contact.m_normalW;
					auto Ft = F - Fn;
					t0 = -glm::dot(Ft, contact.m_tangentW[0]);
					t1 = -glm::dot(Ft, contact.m_tangentW[1]);
				}
				
				if (m_solver == 1) {						//Separate normal and tangent solver
					glmmat3 mc0 = matrixCross3(cp.m_r0W);
					glmmat3 mc1 = matrixCross3(cp.m_r1W);

					glmmat3 K = -mc1 * contact.m_body_inc.m_body->m_inertia_invW * mc1 - mc0 * contact.m_body_ref.m_body->m_inertia_invW * mc0;

					auto dV = -cp.m_restitution * dN * contact.m_normalW - vrel;
					auto kn = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + glm::dot(K * contact.m_normalW, contact.m_normalW);
					f = (glm::dot(dV, contact.m_normalW) + m_use_vbias * cp.m_vbias) / kn;
					cp.m_vbias = 0.0_real;

					auto kt0 = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + 
						glm::dot(K * contact.m_tangentW[0], contact.m_tangentW[0]);
					t0 = -glm::dot(dV, contact.m_tangentW[0]) / kt0;

					auto kt1 = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv +
						glm::dot(K * contact.m_tangentW[1], contact.m_tangentW[1]);
					t1 = -glm::dot(dV, contact.m_tangentW[1]) / kt1;
				}

				auto tmp = cp.m_f;		//make sure that aggregated normal impulse is not negative
				cp.m_f = std::max(tmp + f, 0.0_real);
				f = cp.m_f - tmp;

				glmvec2 dt{t0, t1};		//make sure that aggregated tangent impulse is not negative
				auto tmpt = cp.m_t;
				cp.m_t += dt;
				auto len = glm::length(cp.m_t);
				if (len > fabs(cp.m_f * cp.m_friction)) {			//friction still allowed?
					cp.m_t *= fabs(cp.m_f * cp.m_friction) / len;	//no -> reduce to max allowed length cp.m_f * cp.m_friction
					dt = cp.m_t - tmpt;
				}

				auto F = f * contact.m_normalW - dt.x * contact.m_tangentW[0] - dt.y * contact.m_tangentW[1]; //total impulse

				contact.m_body_ref.m_body->m_linear_velocityW  += -F * contact.m_body_ref.m_body->m_mass_inv;
				contact.m_body_ref.m_body->m_angular_velocityW +=      contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
				contact.m_body_inc.m_body->m_linear_velocityW  +=  F * contact.m_body_inc.m_body->m_mass_inv;
				contact.m_body_inc.m_body->m_angular_velocityW +=      contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
			}
			return res;
		}

		/// <summary>
		/// Go through all contacts and calculate and apply impulses. Do this until number of loops or time 
		/// run out.
		/// </summary>
		/// <param name="loops">Max number of loops through the contacts.</param>
		/// <param name="max_time">Max time you have.</param>
		void calculateImpulses(uint64_t loops, double max_time) {
			uint64_t num = loops;
			auto start = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			do {
				uint64_t res = 0;
				for (auto& contact : m_contacts) { 			//loop over all contacts
					auto nres = calculateContactPointImpules(contact.second);
					res = std::max(nres, (uint64_t)res);
				}
				num = num + res - 1;
				elapsed = std::chrono::high_resolution_clock::now() - start;
			} while (num > 0 && (m_mode == SIMULATION_MODE_DEBUG || std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() < 1.0e6 * max_time));
		}


		//----------------------------------------------------------------------------------------------------

		/// <summary>
		/// Store the result of a SAT query. 
		/// </summary>
		struct SatQuery {
			real	m_separation;	//separation length (negative)
			Vertex* m_vertA;		//ref vertex
			Vertex* m_vertB;		//inc vertex
		};

		/// <summary>
		/// Store the result of an edge-edge query
		/// </summary>
		struct EdgeQuery {
			real	m_separation;	//separation length (negative)
			Edge*	m_edge_ref;		//pointer to reference edge (body 0)
			Edge*	m_edge_inc;		//pointer to incident edge (body 1)
			glmvec3	m_normalL;		//normal of contact (cross product) in A's space
		};

		/// <summary>
		/// Store the result of face-face queries
		/// </summary>
		struct FaceQuery {
			real	m_separation;	//separation length (negative)
			Face*	m_face_ref;		//pointer to reference face
			Vertex* m_vertex_inc;	//pointer to incident vertex
		};

		/// <summary>
		/// For a reference and an incidence object, test if a given direction is a separating axis.
		/// </summary>
		/// <param name="contact">The contact to test.</param>
		/// <param name="nR">Possible separating axis in the reference object space.</param>
		/// <param name="vertex">A pointer to a vertex, to make sure that axis points away from ref object.</param>
		/// <returns>Distance between the object. If negative, this is the seperating distance. Also return ref and inc vertex.</returns>
		SatQuery sat_query(Contact& contact, glmvec3 nR) {
			auto nW = glm::normalize(RTOWN(nR));
			Vertex* vertA = contact.m_body_ref.m_body->support(nR);			//find support point in direction n
			Vertex* vertB = contact.m_body_inc.m_body->support(RTOIN(-nR));	//find support point in direction n
			real maxA = glm::dot(nW, RTOWP(vertA->m_positionL));					//distance in this direction for ref object
			real minB = glm::dot(nW, ITOWP(vertB->m_positionL));					//distance in this direction for inc object
			if(minB - maxA > m_collision_margin) contact.m_separating_axisW = nW;	//Remmber separating axis
			return { minB - maxA, vertA, vertB };	//return distance and reference and incident vertex
		}

		/// <summary>
		/// Perform SAT test for two bodies. If they overlap then compute the contact manifold.
		/// </summary>
		/// <param name="contact">The contact between the two bodies.</param>
		/// https://www.gdcvault.com/play/1022193/Physics-for-Game-Programmers-Robust
		/// http://media.steampowered.com/apps/valve/2015/DirkGregorius_Contacts.pdf
		/// 
		void SAT(Contact& contact) {
			contact.m_body_ref.m_to_other = contact.m_body_inc.m_body->m_model_inv * contact.m_body_ref.m_body->m_model; //transform to bring space A to space B
			contact.m_body_ref.m_to_other_it = glm::transpose(glm::inverse(glmmat3{ contact.m_body_ref.m_to_other }));	//transform for a normal vector
			contact.m_body_inc.m_to_other = contact.m_body_ref.m_body->m_model_inv * contact.m_body_inc.m_body->m_model; //transform to bring space B to space A
			contact.m_body_inc.m_to_other_it = glm::transpose(glm::inverse(glmmat3{ contact.m_body_inc.m_to_other }));	//transform for a normal vector

			if (contact.m_separating_axisW != glmvec3{ 0,0,0 } &&	//try old separating axis
				sat_query(contact, WTORN(contact.m_separating_axisW)).m_separation > m_collision_margin) { return; }

			FaceQuery fq0 = queryFaceDirections(contact);			//Query all normal vectors of faces of first body
			if (fq0.m_separation > m_collision_margin) { return; };	//found a separating axis with face normal

			std::swap(contact.m_body_ref, contact.m_body_inc);		//body 0 is the reference body having the reference face
			FaceQuery fq1 = queryFaceDirections(contact);			//Query all normal vectors of faces of second body
			if (fq1.m_separation > m_collision_margin) { return; };	//found a separating axis with face normal

			std::swap(contact.m_body_ref, contact.m_body_inc);		//prevent flip flopping
			EdgeQuery eq = queryEdgeDirections(contact);			//Query cross product of edge pairs from body 0 and 1	
			if (eq.m_separation > m_collision_margin) { return; }	//found a separating axis with edge-edge normal

			contact.m_separating_axisW = glmvec3{0,0,0};		//no separating axis found
			if (fq0.m_separation >= eq.m_separation * 1.001_real || fq1.m_separation >= eq.m_separation * 1.001_real) {	//max separation is a face-vertex contact
				if (fq0.m_separation >= fq1.m_separation) { createFaceContact(contact, fq0); } 
				else {
					std::swap(contact.m_body_ref, contact.m_body_inc);	//body 0 is the reference body having the reference face
					createFaceContact(contact, fq1);
				}
			}
			else { createEdgeContact(contact, eq); } //max separation is an edge-edge contact 
		}

		/// <summary>
		/// Loop through all faces of body a. Find min and max of body b along the direction of a's face normal.
		/// Return (largest) negative number if they overlap. Return positive number if they do not overlap.
		/// Will be called for BOTH bodies acting as reference, but only ONE (with LARGER NEGATIVE distance) 
		/// is the true reference!
		/// </summary>
		/// <param name="contact">The pair contact struct.</param>
		/// <returns>Negative: overlap of bodies along this axis. Positive: distance between the bodies.</returns>
		FaceQuery queryFaceDirections(Contact& contact) {
			FaceQuery result{ -std::numeric_limits<real>::max(), nullptr, nullptr };
			auto sat = [&]( auto& face ) {							//Run this function for each reference face
				auto sat = sat_query(contact, face.m_normalL);	//Call the sat, get distance
				if (sat.m_separation > result.m_separation) result = { sat.m_separation, &face, sat.m_vertB }; //remember max distance
				return sat.m_separation > m_collision_margin;	//if distance positive, stop - we found a separating axis
			};
			auto face = std::ranges::find_if(contact.m_body_ref.m_body->m_polytope->m_faces, sat);
			return result;
		}

		/// <summary>
		/// Loop over all edge pairs, where one edge is from reference body 0, and one is from incident body 1. 
		/// Create the cross product vector L of an edge pair.
		/// Choose L such that it points away from A. Then compute overlap between A and B.
		/// Return a negative number if there is overlap. Return a positive number if there is no overlap.
		/// </summary>
		/// <param name="contact">The contact pair.</param>
		/// <param name="AtoB">Transform from object space A to B.</param>
		/// <param name="BtoA">Transform from object space B to A.</param>
		/// <returns>Negative: overlap of bodies along this axis. Positive: distance between the bodies.</returns>
		EdgeQuery queryEdgeDirections(Contact& contact) {
			EdgeQuery result{ -std::numeric_limits<real>::max(), nullptr, nullptr };

			for (auto& edgeA : contact.m_body_ref.m_body->m_polytope->m_edges) {	//loop over all edge-edge pairs
				for (auto& edgeB : contact.m_body_inc.m_body->m_polytope->m_edges) {
					glmvec3 n = glm::cross(edgeA.m_edgeL, ITORV(edgeB.m_edgeL));	//axis n is cross product of both edges
					if (n == glmvec3{0,0,0}) continue;
					if (glm::dot(n, edgeA.m_first_vertexL.m_positionL) < 0)	n = -n;		//n must be oriented away from center of A								

					auto sat = sat_query(contact, n);		//Try normal to this edge-edge pair
					if (sat.m_separation > m_collision_margin) return { sat.m_separation, &edgeA, &edgeB };	//if distance positive, stop - we found a separating axis

					auto distance2 = glm::dot( n, ITORP(edgeB.m_first_vertexL.m_positionL) - sat.m_vertA->m_positionL);	//above does not depend on location - could find an adge on the other side
					if (distance2 <= m_collision_margin && sat.m_separation > result.m_separation) {
						result = { sat.m_separation, &edgeA, &edgeB, n };	//remember max of negative distances
					}						
				}
			}
			return result; 
		}

		/// <summary>
		/// We have a vertex-face contact, but the contact manifold could be a face/face contact.
		/// So find the face of the incident body that is best aligned with the normal and
		/// project it to the reference face, then clip the faces to get possible contact points.
		/// Test if these points are actually touching.
		/// </summary>
		/// <param name="contact">The contact between 2 bodies.</param>
		/// <param name="fq">Result of face query.</param>
		void createFaceContact(Contact& contact, FaceQuery& fq) {
			glmvec3 An = glm::normalize( -RTOIN(fq.m_face_ref->m_normalL) ); //transform normal vector of ref face to inc body
			Face* inc_face = maxFaceAlignment(An, fq.m_vertex_inc->m_vertex_face_ptrs);	//Find best incident face
			real sep = clipFaceFace(contact, fq.m_face_ref, inc_face);					//Project and clip it against reference face
			positionBias(fq.m_separation, sep, fq.m_face_ref->m_normalL, contact);		//Add position bias if necessary
		}

		/// <summary>
		/// We found a face of B that is aligned with the ref face of A. Bring inc face vertices of B into 
		/// A's face tangent space, then clip B against A. Bring the result into world space.
		/// If there are more than 4 contact points, reduce this to 4 by selecting those 4 points that span the 
		/// triangle with largest area. 
		/// </summary>
		/// <param name="contact">The contact between the bodies.</param>
		/// <param name="face_ref">The reference face.</param>
		/// <param name="face_inc">The incident face.</param>
		real clipFaceFace(Contact& contact, Face* face_ref, Face* face_inc) {
			std::vector<glmvec2> points;						//2D points holding the projected contact points				
			for (auto* vertex : face_inc->m_face_vertex_ptrs) {	//add face points of B's face
				auto pT = ITORTP( vertex->m_positionL );		//ransform to A's tangent space
				points.emplace_back(pT.x, pT.z);				//add as 2D point
			}
			std::vector<glmvec2> newPolygon;
			geometry::SutherlandHodgman(points, face_ref->m_face_vertex2D_T, newPolygon); //clip B's face against A's face

			if (newPolygon.size() > 4) {										//more than 4 contact points -> reduce to 4
				auto support = [](auto& dir, auto& newPoly, auto& supp) {		//2D support mapping function
					auto compare = [&](auto& a, auto& b) { return glm::dot(dir, a) < glm::dot(dir, b); };
					supp.push_back(*std::ranges::max_element(newPoly, compare));//find support points and push into vector
				};

				std::vector<glmvec2> dirs0{ {0,1}, {1,0}, {0,-1}, {-1,0}, {1,1}, {-1,1}, {-1,-1}, {1,-1} }; //along these directions
				std::vector<glmvec2> supp;
				std::ranges::for_each(dirs0, [&](auto& dirS) { support(dirS, newPolygon, supp); });			//find support points for all 8 directions

				//We want to use those 4 points that maximize the area of the quadrilateral they span
				//We have two such quadrilaterals, made by support points ß-3 and 4-7
				//We compute the area of a quadrilateral by cutting it into two triangles and computing the areas of both (actually double area)
				real A0 = fabs(glm::determinant(glmmat3{ {supp[0].x, supp[0].y, 1}, {supp[1].x, supp[1].y, 1}, {supp[2].x, supp[2].y, 1} }) +
					           glm::determinant(glmmat3{ {supp[0].x, supp[0].y, 1}, {supp[1].x, supp[1].y, 1}, {supp[3].x, supp[3].y, 1} }));

				real A1 = fabs(glm::determinant(glmmat3{ {supp[4].x, supp[4].y, 1}, {supp[5].x, supp[5].y, 1}, {supp[6].x, supp[6].y, 1} }) +
					           glm::determinant(glmmat3{ {supp[4].x, supp[4].y, 1}, {supp[5].x, supp[5].y, 1}, {supp[7].x, supp[7].y, 1} }));

				if (A0 > A1) { newPolygon = std::vector<glmvec2>{ supp[0], supp[1], supp[2], supp[3] }; }	//First quadrilateral is bigger
				else { newPolygon = std::vector<glmvec2>{ supp[4], supp[5], supp[6], supp[7] }; }			//Second quadrilateral is bigger

			}
			real min = 0.0_real;
			for (auto& p2D : newPolygon) {					//Go through all clip points
				auto p = glmvec3{ p2D.x, 0.0_real, p2D.y }; //cannot put comma into macro 
				glmvec3 posRW = RTTOWP(p);					//Bring them to world coordinates
				glmvec3 posIT = WTOTIP(posRW);				//Bring them to the tangent space of the incident face
				posIT.y = 0.0_real;							//Project to incident face
				glmvec3 posIW = ITTOWP( posIT );			//Bring back to world coordinates
				auto dist = glm::dot(posIW - posRW, RTOWN(face_ref->m_normalL));	//Distance between the two points in world coordinates
				if ( dist < m_collision_margin) {			//If close enough the touch
					min = std::min(min, dist);				//Remember the minimum distance
					addContactPoint(contact, posRW, RTOWN(face_ref->m_normalL), dist);
				}
			}
			return min;
		}

		/// <summary>
		/// We have an edge-edge contact. Find the faces of the edges that is best aligned with the collision
		/// normal (the edge edge cross product). Then clip the faces to find possible contact points.
		/// </summary>
		/// <param name="contact">Contact between the two bodies.</param>
		/// <param name="eq">Result of edge query.</param>
		void createEdgeContact(Contact& contact, EdgeQuery &eq) {
			Face* ref_face = maxFaceAlignment(eq.m_normalL, eq.m_edge_ref->m_edge_face_ptrs, fabs);	//face of A best aligned with the contact normal
			Face* inc_face = maxFaceAlignment(-RTOIN(eq.m_normalL), eq.m_edge_inc->m_edge_face_ptrs, fabs);	//face of B best aligned with the contact normal
		
			real dp_ref = fabs(glm::dot(eq.m_normalL, ref_face->m_normalL));	//Use the better aligned face as reference face.
			real dp_inc = fabs(glm::dot(eq.m_normalL, inc_face->m_normalL));
			if (dp_inc > dp_ref) { 
				std::swap(contact.m_body_ref, contact.m_body_inc);	//Use incident face as reference face -> swap positions
				std::swap(ref_face, inc_face);
				std::swap(eq.m_edge_ref, eq.m_edge_inc);
			}
			real sep = clipFaceFace(contact, ref_face, inc_face);		//Project and clip faces			
			positionBias(eq.m_separation, sep, eq.m_normalL, contact);	//Add possibe position bias
		}


	public:
		///Constructor of class EventListenerCollision
		VEEventListenerPhysics(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~VEEventListenerPhysics() {};
	};


	class VEEventListenerPhysicsGUI : public VEEventListener
	{
	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrender_Nuklear* pSubrender = (VESubrender_Nuklear*)getEnginePointer()->getRenderer()->getOverlay();
			if (pSubrender == nullptr)
				return;

			struct nk_context* ctx = pSubrender->getContext();

			/* GUI */
			if (nk_begin(ctx, "Physics Panel", nk_rect(20, 20, 450, 650),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
			{
				std::stringstream str;
				str << std::setprecision(5);

				nk_layout_row_dynamic(ctx, 60, 2);
				if (nk_option_label(ctx, "Solver A", m_physics->m_solver == 0)) m_physics->m_solver = 0;
				if (nk_option_label(ctx, "Solver B", m_physics->m_solver == 1)) m_physics->m_solver = 1;

				str << "Sim Freq " << m_physics->m_sim_frequency;
				nk_layout_row_dynamic(ctx, 30, 4);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-10")) {
					m_physics->m_sim_frequency = std::max(10.0_real, (real)m_physics->m_sim_frequency - 10.0_real);
					m_physics->m_sim_delta_time = 1.0_real / m_physics->m_sim_frequency;
				}
				if (nk_button_label(ctx, "+10")) {
					m_physics->m_sim_frequency += 10;
					m_physics->m_sim_delta_time = 1.0_real / m_physics->m_sim_frequency;
				}
				if (nk_button_label(ctx, "Next time slot")) {
					m_physics->m_current_time += m_physics->m_sim_delta_time;
				}

				nk_layout_row_dynamic(ctx, 30, 2);
				if (nk_option_label(ctx, "Realtime", m_physics->m_mode == VEEventListenerPhysics::simulation_mode_t::SIMULATION_MODE_REALTIME))
					m_physics->m_mode = VEEventListenerPhysics::simulation_mode_t::SIMULATION_MODE_REALTIME;
				if (nk_option_label(ctx, "Debug", m_physics->m_mode == VEEventListenerPhysics::simulation_mode_t::SIMULATION_MODE_DEBUG))
					m_physics->m_mode = VEEventListenerPhysics::simulation_mode_t::SIMULATION_MODE_DEBUG;

				nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, "Time (s)", NK_TEXT_LEFT);
				str.str("");
				str << m_physics->m_current_time;
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				nk_layout_row_end(ctx);

				static real fps = 0.0;
				fps = 0.05_real * m_physics->m_fps + 0.95_real * fps;
				str.str("");
				str << "FPS " << fps;
				nk_layout_row_dynamic(ctx, 30, 1);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);

				str.str("");
				str << "Loops " << m_physics->m_loops;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-5")) { m_physics->m_loops = std::max(5, m_physics->m_loops - 5); }
				if (nk_button_label(ctx, "+5")) { m_physics->m_loops += 5; }

				str.str("");
				str << "Resting Fac " << m_physics->m_resting_factor;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-0.2")) { m_physics->m_resting_factor = std::max(0.2_real, m_physics->m_resting_factor - 0.2_real); }
				if (nk_button_label(ctx, "+0.2")) { m_physics->m_resting_factor += 0.2_real; }

				str.str("");
				str << "Damp Incr " << m_physics->m_damping_incr;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-5")) { m_physics->m_damping_incr = std::max(0.0_real, m_physics->m_damping_incr - 5.0_real); }
				if (nk_button_label(ctx, "+5")) { m_physics->m_damping_incr += 5.0_real; }

				str.str("");
				str << "PBias Fac " << m_physics->m_pbias_factor;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-0.1")) { m_physics->m_pbias_factor = glm::clamp(m_physics->m_pbias_factor - 0.1_real, 0.0_real, 1.0_real); }
				if (nk_button_label(ctx, "+0.1")) { m_physics->m_pbias_factor = glm::clamp(m_physics->m_pbias_factor + 0.1_real, 0.0_real, 1.0_real); }

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Align PBias", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_align_position_bias == 1))
					m_physics->m_align_position_bias = 1;
				if (nk_option_label(ctx, "No", m_physics->m_align_position_bias == 0))
					m_physics->m_align_position_bias = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Use VBias", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_use_vbias == 1))
					m_physics->m_use_vbias = 1;
				if (nk_option_label(ctx, "No", m_physics->m_use_vbias == 0))
					m_physics->m_use_vbias = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Warmstart All", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_use_warmstart == 1))
					m_physics->m_use_warmstart = 1;
				if (nk_option_label(ctx, "No", m_physics->m_use_warmstart == 0))
					m_physics->m_use_warmstart = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Warmstart Single", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_use_warmstart_single == 1))
					m_physics->m_use_warmstart_single = 1;
				if (nk_option_label(ctx, "No", m_physics->m_use_warmstart_single == 0))
					m_physics->m_use_warmstart_single = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Deactivate", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_deactivate))
					m_physics->m_deactivate = true;
				if (nk_option_label(ctx, "No", !m_physics->m_deactivate))
					m_physics->m_deactivate = false;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Clamp Pos", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", m_physics->m_clamp_position == 1))
					m_physics->m_clamp_position = 1;
				if (nk_option_label(ctx, "No", m_physics->m_clamp_position == 0))
					m_physics->m_clamp_position = 0;

				str.str("");
				str << "Num Bodies " << m_physics->m_bodies.size();
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "Create Bodies")) {
					m_physics->createRandomBodies(20);
				}
				if (nk_button_label(ctx, "Clear Bodies")) {
					m_physics->clear();
				}

				str.str("");
				str << "Num Contacts " << m_physics->m_contacts.size();
				nk_layout_row_dynamic(ctx, 30, 1);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);

				str.str("");
				str << "Num Active " << m_physics->m_num_active;
				nk_layout_row_dynamic(ctx, 30, 1);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);

				str.str("");
				str << "Cell width " << m_physics->m_width;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-1")) {
					m_physics->m_grid.clear();
					m_physics->m_width = std::max(1.0_real, m_physics->m_width - 1);
					for (auto body : m_physics->m_bodies) m_physics->addGrid(body.second);
				}
				if (nk_button_label(ctx, "+1")) {
					m_physics->m_grid.clear();
					m_physics->m_width++;
					for (auto body : m_physics->m_bodies) m_physics->addGrid(body.second);
				}

				nk_layout_row_dynamic(ctx, 30, 3);
				str.str("Current Body ");
				if (m_physics->m_body) { str << "Current Body " << m_physics->m_body->m_name; }
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "Pick body")) { 
					m_physics->m_body = m_physics->pickBody(); 
				}
				if (nk_button_label(ctx, "Delete body")) {
					auto b = m_physics->pickBody();
					if(b) m_physics->eraseBody(b);
				}

				real vel = 5.0;
				real m_dx, m_dy, m_dz, m_da, m_db, m_dc;
				m_dx = m_dy = m_dz = m_da = m_db = m_dc = 0.0;

				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "+X")) {m_dx = vel; }
				if (nk_button_label(ctx, "-X")) { m_dx = -vel; }
				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "+Y")) { m_dy = vel; }
				if (nk_button_label(ctx, "-Y")) { m_dy = -vel; }
				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "+Z")) { m_dz = vel; }
				if (nk_button_label(ctx, "-Z")) {m_dz = -vel; }

				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "RX")) { m_da = vel; }
				if (nk_button_label(ctx, "-RX")) { m_da = -vel; }
				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "+RY")) { m_db = vel; }
				if (nk_button_label(ctx, "-RY")) { m_db = -vel; }
				nk_layout_row_static(ctx, 30, 100, 2);
				if (nk_button_label(ctx, "+RZ")) { m_dc = vel; }
				if (nk_button_label(ctx, "-RZ")) { m_dc = -vel; }

				if (m_physics->m_body) {
					real dt = (real)m_physics->m_sim_delta_time;
					m_physics->m_body->m_positionW += (real)dt * glmvec3{ m_dx, m_dy, m_dz };
					m_physics->m_body->m_orientationLW =
						glm::rotate(glmquat{ 1,0,0,0 }, (real)dt * m_da, glmvec3{ 1, 0, 0 }) *
						glm::rotate(glmquat{ 1,0,0,0 }, (real)dt * m_db, glmvec3{ 0, 1, 0 }) *
						glm::rotate(glmquat{ 1,0,0,0 }, (real)dt * m_dc, glmvec3{ 0, 0, 1 }) * m_physics->m_body->m_orientationLW;

					m_physics->m_body->updateMatrices();
					m_dx = m_dy = m_dz = m_da = m_db = m_dc = 0.0_real;
				}
			}
			nk_end(ctx);
		}


	public:
		VEEventListenerPhysics* m_physics;

		int m_zoom = 0; ///<example data

		///Constructor of class VEEventListenerPhysicsGUI
		VEEventListenerPhysicsGUI(std::string name, VEEventListenerPhysics* phy) : VEEventListener{ name }, m_physics{ phy } {};

		///Destructor of class VEEventListenerPhysicsGUI
		virtual ~VEEventListenerPhysicsGUI() {};
	};


	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(veRendererType type = veRendererType::VE_RENDERER_TYPE_FORWARD, bool debug=false) : VEEngine(type, debug) {};
		~MyVulkanEngine() {};

		VEEventListenerPhysics *m_physics;
		VEEventListenerPhysicsGUI* m_physics_gui;

		///Register an event listener to interact with the user
		
		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(m_physics = new VEEventListenerPhysics("Physics"), { veEvent::VE_EVENT_FRAME_STARTED, veEvent::VE_EVENT_KEYBOARD, veEvent::VE_EVENT_FRAME_ENDED });
			registerEventListener(m_physics_gui = new VEEventListenerPhysicsGUI("PhysicsGUI", m_physics), { veEvent::VE_EVENT_DRAW_OVERLAY });
		};
		

		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel( uint32_t numLevel=1) {

			VEEngine::loadLevel(numLevel );			//create standard cameras and lights

			VESceneNode *pScene;
			VECHECKPOINTER( pScene = getSceneManagerPointer()->createSceneNode("Level 1", getRoot()) );
	
			//scene models

			VESceneNode *sp1;
			VECHECKPOINTER( sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
										{	"bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", 
											"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene)  );

			VESceneNode *e4;
			VECHECKPOINTER( e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test/plane", "plane_t_n_s.obj",0, pScene) );
			e4->setTransform(glm::scale( glm::translate( glm::vec3{ 0,0,0,}) , glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity *pE4;
			VECHECKPOINTER( pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0") );
			pE4->setParam( glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f) );

			getSceneManagerPointer()->getSceneNode("StandardCameraParent")->setPosition({0,1,-4});

			//VESceneNode *e1,*eParent;
			//eParent = getSceneManagerPointer()->createSceneNode("The Cube Parent", pScene, glm::mat4(1.0));
			//VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube0", "media/models/test/crate0", "cube.obj"));
			//eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 1.0f, 10.0f)));
			//eParent->addChild(e1);
		};
	};


}


//-------------------------------------------------------------------------------------------------------

using namespace ve;

namespace geometry {

	//https://en.wikipedia.org/wiki/Skew_lines#Nearest_points
	/// <summary>
	/// </summary>
	/// <param name=""</param>
	std::pair<glmvec3, glmvec3> closestPointsLineLine(glmvec3 p1, glmvec3 a, glmvec3 p2, glmvec3 b) {
		glmvec3 d1 = a - p1;
		glmvec3 d2 = b - p2;
		glmvec3 n = glm::cross(d1, d2);
		glmvec3 n1 = glm::cross(d1, n);
		glmvec3 n2 = glm::cross(d2, n);
		real t1 = glm::clamp(glm::dot(p2 - p1, n2) / glm::dot(d1, n2), 0.0_real, 1.0_real);
		real t2 = glm::clamp(glm::dot(p1 - p2, n1) / glm::dot(d2, n1), 0.0_real, 1.0_real);
		return {p1 + t1*d1, p2 + t2*d2};
	}

	//https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
	real distancePointLine(glmvec3 p, glmvec3 a, glmvec3 b) {
		glmvec3 n = glm::normalize( b - a );
		return glm::length( (p - a) - glm::dot( p - a, n ) * n );
	}

	//https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
	real distancePointLinesegment(glmvec3 p, glmvec3 a, glmvec3 b) {
		return glm::clamp(distancePointLine(p,a,b), glm::length(p-a), glm::length(p - b));
	}

	//https://box2d.org/posts/2014/02/computing-a-basis/
	void computeBasis(const glmvec3& a, glmvec3& b, glmvec3& c)
	{
		// Suppose vector a has all equal components and is a unit vector:
		// a = (s, s, s)
		// Then 3*s*s = 1, s = sqrt(1/3) = 0.57735. This means that at
		// least one component of a unit vector must be greater or equal
		// to 0.57735.

		if (fabs(a.x) >= 0.57735)
			b = glmvec3(a.y, -a.x, 0.0);
		else
			b = glmvec3(0.0, a.z, -a.y);

		b = glm::normalize(b);
		c = glm::cross(a, b);
	}


	//https://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping
	//rewritten for std::vector

	using namespace std;

	// check if a point is on the RIGHT side of an edge
	bool inside(glmvec2 p, glmvec2 p1, glmvec2 p2) {
		return (p2.y - p1.y) * p.x + (p1.x - p2.x) * p.y + (p2.x * p1.y - p1.x * p2.y) >= 0;
	}

	// calculate intersection point
	glmvec2 intersection(glmvec2 cp1, glmvec2 cp2, glmvec2 s, glmvec2 e) {
		glmvec2 dc = { cp1.x - cp2.x, cp1.y - cp2.y };
		glmvec2 dp = { s.x - e.x, s.y - e.y };

		real n1 = cp1.x * cp2.y - cp1.y * cp2.x;
		real n2 = s.x * e.y - s.y * e.x;
		real n3 = 1.0_real / (dc.x * dp.y - dc.y * dp.x);

		return { (n1 * dp.x - n2 * dc.x) * n3, (n1 * dp.y - n2 * dc.y) * n3 };
	}

	// Sutherland-Hodgman clipping
	//https://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping#C.2B.2B
	template<typename T>
	void SutherlandHodgman(T& subjectPolygon, T& clipPolygon, T& newPolygon) {
		glmvec2 cp1, cp2, s, e;
		std::vector<glmvec2> inputPolygon;
		newPolygon = subjectPolygon;

		for (int j = 0; j < clipPolygon.size(); j++)
		{
			// copy new polygon to input polygon & set counter to 0
			inputPolygon = newPolygon;
			newPolygon.clear();

			// get clipping polygon edge
			cp1 = clipPolygon[j];
			cp2 = clipPolygon[(j + 1) % clipPolygon.size()];

			for (int i = 0; i < inputPolygon.size(); i++)
			{
				// get subject polygon edge
				s = inputPolygon[i];
				e = inputPolygon[(i + 1) % inputPolygon.size()];

				// Case 1: Both vertices are inside:
				// Only the second vertex is added to the output list
				if (inside(s, cp1, cp2) && inside(e, cp1, cp2)) {
					newPolygon.emplace_back(e);
				}

				// Case 2: First vertex is outside while second one is inside:
				// Both the point of intersection of the edge with the clip boundary
				// and the second vertex are added to the output list
				else if (!inside(s, cp1, cp2) && inside(e, cp1, cp2)) {
					newPolygon.emplace_back(intersection(cp1, cp2, s, e));
					newPolygon.emplace_back(e);
				}

				// Case 3: First vertex is inside while second one is outside:
				// Only the point of intersection of the edge with the clip boundary
				// is added to the output list
				else if (inside(s, cp1, cp2) && !inside(e, cp1, cp2)) {
					newPolygon.emplace_back(intersection(cp1, cp2, s, e));
				}
				// Case 4: Both vertices are outside
				else if (!inside(s, cp1, cp2) && !inside(e, cp1, cp2)) {
					// No vertices are added to the output list
				}
			}
		}
	}


	//-------------------------------------------------------------------------------------------------------
	//Test SutherlandHodgman

	int testSutherlandHodgman()
	{
		// subject polygon
		std::vector<glmvec2> subjectPolygon {
		{50,150}, {200,50}, {350,150},
			{350,300},{250,300},{200,250},
			{150,350},{100,250},{100,200}
		};

		// clipping polygon
		std::vector<glmvec2> clipPolygon { {100,100}, {300,100}, {300,300}, {100,300} };

		// define the new clipped polygon (empty)
		std::vector<glmvec2> newPolygon;

		// apply clipping
		SutherlandHodgman(subjectPolygon, clipPolygon, newPolygon);

		// print clipped polygon points
		cout << "Clipped polygon points:" << endl;
		for (int i = 0; i < newPolygon.size(); i++)
			cout << "(" << newPolygon[i].x << ", " << newPolygon[i].y << ")" << endl;

		return 0;
	}

}


//-------------------------------------------------------------------------------------------------------


int main() {
	bool debug = true;

	MyVulkanEngine mve(veRendererType::VE_RENDERER_TYPE_FORWARD, debug);	//enable or disable debugging (=callback, validation layers)
	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	//geometry::testSutherlandHodgman();		//test clipping

	return 0;
}
