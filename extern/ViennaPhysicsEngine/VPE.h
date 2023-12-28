/**
* The Vienna Physics Engine
*
* (c) bei Helmut Hlavacs, University of Vienna, 2022
*
*/

#pragma once

#include <algorithm>
#include <vector>
#include <cstdio>
#include <iterator>
#include <ranges>
#include <string>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <random>
#include <chrono>
#include <set>
#include <functional>

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

#define VPE_SINGLE_ACCURACY
//#define VPE_DOUBLE_ACCURACY

#ifdef VPE_DOUBLE_ACCURACY
using real = double;
using int_t = int64_t;
using uint_t = uint64_t;
#define glmvec2 glm::dvec2
#define glmvec3 glm::dvec3
#define glmmat2 glm::dmat2
#define glmmat3 glm::dmat3
#define glmvec4 glm::dvec4
#define glmmat4 glm::dmat4
#define glmquat glm::dquat
const real c_eps = 1.0e-12;
#else
#ifdef VPE_SINGLE_ACCURACY
using real = float;
using int_t = int32_t;
using uint_t = uint32_t;
#define glmvec2 glm::vec2
#define glmvec3 glm::vec3
#define glmvec4 glm::vec4
#define glmmat2 glm::mat2
#define glmmat3 glm::mat3
#define glmmat4 glm::mat4
#define glmquat glm::quat
const real c_eps = 1.0e-8f;
const real pi = glm::pi<real>();
const real pi2 = 2.0f * pi;
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
	void computeBasis(const glmvec3& a, glmvec3& b, glmvec3& c);
	void SutherlandHodgman(auto& subjectPolygon, auto& clipPolygon, auto& newPolygon);
	glmvec3 orthoUnitVector(const glmvec3& vec);

	//----------------------------------Cloth-Simulation-Stuff--------------------------------------
	// by Felix Neumann
	// Defintion and source below this file
	real alphaMaxPlusBetaMin(real a, real b);
	real alphaMaxPlusBetaMedPlusGammaMin(real a, real b, real c);
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
			size_t seed = std::hash<void*>()(std::min(p.first, p.second));	//Makes sure the smaller one is always first
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
	inline ostream& operator<<(ostream& os, const glmvec3& v) {
		os << "(" << v.x << ',' << v.y << ',' << v.z << ")";				//output 3D vector
		return os;
	}

	//For outputting vectors/matrices to a string stream
	inline ostream& operator<<(ostream& os, const glmvec2& v) {
		os << "(" << v.x << ',' << v.y << ")";								//output 2D vector
		return os;
	}

	inline ostream& operator<<(ostream& os, const glmquat& q) {
		os << "(" << q.x << ',' << q.y << ',' << q.z << ',' << q.w << ")";	//output quaternion
		return os;
	}

	inline ostream& operator<<(ostream& os, const glmmat3& m) {
		os << "(" << m[0][0] << ',' << m[0][1] << ',' << m[0][2] << ")\n";	//Output a 3x3 matrix
		os << "(" << m[1][0] << ',' << m[1][1] << ',' << m[1][2] << ")\n";
		os << "(" << m[2][0] << ',' << m[2][1] << ',' << m[2][2] << ")\n";
		return os;
	}

	inline ostream& operator<<(ostream& os, const glmmat4& m) {
		os << "(" << m[0][0] << ',' << m[0][1] << ',' << m[0][2] << ',' << m[0][3] << ")\n";	//Output a 4x4 matrix
		os << "(" << m[1][0] << ',' << m[1][1] << ',' << m[1][2] << ',' << m[1][3] << ")\n";
		os << "(" << m[2][0] << ',' << m[2][1] << ',' << m[2][2] << ',' << m[2][3] << ")\n";
		os << "(" << m[3][0] << ',' << m[3][1] << ',' << m[3][2] << ',' << m[3][3] << ")\n";
		return os;
	}

	inline std::string to_string(const glmvec3 v) {
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


namespace vpe {

	/// <summary>
	/// This class  implements a simple rigid body physics engine.
	/// </summary>
	class VPEWorld {

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
		/// Vertices, edges and faces. We use a left handed space. Edges can have signs, meaning
		/// their vectors are multiplied with this factor (1 or -1). 
		/// </summary>
		inline static Polytope g_cube {
			{ { -0.5_real,-0.5_real,-0.5_real }, { -0.5_real,-0.5_real,0.5_real }, { -0.5_real,0.5_real,0.5_real }, { -0.5_real,0.5_real,-0.5_real },
			{ 0.5_real,-0.5_real,0.5_real }, { 0.5_real,-0.5_real,-0.5_real }, { 0.5_real,0.5_real,-0.5_real }, { 0.5_real,0.5_real,0.5_real } },
			{ {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {5,0}, {1,4}, {3,6}, {7,2} }, //edges
			{	{ {0}, {1}, {2}, {3} },					//face 0
				{ {4}, {5}, {6}, {7} },					//face 1
				{ {0,-1}, {8,-1},  {4,-1}, {9,-1} },	//face 2
				{ {2,-1}, {11,-1}, {6,-1}, {10,-1} },	//face 3
				{ {3,-1}, {10},    {5,-1}, {8} },		//face 4
				{ {1,-1}, {9},     {7,-1}, {11} }		//face 5
			},
			[&](real mass, glmvec3& s) { //callback for calculating the inertia tensor of this polytope
				return mass * glmmat3{ {s.y * s.y + s.z * s.z,0,0}, {0,s.x * s.x + s.z * s.z,0}, {0,0,s.x * s.x + s.y * s.y} } / 12.0_real;
			}
		};

		//--------------------------------------------------------------------------------------------------
		//Physics engine stuff

		class Body;
		using callback_move = std::function<void(double, std::shared_ptr<Body>)>; //call this function when the body moves
		using callback_erase = std::function<void(std::shared_ptr<Body>)>; //call this function when the body moves
		using callback_collide = std::function<void(std::shared_ptr<Body>, std::shared_ptr<Body>)>; //call this function when the body moves

		/// <summary>
		/// This class implements the basic physics properties of a rigid body.
		/// </summary>
		class Body {

		public:
			VPEWorld* m_physics;			//Pointer to the physics world to access parameters
			std::string	m_name;							//The name of this body

			//-----------------------------------------------------------------------------
			//Physics parameters of the body

			void*		m_owner = nullptr;				//pointer to owner of this body, must be unique (owner is called if body moves)
			Polytope*	m_polytope = nullptr;			//geometric shape
			glmvec3		m_scale{ 1,1,1 };				//scale factor in local space
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmquat		m_orientationLW{ 1, 0, 0, 0 };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };	//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space
			callback_move  m_on_move = nullptr;			//called if the body moves
			callback_erase m_on_erase = nullptr;			//called if the body is erased
			real		m_mass_inv{ 0 };				//1 over mass
			real		m_restitution{ 0 };				//coefficient of restitution eps
			real		m_friction{ 1 };				//coefficient of friction mu
			uint64_t	m_loop_last_active{ 0 };		//The loop number in which this body was last time active

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
			Body(VPEWorld* physics) : m_physics{ physics } { m_polytope = &m_physics->g_cube; inertiaTensorL(); updateMatrices(); };

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
			/// <param name="on_erase">Callback that is called when the body is erased.</param>
			/// <param name="linear_velocityW">Starting linear velocity.</param>
			/// <param name="angular_velocityW">Starting angular velovity as axis vector. Length of vector is the speed.</param>
			/// <param name="mass_inv">1 / mass. If zero, then mass is infinite.</param>
			/// <param name="restitution">Bounciness, between 0 and 1.</param>
			/// <param name="friction">Friction coefficient, usually larger than 0.5.</param>
			Body(VPEWorld* physics, std::string name, void* owner, Polytope* polytope,
				glmvec3 scale, glmvec3 positionW, glmquat orientationLW = { 1,0,0,0 },
				glmvec3 linear_velocityW = glmvec3{ 0,0,0 }, glmvec3 angular_velocityW = glmvec3{ 0,0,0 },
				real mass_inv = 0, real restitution = 0.2_real, real friction = 1) :
				m_physics{ physics }, m_name{ name }, m_owner{ owner }, m_polytope{ polytope },
				m_scale{ scale }, m_positionW{ positionW }, m_orientationLW{ orientationLW },
				m_linear_velocityW{ linear_velocityW }, m_angular_velocityW{ angular_velocityW },
				m_mass_inv{ mass_inv }, m_restitution{ restitution }, m_friction{ friction } {
					m_scale *= m_physics->m_collision_margin_factor;
					inertiaTensorL();
					updateMatrices();
					m_loop_last_active = m_physics->m_loop;
			};

			/// <summary>
			/// Attach a force to the body or change it.
			/// </summary>
			/// <param name="id">The force ID.</param>
			/// <param name="force">The force itself.</param>
			template<typename F>
			void setForce(uint64_t id, F&& force) {
				m_forces[id] = std::forward<F>(force);
			}

			/// <summary>
			/// Remove a force.
			/// </summary>
			/// <param name="id">Force id.</param>
			void removeForce(uint64_t id) {
				m_forces.erase(id);
			}

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

				auto avL = glmmat3{ m_model_inv } *m_angular_velocityW;	//The same for orientation
				real len = glm::length(avL);
				if (len > m_physics->c_small) {
					if (m_physics->m_clamp_position == 1) quat = glm::rotate(quat, len * (real)dt, avL / len);	//Euler step
					active = true;
				}
				if (m_physics->m_clamp_position == 0 && len != 0.0_real) quat = glm::rotate(quat, len * (real)dt, avL / len); //Euler step

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
		real	m_pbias_factor = 0.3_real;					//Add only a fraction of the current position bias.
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
		/// Wrapper class that provides parts of a std::unordered_map like interface while using a std::vector as the underlying data structure.
		/// As inserting, erasing and targeted lookup for bodies are barely used and the main use case of this data structure is linear iteration
		/// std::vector gives much better perfomance when compared to std::unordered:map. However, the map interface is much more convenient to use, thus this wrapper class.
		/// Note: Unlike maps, this does not guarantee that only one entry per owner is present. Lookup returns the first element for a given owner
		/// </summary>
		template<typename key_type, typename mapped_type>
		class MapWrapper {
			using pair_t = std::pair<key_type, mapped_type>;
			std::vector<pair_t> m_vector;
		public:
			MapWrapper() {}
			MapWrapper(std::initializer_list<pair_t> elements) {
				for (const auto& entry : elements) {
					this->insert(entry);
				}
			}

			typename std::vector<pair_t>::iterator insert(const pair_t& pair) {
				return m_vector.insert(m_vector.end(), pair);
			}

			typename std::vector<pair_t>::iterator insert(pair_t&& pair) {
				return m_vector.insert(m_vector.end(), pair);
			}

			void push_back(const pair_t& pair) {
				m_vector.push_back(pair);
			}

			void push_back(pair_t&& pair) {
				m_vector.push_back(pair);
			}

			size_t erase(const key_type& key) {
				return std::erase_if(m_vector, [&key](pair_t element) {
					return element.first == key;
				});
			}

			typename std::vector<pair_t>::iterator find(const key_type& key) const {
				return std::find_if(m_vector.begin(), m_vector.end(), [&key](auto pair) { return pair.first == key; });
			}

			pair_t& operator [] (const key_type& key) const {
				auto element = this->find(key);
				if (element != m_vector.end()) {
					return *element;
				}
				else {
					throw std::out_of_range("Attempted element lookup failed, key is not in vector.");
				}
			}

			pair_t& at(const key_type& key) const {
				return (*this)[key];
			}

			void clear() {
				m_vector.clear();
			}

			typename std::vector<pair_t>& get_vector() const {
				return m_vector;
			}

			typename std::vector<pair_t>::iterator begin() {
				return m_vector.begin();
			}

			typename std::vector<pair_t>::iterator end() {
				return m_vector.end();
			}

			typename std::vector<pair_t>::const_iterator begin() const {
				return m_vector.begin();
			}

			typename std::vector<pair_t>::const_iterator end() const {
				return m_vector.end();
			}

			size_t size() const {
				return m_vector.size();
			}

			void reserve(size_t size) {
				m_vector.reserve(size);
			}
		};

		/// <summary>
		/// All bodies are stored in the map m_bodies. The key is a void*, which can be used 
		/// to call back an owner if the body moves. With this key, the body can also be found.
		/// So best if there is a 1:1 correspondence. E.g., the owner can be a specific VESceneNode.
		/// </summary>
		using body_map = MapWrapper<void*, std::shared_ptr<Body>>;
		body_map	m_bodies;			//main container of all bodies
		uint64_t	m_body_id{ 0 };		//Unique id for body if needed

		/// <summary>
		/// The broadphase uses a 2D grid of cells, each body is stored in exactly one cell.
		/// Only cells which actually contain bodies are stored in the map.
		/// </summary>
		real		m_width{ 3 };								//grid cell width (m)
		std::unordered_map<intpair_t, body_map > m_grid;	//broadphase grid of cells.

		std::shared_ptr<Body> m_ground = std::make_shared<Body>(Body{ this, "Ground", nullptr, &g_cube, {1000, 1000, 1000}, {0, -500.0_real, 0}, {1,0,0,0} });
		body_map	m_global_cell{ { nullptr, m_ground } };	//cell containing only the ground

		/// <summary>
		/// A contact is a struct that contains contact information for a pair of bodies A and B.
		/// The map hash function alsways uses the smaller of A and B first, so there is only one contact for
		/// A/B and B/A.
		/// </summary>
		std::unordered_map<voidppair_t, Contact> m_contacts;	//possible contacts resulting from broadphase

		std::unordered_map<void*, callback_collide> m_collider;	//Call these callbacks if there is a collision for a specific body

		/// <summary>
		///  Holds all constraints 
		/// </summary>
		class Constraint;
		std::vector<std::shared_ptr<Constraint>> m_constraints;

		//-----------------------------------------------------------------------------------------------------

		/// <summary>
		/// Pick the body that is best aligned with the camera lokking direction.
		/// </summary>
		/// <returns>The picked body.</returns>
		std::shared_ptr<Body> pickBody( glmvec3 pos, glmvec3 dir) {
			auto compare = [&](auto& a, auto& b) { return glm::dot(glm::normalize(a.second->m_positionW - pos), dir) < glm::dot(glm::normalize(b.second->m_positionW - pos), dir); };
			return std::ranges::max_element(m_bodies, compare)->second;
		}
		std::shared_ptr<Body> m_body; // the body we can move with the debug panel (always the latest body created)


		/// <summary>
		/// Add a body to the 2D broadphase grid. This is a separate function, so we can later change the grid width.
		/// </summary>
		/// <param name="pbody">The body to add.</param>
		void addGrid(auto pbody) {
			pbody->m_grid_x = static_cast<int_t>(pbody->m_positionW.x / m_width);	//2D coordinates in the broadphase grid
			pbody->m_grid_z = static_cast<int_t>(pbody->m_positionW.z / m_width);
			m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].insert({ pbody->m_owner, pbody }); //Put into broadphase grid
		}

		/// <summary>
		/// Add a new body to the physics world.
		/// </summary>
		/// <param name="pbody">The new body.</param>
		void addBody(auto pbody) {
			m_body = pbody;
			m_bodies.insert({ pbody->m_owner, pbody });	//Put into body container
			addGrid(pbody);	//add to broadphase grid.
			pbody->updateMatrices();
			++m_body_id;
		}

		/// <summary>
		/// Retrieve a body using the owner.
		/// </summary>
		/// <param name="owner">Pointer to the owner</param>
		/// <returns>Shared pointer to the body.</returns>
		auto getBody(auto* owner) {
			return m_bodies[(void*)owner];
		}

		/// <summary>
		/// Delete all bodies.
		/// </summary>
		void clear() {
			for (  std::pair<void*, std::shared_ptr<Body>> body : m_bodies) {
				auto b = body.second;
				if (b->m_on_erase) {	//if there is a callback for removing the owner
					b->m_on_erase(b);	//call it first
				}
			}
			m_constraints.clear();
			m_collider.clear();
			m_bodies.clear();
			m_grid.clear();
		}

		/// <summary>
		/// Erase one body.
		/// </summary>
		/// <param name="body">(Shared) pointer to the body.</param>
		void eraseBody(std::shared_ptr<Body> body) {
			if (body->m_on_erase) body->m_on_erase(body);
			m_collider.erase(body->m_owner);
			m_bodies.erase(body->m_owner);
			m_grid[intpair_t{ body->m_grid_x, body->m_grid_z }].erase(body->m_owner);
			for (auto it = m_constraints.begin(); it != m_constraints.end();) {
				if ((*it)->containsBody(body)) {
					it = m_constraints.erase(it);
				}
				else ++it;
			}

		}

		/// <summary>
		/// Erase one body.
		/// </summary>
		/// <param name="owner">A void pointer to the owner of the body.</param>
		void eraseBody(auto* owner) {
			std::shared_ptr<Body> body = m_bodies[(void*)owner];
			if (body->m_on_erase) body->m_on_erase(body);
			m_collider.erase(body->m_owner);
			m_bodies.erase(owner);
			m_grid[intpair_t{ body->m_grid_x, body->m_grid_z }].erase(body->m_owner);
		}

		void addCollider(std::shared_ptr<Body> body, callback_collide collider ) {
			m_collider[body->m_owner] = collider;
		}

		void eraseCollider(std::shared_ptr<Body> body) {
			m_collider.erase(body->m_owner);
		}

		void clearCollider() {
			m_collider.clear();
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
		void addPositionBias(glmvec3& current_bias, glmvec3& new_bias) {
			auto B = new_bias;								//temp value
			if (m_align_position_bias == 1) {				//Should we align thenew bias with the old one?
				auto l = glm::length(current_bias);			//Length of current bias
				if (l > 0.0_real) {							//if there is a current bias
					auto f = std::max(glm::dot(new_bias, current_bias / l) - l, 0.0_real); //how much is already in this direction?
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
		void positionBias(real query_separation, real face_separation, glmvec3 normalL, Contact& contact) {
			if (query_separation < m_collision_margin) {
				real weight = 1.0_real / (1.0_real + contact.m_body_inc.m_body->mass() * contact.m_body_ref.m_body->m_mass_inv);
				auto pbias = -RTOWN(normalL) * (-query_separation) * (real)m_sim_frequency * (1.0_real - weight);
				addPositionBias(contact.m_body_ref.m_body->m_pbias, pbias);
				pbias = RTOWN(normalL) * (-query_separation) * (real)m_sim_frequency * weight;
				addPositionBias(contact.m_body_inc.m_body->m_pbias, pbias);
			}
		}


		//--------------------------------------------------------------------------------------------------------
		/// <summary>
		/// Callback for the frame started event. This is the MAIN physics engine entry point!
		/// Once at the start of each new frame this is called.
		/// The function calculates the current collision/contact points, and warmstarts resting contacts.
		/// Time is forwarded in fixed slots. If the clock falls in between two slots (which it always does)
		/// then the state of the world at this time is forward extrapolated.
		/// </summary>
		/// <param name="event">The event data.</param>
		void tick(double dt) {
			if (m_mode == SIMULATION_MODE_REALTIME) {		//if the engine is in realtime mode, advance time
				m_current_time = m_last_time + dt;	//advance time by the time that went by since the last loop
				if (dt != 0.0) m_fps = 1.0_real / (real)dt; //estimate for frames per second
			}

			auto last_loop = m_loop;
			while (m_current_time > m_next_slot) {	//compute position/vel only at time slots
				++m_loop;				//increase loop counter
				uint_t num_active{ 0 };	//set number currently active objects to 0
				broadPhase();			//run the broad phase
				narrowPhase();			//Run the narrow phase
				warmStart();			//Warm start the resting contacts if possible

				for (auto& body : m_bodies) { body.second->stepVelocity(m_sim_delta_time); }	//Integration step for velocity
				setupConstraints(m_sim_delta_time);												//Pre-calculate values the constraints need during iteration 
				calculateImpulses(m_loops, m_sim_delta_time);									//Calculate and apply impulses (also solve constraints here)

				for (auto& body : m_bodies) {	//integrate positions and update the matrices for the bodies
					if (body.second->stepPosition(m_sim_delta_time, body.second->m_positionW, body.second->m_orientationLW)) ++num_active;
					body.second->updateMatrices();
				}

				m_num_active = 0.9_real * m_num_active + 0.1_real * num_active; //smooth the number of active nodies
				if (m_num_active < c_small) m_num_active = 0;					//If near 0, set to 0

				//--------------------------Begin-Cloth-Simulation-Stuff----------------------------
				// by Felix Neumann

				for (auto& cloth : m_cloths)														// Integrate all cloths which means solve their constraints
					cloth.second->integrate(m_grid, (real) m_sim_delta_time);						// and resolve their collisions

				//---------------------------End-Cloth-Simulation-Stuff-----------------------------

				m_last_slot = m_next_slot;			//Remember last slot
				m_next_slot += m_sim_delta_time;	//Move to next time slot as slong as we do not surpass current time
			}
			if (m_loop > last_loop) {	//if we have entered a new time slot bodies might have moved, so update broadphase grid
				for (auto& body : m_bodies) { moveBodyInGrid(body.second); } //update grid
			}
			for (auto& body : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, this is not stored anywhere
				if (body.second->m_on_move) {
					body.second->m_on_move(m_current_time - m_last_slot, body.second); //predict new pos/orient
				}
			}

			//----------------------------Begin-Cloth-Simulation-Stuff------------------------------
			// by Felix Neumann

			for (auto& cloth : m_cloths)															// Notify the owner of the cloth that the cloth has moved
				if (cloth.second->m_on_move)
					cloth.second->m_on_move(m_current_time - m_last_slot, cloth.second);

			//-----------------------------End-Cloth-Simulation-Stuff-------------------------------

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
						bool ct = false;
						if (contact.m_body_ref.m_body->m_owner == nullptr) {
							ct = groundTest(contact);		//is the ref body the ground?
						}
						else {
							glmvec3 diff = contact.m_body_inc.m_body->m_positionW - contact.m_body_ref.m_body->m_positionW;
							real rsum = contact.m_body_ref.m_body->boundingSphereRadius() + contact.m_body_inc.m_body->boundingSphereRadius();
							if (glm::dot(diff, diff) > rsum * rsum) { ++it;  continue; }
							ct = SAT(it->second);						//yes - test it
						}
						if (ct) {
							if (m_collider.contains(contact.m_body_ref.m_body->m_owner)) m_collider[contact.m_body_ref.m_body->m_owner](contact.m_body_ref.m_body, contact.m_body_inc.m_body);
							if (m_collider.contains(contact.m_body_inc.m_body->m_owner)) m_collider[contact.m_body_inc.m_body->m_owner](contact.m_body_inc.m_body, contact.m_body_ref.m_body);
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
		bool warmStartContact(Contact& contact) {
			if (m_use_warmstart_single == 0
				|| contact.m_body_ref.m_body->m_loop_last_active + 2 > m_loop		//do not set to 1
				|| contact.m_body_inc.m_body->m_loop_last_active + 2 > m_loop
				|| contact.m_old_contact_points.size() < 3
				|| contact.m_num_resting != contact.m_old_contact_points.size()) {	//do not warmstart if non resting contact points present
				return false;
			}

			for (auto& cp : contact.m_old_contact_points) {
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

				if (num0 < 4 || num1 < 4 || contact.m_contact_points.size() == 0 || contact.m_old_contact_points.size() == 0) continue;

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
		bool groundTest(Contact& contact) {
			if (contact.m_body_inc.m_body->m_positionW.y > contact.m_body_inc.m_body->boundingSphereRadius()) return false; //early out test
			real min_depth{ std::numeric_limits<real>::max() };
			bool res = false;
			for (auto& vL : contact.m_body_inc.m_body->m_polytope->m_vertices) {
				auto vW = ITOWP(vL.m_positionL);							//world coordinates
				if (vW.y <= m_collision_margin) {							//close to the ground?
					min_depth = std::min(min_depth, vW.y);					//remember smalles y coordinate for calculating bias
					addContactPoint(contact, vW, glmvec3{ 0,1,0 }, vW.y);	//add the contact point
					res = true;
				}
			}
			positionBias(min_depth, min_depth, glmvec3{ 0,1,0 }, contact);	//add position bias if necessary
			return res;
		}

		/// <summary>
		/// For a given contact, go through all contact points and apply a small impulse to satisfy the 
		/// desired velocity. Since impulses in any loop can be negative, assure that the total impulses in
		/// normal and tangent directions do not get negative.
		/// </summary>
		/// <param name="contact">Contact manifold of two bodies.</param>
		/// <param name="contact_type">Only compute the given type of contacts.</param>
		/// <returns></returns>
		uint64_t calculateContactPointImpules(Contact& contact) {
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

				glmvec2 dt{ t0, t1 };		//make sure that aggregated tangent impulse is not negative
				auto tmpt = cp.m_t;
				cp.m_t += dt;
				auto len = glm::length(cp.m_t);
				if (len > fabs(cp.m_f * cp.m_friction)) {			//friction still allowed?
					cp.m_t *= fabs(cp.m_f * cp.m_friction) / len;	//no -> reduce to max allowed length cp.m_f * cp.m_friction
					dt = cp.m_t - tmpt;
				}

				auto F = f * contact.m_normalW - dt.x * contact.m_tangentW[0] - dt.y * contact.m_tangentW[1]; //total impulse

				contact.m_body_ref.m_body->m_linear_velocityW += -F * contact.m_body_ref.m_body->m_mass_inv;
				contact.m_body_ref.m_body->m_angular_velocityW += contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
				contact.m_body_inc.m_body->m_linear_velocityW += F * contact.m_body_inc.m_body->m_mass_inv;
				contact.m_body_inc.m_body->m_angular_velocityW += contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
			}
			return res;
		}

		/// <summary>
		/// Go through all contacts and calculate and apply impulses. Do this until number of loops or time 
		/// run out.
		/// Also solve all constraints once per iteration
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
				for (const auto& constraint : m_constraints) { //loop over all constraints
					constraint->solveVelocity();
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
			Edge* m_edge_ref;		//pointer to reference edge (body 0)
			Edge* m_edge_inc;		//pointer to incident edge (body 1)
			glmvec3	m_normalL;		//normal of contact (cross product) in A's space
		};

		/// <summary>
		/// Store the result of face-face queries
		/// </summary>
		struct FaceQuery {
			real	m_separation;	//separation length (negative)
			Face* m_face_ref;		//pointer to reference face
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
			if (minB - maxA > m_collision_margin) contact.m_separating_axisW = nW;	//Remmber separating axis
			return { minB - maxA, vertA, vertB };	//return distance and reference and incident vertex
		}

		/// <summary>
		/// Perform SAT test for two bodies. If they overlap then compute the contact manifold.
		/// </summary>
		/// <param name="contact">The contact between the two bodies.</param>
		/// https://www.gdcvault.com/play/1022193/Physics-for-Game-Programmers-Robust
		/// http://media.steampowered.com/apps/valve/2015/DirkGregorius_Contacts.pdf
		/// 
		bool SAT(Contact& contact) {
			contact.m_body_ref.m_to_other = contact.m_body_inc.m_body->m_model_inv * contact.m_body_ref.m_body->m_model; //transform to bring space A to space B
			contact.m_body_ref.m_to_other_it = glm::transpose(glm::inverse(glmmat3{ contact.m_body_ref.m_to_other }));	//transform for a normal vector
			contact.m_body_inc.m_to_other = contact.m_body_ref.m_body->m_model_inv * contact.m_body_inc.m_body->m_model; //transform to bring space B to space A
			contact.m_body_inc.m_to_other_it = glm::transpose(glm::inverse(glmmat3{ contact.m_body_inc.m_to_other }));	//transform for a normal vector

			if (contact.m_separating_axisW != glmvec3{ 0,0,0 } &&	//try old separating axis
				sat_query(contact, WTORN(contact.m_separating_axisW)).m_separation > m_collision_margin) {
				return false;
			}

			FaceQuery fq0 = queryFaceDirections(contact);			//Query all normal vectors of faces of first body
			if (fq0.m_separation > m_collision_margin) { return false; };	//found a separating axis with face normal

			std::swap(contact.m_body_ref, contact.m_body_inc);		//body 0 is the reference body having the reference face
			FaceQuery fq1 = queryFaceDirections(contact);			//Query all normal vectors of faces of second body
			if (fq1.m_separation > m_collision_margin) { return false; };	//found a separating axis with face normal

			std::swap(contact.m_body_ref, contact.m_body_inc);		//prevent flip flopping
			EdgeQuery eq = queryEdgeDirections(contact);			//Query cross product of edge pairs from body 0 and 1	
			if (eq.m_separation > m_collision_margin) { return false; }	//found a separating axis with edge-edge normal

			contact.m_separating_axisW = glmvec3{ 0,0,0 };		//no separating axis found
			if (fq0.m_separation >= eq.m_separation * 1.001_real || fq1.m_separation >= eq.m_separation * 1.001_real) {	//max separation is a face-vertex contact
				if (fq0.m_separation >= fq1.m_separation) { createFaceContact(contact, fq0); }
				else {
					std::swap(contact.m_body_ref, contact.m_body_inc);	//body 0 is the reference body having the reference face
					createFaceContact(contact, fq1);
				}
			}
			else { createEdgeContact(contact, eq); } //max separation is an edge-edge contact 
			return true;
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
			auto sat = [&](auto& face) {							//Run this function for each reference face
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
					if (n == glmvec3{ 0,0,0 }) continue;
					if (glm::dot(n, edgeA.m_first_vertexL.m_positionL) < 0)	n = -n;		//n must be oriented away from center of A								

					auto sat = sat_query(contact, n);		//Try normal to this edge-edge pair
					if (sat.m_separation > m_collision_margin) return { sat.m_separation, &edgeA, &edgeB };	//if distance positive, stop - we found a separating axis

					auto distance2 = glm::dot(n, ITORP(edgeB.m_first_vertexL.m_positionL) - sat.m_vertA->m_positionL);	//above does not depend on location - could find an adge on the other side
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
			glmvec3 An = glm::normalize(-RTOIN(fq.m_face_ref->m_normalL)); //transform normal vector of ref face to inc body
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
				auto pT = ITORTP(vertex->m_positionL);		//ransform to A's tangent space
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
				glmvec3 posIW = ITTOWP(posIT);			//Bring back to world coordinates
				auto dist = glm::dot(posIW - posRW, RTOWN(face_ref->m_normalL));	//Distance between the two points in world coordinates
				if (dist < m_collision_margin) {			//If close enough the touch
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
		void createEdgeContact(Contact& contact, EdgeQuery& eq) {
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


		// -----------------Begin Constraints Implementation -------------------------
		// by Julian Schneebaur

		// The following implementation of constraints via sequential impulses closely follow the methods shown in talks and papers by Erin Catto:
		// https://box2d.org/files/ErinCatto_IterativeDynamics_GDC2005.pdf and https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc09/slides/04-GDC09_Catto_Erin_Solver.pdf
		// For the concrete math, see: https://danielchappuis.ch/download/ConstraintsDerivationRigidBody3D.pdf

		/// <summary>
		/// Pre-compute constraint values that are needed for loop iterations
		/// This should be called once in tick() before the constraints are iterated over
		/// </summary>
		/// <param name="dt">Elapsed time</param>
		void setupConstraints(double dt) {
			for (const auto& constraint : m_constraints) {
				constraint->setUp((real)dt);
			}
		}

		/// <summary>
		/// Adds a constraint to the physics simulation
		/// </summary>
		/// <param name="constraint">Pointer to the constraint to be added</param>
		void addConstraint(std::shared_ptr<Constraint> constraint) {
			m_constraints.push_back(constraint);
		}

		/// <summary>
		/// Removes a constraint from the physics simulation
		/// </summary>
		/// <param name="constraint">Pointer to the constraint to be removed</param>
		void removeConstraint(std::shared_ptr<Constraint> constraint) {
			std::erase(m_constraints, constraint);
		}

		/// <summary>
		/// Base class for all constraints. 
		/// A constraint enforces a condition between two bodies' relative movement.
		/// It does this by computing and applying impulses that make sure the bodies only move in a way that doesn't violate the constraint
		/// </summary>
		class Constraint {
		protected:
			static constexpr real epsilon = 0.0000001_real;
			std::shared_ptr<Body> m_body1;	// First body
			std::shared_ptr<Body> m_body2;	// Second body

			// Issue: The inverted inertia tensor for objects that have infinite mass (inverse mass is 0) isn't actually a 0-matrix
			// It has some very small values, and thus just multiplying it onto the impulse would still result in very small velocities
			// If you add this up thousands of times per second, it adds up and thus allows these objects to move even though they have infinite mass
			// These values are used to solve this issue by being multiplied onto the angular velocity (either 0 or 1)
			real m_body1_factor = !(m_body1->m_mass_inv <= Constraint::epsilon); // Use this factor whenever adding angular velocity to body1
			real m_body2_factor = !(m_body2->m_mass_inv <= Constraint::epsilon); // Use this factor whenever adding angular velocity to body2
		public:
			Constraint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2) : m_body1 { body1 }, m_body2{ body2 } {}
			virtual ~Constraint() {}

			/// <summary>
			/// Computes values that are needed for subsequent loop iteration
			/// These values don't change when applying impulses during the loop, so computing them here gives us much better performance
			/// </summary>
			/// <param name="dt">Simulation time step</param>
			virtual void setUp(real dt) = 0;
			/// <summary>
			/// Computes and applies constraint forces by solving the velocity constraint
			/// </summary>
			virtual void solveVelocity() = 0;

			/// <summary>
			/// Should return true if the body is part of the constraint
			/// </summary>
			/// <param name="body">Body to check for</param>
			/// <returns></returns>
			bool containsBody(std::shared_ptr<Body> body) const {
				return body == m_body1 || body == m_body2;
			}

		};

		/// <summary>
		/// A very simple distance constraint, only acting on the linear velocities of the affected bodies
		/// Given two bodies, the constraint makes sure there is always a given distance between their center points
		/// </summary>
		class DistanceConstraint : public Constraint {
			real m_distance;							// The distance the constraint has to maintain
			real m_bias_factor = 0.1_real;				// Bias factor for Baumgarte stabilization

			// These values are computed once in setUp() before each constraint loop, as they don't change between iterations
			glmvec3 m_rel_pos{ 0.0_real };				// Relative position of the bodies
			real m_body_distance = 0.0_real;			// Distance between bodies
			real m_offset = 0.0_real;					// The constraint error
			glmvec3 m_j1{ 0.0_real };					// First entry of jacobian matrix
			glmvec3 m_j2{ 0.0_real };					// Second entry of jacobian matrix
			real m_inv_constraint_mass = 0.0_real;		// Inverse effective constraint mass
			real m_bias = 0.0_real;						// Bias for Baumgarte stabilization
		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="body1">First body</param>
			/// <param name="body2">Second body</param>
			/// <param name="distance">Distance the constraint should maintain</param>
			DistanceConstraint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2, real distance) : Constraint(body1, body2), m_distance{ distance } {}
			~DistanceConstraint() {}

			/// <summary>
			/// Use to update the Baumgarte stabilization bias
			/// </summary>
			/// <param name="new_bias"></param>
			void setTranslationBias(real new_bias) {
				m_bias_factor = new_bias;
			}

			void setUp(real dt) override {
				// Compute distance between the objects' center, their distance and the difference to the constraint distance
				m_rel_pos = m_body1->m_positionW - m_body2->m_positionW;
				m_body_distance = glm::length(m_rel_pos);
				m_offset = m_distance - m_body_distance;

				// Compute parts of the jacobian matrix; in this case the relative positions
				m_j1 = glm::normalize(m_rel_pos);
				m_j2 = -m_j1;

				// Compute total constraint mass and invert it if possible
				real total_mass = m_body1->m_mass_inv + m_body2->m_mass_inv;
				m_inv_constraint_mass = total_mass < Constraint::epsilon ? 0.0_real : 1.0_real / total_mass;

				// Compute bias for Baumgarte stabilization
				m_bias = (m_bias_factor * m_offset) / dt;
			};
			
			/// <summary>
			/// Compute and apply constraint impulses
			/// </summary>
			void solveVelocity() override {
				if (abs(m_offset) > Constraint::epsilon) {
					// Compute dot product of Jacobian and velocity vector; keep in mind that j2 = -j1, so the original expression can be simplified
					real jv = glm::dot(m_body1->m_linear_velocityW - m_body2->m_linear_velocityW, m_j1);
					real lambda = m_inv_constraint_mass * -(jv - m_bias);

					glmvec3 impulse1 = m_j1 * lambda * m_body1->m_mass_inv;
					glmvec3 impulse2 = m_j2 * lambda * m_body2->m_mass_inv;

					m_body1->m_linear_velocityW += impulse1;
					m_body2->m_linear_velocityW += impulse2;
				}				
			}
		};

		/// <summary>
		/// A constraint that models a ball-socket joint. The two bodies are connected at the anchor point given in world space
		/// They can rotate around the anchor point freely, but no relative translation is allowed
		/// </summary>
		class BallSocketJoint : public Constraint {
			real m_bias_factor = 0.15_real;					// Bias factor for Baumgarte stabilization

			// These values are set when the Joint is created
			glmvec3 m_anchor_w;								// Anchor point in world space
			glmvec3 m_anchor_body1;							// Anchor point in local space of body1
			glmvec3 m_anchor_body2;							// Anchor point in local space of body2

			// These values are computed once before each loop as they don't change between iterations		
			glmvec3 m_r1{ 0.0_real };						// vector from body1's center to body1's anchor in world space
			glmvec3 m_r2{ 0.0_real };						// vector from body2's center to body2's anchor in world space
			glmvec3 m_offset{ 0.0_real };					// Constraint error
			glmmat3 m_j1{ 0.0_real };						// First part of Jacobian
			glmmat3 m_j2{ 0.0_real };;						// Second part of Jacobian
			glmmat3 m_j3{ 0.0_real };						// Third part of Jacobian
			glmmat3 m_j4{ 0.0_real };						// Fourth part of Jacobian
			glmmat3 m_inv_constraint_mass{ 0.0_real };		// Inverse effective constraint mass
			glmvec3 m_bias{ 0.0_real };						// Bias to be used for Baumgarte stabilization
		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="body1">First body</param>
			/// <param name="body2">Second body</param>
			/// <param name="anchor">Anchor point in world space</param>
			BallSocketJoint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2, glmvec3 anchor) : Constraint(body1, body2), m_anchor_w{ anchor } {
				// Move the anchor point to local space for each body
				m_anchor_body1 = m_body1->m_model_inv * glmvec4(m_anchor_w, 1.0_real);
				m_anchor_body2 = m_body2->m_model_inv * glmvec4(m_anchor_w, 1.0_real);
			}
			~BallSocketJoint() {}

			/// <summary>
			/// Use to update the Baumgarte stabilization bias for the translation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setTranslationBias(real new_bias) {
				m_bias_factor = new_bias;
			}

			/// <summary>
			/// Computes values that remain static within one loop/timestep
			/// </summary>
			/// <param name="dt">Simulation timestep</param>
			void setUp(real dt) override {
				// Move anchor points back to world space
				glmvec3 anchor1 = m_body1->m_model * glmvec4(m_anchor_body1, 1.0_real);
				glmvec3 anchor2 = m_body2->m_model * glmvec4(m_anchor_body2, 1.0_real);

				// compute vector from body center to bodies' anchor in world space
				m_r1 = anchor1 - m_body1->m_positionW;
				m_r2 = anchor2 - m_body2->m_positionW;
				m_offset = anchor2 - anchor1;

				// Compute components of Jacobian matrix
				m_j1 = glm::mat3(-1.0_real);
				m_j2 = glm::matrixCross3(m_r1);
				m_j3 = glm::mat3(1.0_real);
				m_j4 = -glm::matrixCross3(m_r2);

				// Compute total constraint mass
				glmmat3 constraint_mass = m_body1->m_mass_inv * glmmat3(1.0_real) + m_j2 * m_body1->m_inertia_invW * glm::transpose(m_j2) + m_body2->m_mass_inv * glmmat3(1.0_real) + (-m_j4) * m_body2->m_inertia_invW * glm::transpose(-m_j4);
				// only invert if matrix is actually invertible - can happen when two bodies with infinite mass are involved
				m_inv_constraint_mass = glm::determinant(constraint_mass) < Constraint::epsilon ? glmmat3(0.0_real) : glm::inverse(constraint_mass);
				m_bias = (m_bias_factor / dt) * m_offset;
			};

			/// <summary>
			/// Computes and applies constraint impulses
			/// </summary>
			void solveVelocity() override {
				glmvec3 abs_offset = glm::abs(m_offset);
				if (abs_offset.x > Constraint::epsilon || abs_offset.y > Constraint::epsilon || abs_offset.z > Constraint::epsilon) {
					// Compute product of jacobian (3x12 matrix) and velocity vector (12x1 matrix) with submatrices
					glmvec3 j1v1 = -m_body1->m_linear_velocityW; // j1 is negated identity matrix
					glmvec3 j2v2 = m_j2 * m_body1->m_angular_velocityW;
					glmvec3 j3v3 = m_body2->m_linear_velocityW; // j3 is identity matrix
					glmvec3 j4v4 = m_j4 * m_body2->m_angular_velocityW;
					glmvec3 jv = j1v1 + j2v2 + j3v3 + j4v4;

					glmvec3 lambda = m_inv_constraint_mass * (-jv - m_bias);
			
					// Compute impulses via constraint_force = J^t * lambda
					// j2 and j4 are skew symmetric, so their transpose is their negation
					glmvec3 impulse1 = -lambda; // j1 is negated identity matrix
					glmvec3 impulse2 = -m_j2 * lambda;
					glmvec3 impulse3 = lambda; // j3 is identity matrix
					glmvec3 impulse4 = -m_j4 * lambda;

					m_body1->m_linear_velocityW += m_body1->m_mass_inv * impulse1;
					m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse2;
					m_body2->m_linear_velocityW += m_body2->m_mass_inv * impulse3;
					m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse4;
				}
			}
		};

		/// <summary>
		/// A hinge constraint that connects two bodies via an anchor point and only allows them to rotate around a given hinge axis in world space
		/// Supports angle limits and a motor
		/// </summary>
		class HingeJoint : public Constraint {
			std::shared_ptr<BallSocketJoint> m_ballsocket;		// Used for the translation constraint
			real m_bias_factor_rot = 0.15_real;					// Bias factor for translation constraint Baumgarte stabilization
			real m_bias_factor_trans = 0.15_real;				// Bias factor for rotation constraint Baumgarte stabilization
			real m_bias_factor_limit = 0.1_real;				// Bias factor for limit constraint Baumgarte stabilization

			// These values are set once when the Joint is created
			glmvec3 m_rot_axis_w;								// Hinge axis in world space
			glmvec3 m_rot_axis_body1;							// Hinge axis in body1's local space
			glmvec3 m_rot_axis_body2;							// Hinge axis in body2's local space

			bool m_limit_active = false;						// Flag that enables angle limits
			real m_limit_max = -pi2;							// Maximum angle i.e. max rotation in positive direction
			real m_limit_min = pi2;								// Minimum angle i.e. max rotation in negative direction
			glmquat m_init_orientation_inv;						// Inverse of initial orientation between the two bodies

			bool m_motor_active = false;						// Flag that enables the motor (if active, the constraint tries to maintain a certain angular velocity along the hinge axis)
			real m_fmotor = 0.0_real;							// The speed of the motor in radians/s, i.e. the angular speed that should be maintained
			real m_fmotor_max = 0.0_real;						// The maximum force the motor is allowed to apply per iteration, can be used to controll ramp up time to motor speed
			bool m_body1_motor_factor = true;					// Whether body1 should be affected by motor forces
			bool m_body2_motor_factor = true;					// Whether body2 should be affected by motor forces

			// These values are computed in setUp() before each loop as they don't change between iterations
			glmvec3 m_axis1_world{ 0.0_real };					// Hinge axis of body1 moved to world space
			glmvec3 m_axis2_world{ 0.0_real };					// Hinge axis of body2 moved to world space

			// Angle limit 
			real m_bias_limit_min = 0.0_real;					// Baumgarte bias to be used for lower angle limit
			real m_bias_limit_max = 0.0_real;					// Baumgarte bias to be used for upper angle limit
			real m_theta = 0.0_real;							// Angle between the bodies along the hinge axis
			real m_inv_constraint_mass_limit = 0.0_real;		// Inverted effective constraint mass for angle limit

			// Motor
			real m_offset_motor = 0.0_real;						// Constraint error for motor
			real m_inv_constraint_mass_motor = 0.0_real;		// Inverted effective constraint mass for motor

			// Rotation
			glmvec2 m_offset_rotation{ 0.0_real };				// Constraint error for rotation constraint
			glmvec3 m_bCa{ 0.0_real };							// Cross product needed for Jacobian and mass
			glmvec3 m_cCa{ 0.0_real };							// Cross product needed for Jacobian and mass
			glmmat2 m_inv_constraint_mass_rot{ 0.0_real };		// Inverted effective constraint mass for rotation constraint
			glmvec2 m_bias_rotation{ 0.0_real };				// Baumgarte bias to be used for rotation constraint

			/// <summary>
			/// Returns the inverse mass matrix for the motor and limit constraints
			/// Requires m_axis1_world to have been computed already
			/// </summary>
			/// <returns></returns>
			real getMotorAndLimitMass() const {
				real mass = glm::dot(m_axis1_world * m_body1->m_inertia_invW, m_axis1_world) + glm::dot(m_axis1_world * m_body2->m_inertia_invW, m_axis1_world);
				return mass < Constraint::epsilon ? 0.0_real : 1.0_real / mass;
			}

		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="body1">First body</param>
			/// <param name="body2">Second body</param>
			/// <param name="anchor">Anchor point in world space</param>
			/// <param name="axis">Hinge axis in world space</param>
			HingeJoint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2, glmvec3 anchor, glmvec3 axis) : Constraint(body1, body2) {
				// Initialize ballsocket joint for translation constraint
				m_ballsocket = std::make_shared<VPEWorld::BallSocketJoint>(m_body1, m_body2, anchor);
				m_ballsocket->setTranslationBias(m_bias_factor_trans);
				// Move rotation axis to local space of each body
				m_rot_axis_w = glm::normalize(axis);
				m_rot_axis_body1 = glm::normalize(m_body1->m_model_inv * glmvec4(m_rot_axis_w, 0.0_real));
				m_rot_axis_body2 = glm::normalize(m_body2->m_model_inv * glmvec4(m_rot_axis_w, 0.0_real));
			};

			~HingeJoint() {}
			
			/// <summary>
			/// Use this to specify whether body1 should be affected by the motor force or not
			/// </summary>
			/// <param name="moved_by_motor">Should this body be moved by the motor?</param>
			void setBody1MotorEnabled(bool moved_by_motor) {
				m_body1_motor_factor = moved_by_motor;
			}

			/// <summary>
			/// Use this to specify whether body2 should be affected by the motor force or not
			/// </summary>
			/// <param name="moved_by_motor">Should this body be moved by the motor?</param>
			void setBody2MotorEnabled(bool moved_by_motor) {
				m_body2_motor_factor = moved_by_motor;
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the translation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setTranslationBias(real new_bias) {
				m_bias_factor_trans = new_bias;
				m_ballsocket->setTranslationBias(m_bias_factor_trans);
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the rotation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setRotationBias(real new_bias) {
				m_bias_factor_rot = new_bias;
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the angle limit constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setLimitBias(real new_bias) {
				m_bias_factor_limit = new_bias;
			}

			/// <summary>
			/// Enables angle limits for the hinge joint. 
			/// The current relative orientation of the bodies is used for the initial orientation,
			/// which then corresponds to an angle of 0.
			/// </summary>
			/// <param name="min_angle">Angle that the hinge should be able to rotate around in the negative direction in radians in range [-2*pi, 0]</param>
			/// <param name="max_angle">Angle that the hinge should be able to rotate around in the positive direction in radians in range [0,  2*pi]</param>
			void enableLimit(real min_angle, real max_angle) {
				assert(min_angle < max_angle);
				assert(min_angle <= 0.0_real);
				assert(max_angle >= 0.0_real);

				m_limit_min = min_angle;
				m_limit_max = max_angle;
				// Compute current orientation between the bodies
				m_init_orientation_inv = glm::normalize(m_body2->m_orientationLW * glm::inverse(m_body1->m_orientationLW));
				glm::inverse(m_init_orientation_inv);
				m_limit_active = true;
			}

			/// <summary>
			/// Disables the angle limits for the hinge joint
			/// </summary>
			void disableLimit() {
				m_limit_active = false;
			}

			/// <summary>
			/// Enables the motor for the hinge joint.
			/// If the motor is enabled, the constraint tries to maintain the given angular velocity along the hinge axis between the bodies
			/// </summary>
			/// <param name="motor_speed">The angular speed in radians/sec that the motor should maintain</param>
			/// <param name="max_force">The maximum force in newton meters that can be applied per iteration. Use this to controll ramp up time</param>
			void enableMotor(real motor_speed, real max_force) {
				assert(max_force > 0.0_real);
				m_fmotor = motor_speed;
				m_fmotor_max = max_force;
				m_motor_active = true;
			}

			/// <summary>
			/// Disables the motor for the hinge joint
			/// </summary>
			void disableMotor() {
				m_motor_active = false;
				m_fmotor = 0.0_real;
				m_fmotor_max = 0.0_real;
			}

			/// <summary>
			/// Computes values that remain static within one loop/timestep
			/// Note: We often use m_axis1_world even though the math would require m_rot_axis_w;
			/// We do this to allow the entire system to tilt, i.e. for the hinge axis to also be able to rotate. Using m_rot_axis_w would lead to the system always trying to keep the same orientation which can become quite unstable
			/// </summary>
			/// <param name="dt">Simulation timestep</param>
			void setUp(real dt) override {
				m_ballsocket->setUp(dt);

				// Move hinge axis back to world space for each body
				m_axis1_world = glm::normalize(m_body1->m_model * glmvec4(m_rot_axis_body1, 0.0_real));
				m_axis2_world = glm::normalize(m_body2->m_model * glmvec4(m_rot_axis_body2, 0.0_real));

				if (m_limit_active) {
					glmquat current_orientation = m_body2->m_orientationLW * glm::inverse(m_body1->m_orientationLW);
					// Difference between original orientation and current one
					glmquat diff_orientation = glm::normalize(current_orientation * m_init_orientation_inv);

					// Recover rotation angle theta from quaternion
					// See https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation (section Recovering the axis-angle representation)
					glmvec3 rel_rot_axis(diff_orientation.x, diff_orientation.y, diff_orientation.z);
					// Two quaternions q and -q encode the same rotation; take whichever one points in the same direction as the hinge axis
					real w = diff_orientation.w;
					if (glm::dot(rel_rot_axis, m_axis1_world) < 0.0_real) w = -w; // axes point in different directions

					// This calculates the angle in [0; 2pi] and converts it to [-pi, pi]
					real theta = std::remainder(2.0_real * std::atan2(glm::length(rel_rot_axis), w), pi2);

					// Our angle is now in [-pi; pi], so negative values go in one direction and positive ones in the other
					// However, a positive rotation can also be the continuation of a negative one (since it flips over when you go lower than -pi) and vice versa
					// Even if theta is larger/lower than one of the angle limits,
					// we only want to compare to the angle that is closer to theta, so convert it accordingly
					if (theta > m_limit_max) {
						if (std::abs(std::remainder(theta - m_limit_max, pi2)) > std::abs(std::remainder(theta - m_limit_min, pi2))) {
							theta -= pi2;
						}
					}
					else if (theta < m_limit_min) {
						if (std::abs(std::remainder(m_limit_max - theta, pi2)) < std::abs(std::remainder(m_limit_min - theta, pi2))) {
							theta += pi2;
						}
					}

					m_bias_limit_min = (m_bias_factor_limit / dt) * (theta - m_limit_min);
					m_bias_limit_max = (m_bias_factor_limit / dt) * (m_limit_max - theta);
					m_theta = theta;
					m_inv_constraint_mass_limit = getMotorAndLimitMass();
				}

				if (m_motor_active) {
					// Get current angular speed along the hinge axis (using m_axis1_world here, see note above class)
					real speed_projection = glm::dot(m_body2->m_angular_velocityW - m_body1->m_angular_velocityW, m_axis1_world);
					m_offset_motor = speed_projection + m_fmotor;
					m_inv_constraint_mass_motor = getMotorAndLimitMass();
				}

				// Compute 2 orthogonal unit vectors to axis2 to formulate the rotation constraint function
				glmvec3 b1 = geometry::orthoUnitVector(m_axis2_world);
				glmvec3 c1 = glm::normalize(glm::cross(m_axis2_world, b1));

				// Constraint error and bias
				m_offset_rotation = glmvec2(glm::dot(m_axis1_world, b1), glm::dot(m_axis1_world, c1));
				m_bias_rotation = (m_bias_factor_rot / dt) * m_offset_rotation;

				glmvec3 bCa = glm::cross(b1, m_axis1_world);
				glmvec3 cCa = glm::cross(c1, m_axis1_world);
				m_bCa = bCa;
				m_cCa = cCa;

				// Compute constraint mass matrix with J*M^-1*J^T
				real a = glm::dot(bCa * m_body1->m_inertia_invW, bCa) + glm::dot(bCa * m_body2->m_inertia_invW, bCa);
				real b = glm::dot(bCa * m_body1->m_inertia_invW, cCa) + glm::dot(bCa * m_body2->m_inertia_invW, cCa);
				real c = glm::dot(cCa * m_body1->m_inertia_invW, bCa) + glm::dot(cCa * m_body2->m_inertia_invW, bCa);
				real d = glm::dot(cCa * m_body1->m_inertia_invW, cCa) + glm::dot(cCa * m_body2->m_inertia_invW, cCa);
				// Column major!
				glmmat2 constraint_mass(a, c, b, d);
				// Only invert if matrix is actually invertible
				m_inv_constraint_mass_rot = glm::determinant(constraint_mass) < Constraint::epsilon ? glmmat2(0.0_real) : glm::inverse(constraint_mass);
			};

			/// <summary>
			/// Computes and applies constraint forces if necessary
			/// </summary>
			/// <param name="dt">Delta time since last frame</param>
			void solveVelocity() override {
				// Handle limit constraints
				if (m_limit_active) {
					if (m_theta < m_limit_min) {
						// Missing compontents of Jacobian matrix are 0
						glmvec3 j2 = -m_axis1_world;
						glmvec3 j4 = m_axis1_world;

						// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
						real jv = glm::dot(m_axis1_world, -m_body1->m_angular_velocityW + m_body2->m_angular_velocityW);
						real lambda = m_inv_constraint_mass_limit * (-jv - m_bias_limit_min);

						glmvec3 impulse1 = j2 * lambda;
						glmvec3 impulse2 = j4 * lambda;

						m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse1;
						m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse2;
					}

					if (m_theta > m_limit_max) { 
						// Missing compontents of Jacobian matrix are 0
						glmvec3 j2 = m_axis1_world;
						glmvec3 j4 = -m_axis1_world;

						// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
						real jv = glm::dot(m_axis1_world, m_body1->m_angular_velocityW - m_body2->m_angular_velocityW);
						real lambda = m_inv_constraint_mass_limit * (-jv - m_bias_limit_max);

						glmvec3 impulse1 = j2 * lambda;
						glmvec3 impulse2 = j4 * lambda;

						m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse1;
						m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse2;
					} 
				}
				
				// Handle motor constraint
				if (m_motor_active) {
					// Missing compontents of Jacobian matrix are 0
					glmvec3 j2 = m_axis1_world;
					glmvec3 j4 = -m_axis1_world;

					// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
					real jv = glm::dot(m_axis1_world, -m_body2->m_angular_velocityW + m_body1->m_angular_velocityW);

					real bias = m_fmotor; // Our bias is the motor speed. We introduce extra energy into the system here
					real lambda = m_inv_constraint_mass_motor * (-jv - bias);

					// Clamp lambda so it doesn't exceed the maximum allowed force
					lambda = std::clamp(lambda, -m_fmotor_max, m_fmotor_max);

					glmvec3 impulse1 = j2 * lambda;
					glmvec3 impulse2 = j4 * lambda;

					m_body1->m_angular_velocityW += m_body1_motor_factor * m_body1_factor * m_body1->m_inertia_invW * impulse1;
					m_body2->m_angular_velocityW += m_body2_motor_factor * m_body2_factor * m_body2->m_inertia_invW * impulse2;
				}

				m_ballsocket->solveVelocity();
			
				glmvec2 abs_offset = glm::abs(m_offset_rotation);
		
				if (abs_offset.x > Constraint::epsilon || abs_offset.y > Constraint::epsilon) {
					// Compute Jacobian matrix
					// Missing compontents are 0
					glmvec3 j12 = -m_bCa;
					glmvec3 j22 = -m_cCa;
					glmvec3 j14 = m_bCa;
					glmvec3 j24 = m_cCa;

					// Calculate product of 2x12 Jacobian matrix and 12x1 velocity vector
					glmvec2 jv(
						glm::dot(j12, m_body1->m_angular_velocityW) + glm::dot(j14, m_body2->m_angular_velocityW),
						glm::dot(j22, m_body1->m_angular_velocityW) + glm::dot(j24, m_body2->m_angular_velocityW)
					);

					glmvec2 lambda = m_inv_constraint_mass_rot * (-jv - m_bias_rotation);

					// Compute constraint forces via J^t*lambda. Since J has 0-compontents, we don't have any linear impulses here
					// Since we can only easily access the columns of J^t, we do the multiplication column-wise instead of using rows
					// see https://math.stackexchange.com/questions/1422045/matrix-multiplication-of-columns-times-rows-instead-of-rows-times-columns
					glmvec3 impulse1 = j12 * lambda.x + j22 * lambda.y;
					glmvec3 impulse2 = j14 * lambda.x + j24 * lambda.y;

					m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse1;
					m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse2;
				}
			}
		};

		/// <summary>
		/// A fixed joint removes all degrees of freedom by allowing no relative motion between the involved bodies.
		/// The bodies essentially become one system, i.e. their movement depends completely on each other.
		/// The joint is defined by an anchor point. 
		/// If you use a body with infinite mass to fixate it, setting the anchor point to the other, non-fixed body increases stiffness
		/// </summary>
		class FixedJoint : public Constraint {
			std::shared_ptr<BallSocketJoint> m_ballsocket;		// Used for the translation constraint
			real m_bias_factor_trans = 0.2_real;				// Bias factor for translation constraint Baumgarte stabilization
			real m_bias_factor_rot = 0.1_real;					// Bias factor for rotation constraint Baumgarte stabilization

			// This is computed when the Joint is created
			glmquat m_init_orientation_inv;						// Initial orientation of the two bodies

			// This is computed before each loop in setUp()
			glmmat3 m_inv_constraint_mass_rot{ 0.0_real };		// Inverse effective constraint mass
			glmvec3 m_bias_rot{ 0.0_real };						// Baumgarte bias to be used for the rotation constraint
		public:
			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="body1">First body</param>
			/// <param name="body2">Second body</param>
			/// <param name="anchor">Anchor point in world space</param>
			FixedJoint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2, glmvec3 anchor) : Constraint(body1, body2) {
				m_ballsocket = std::make_shared<VPEWorld::BallSocketJoint>(m_body1, m_body2, anchor);
				m_ballsocket->setTranslationBias(m_bias_factor_trans);
				// Compute inverse of initial orientation between the bodies
				m_init_orientation_inv = glm::inverse(m_body2->m_orientationLW) * m_body1->m_orientationLW;
			}
			~FixedJoint() {}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the translation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setTranslationBias(real new_bias) {
				m_bias_factor_trans = new_bias;
				m_ballsocket->setTranslationBias(m_bias_factor_trans);
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the rotation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setRotationBias(real new_bias) {
				m_bias_factor_rot = new_bias;
			}

			/// <summary>
			/// Computes values that remain static within one timestep
			/// </summary>
			/// <param name="dt">Simulation timestep</param>
			void setUp(real dt) {
				m_ballsocket->setUp(dt);

				// Compute constraint mass and invert it if possible
				glmmat3 constraint_mass = m_body1->m_inertia_invW + m_body2->m_inertia_invW;
				m_inv_constraint_mass_rot = glm::determinant(constraint_mass) < Constraint::epsilon ? glmmat3(0.0_real) : glm::inverse(constraint_mass);

				// Compute constraint error (relative rotation) and Baumgarte bias
				glmquat offset = m_body2->m_orientationLW * m_init_orientation_inv * glm::inverse(m_body1->m_orientationLW);
				m_bias_rot = (m_bias_factor_rot / dt) * 2.0_real * glmvec3(offset.x, offset.y, offset.z);
			}

			void solveVelocity() {
				m_ballsocket->solveVelocity();

				// Compute product of 3x12 Jacobian and 12x1 velocity vector
				// Since the Jacobian is (0 -I 0 I) (I being the 3x3 identity matrix), this is pretty simple here
				glmvec3 jv = m_body2->m_angular_velocityW - m_body1->m_angular_velocityW;
				glmvec3 lambda = m_inv_constraint_mass_rot * (-jv - m_bias_rot);

				glmvec3 impulse1 = -lambda;
				glmvec3 impulse2 = lambda;

				m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse1;
				m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse2;
			}
		};

		/// <summary>
		/// A slider joint only allows one degree of freedom, namely translation along the slide / joint axis.
		/// The joint is given by an anchor point and translation axis in world space.
		/// The bodies can then only do relative translations along the translation axis
		/// If you use a body with infinite mass to fixate it, setting the anchor point to the other, non-fixed body increases stiffness
		/// </summary>
		class SliderJoint : public Constraint {
			real m_bias_factor_trans = 0.2_real;				// Bias factor for translation constraint Baumgarte stabilization
			real m_bias_factor_rot = 0.2_real;					// Bias factor for rotation constraint Baumgarte stabilization
			real m_bias_factor_limit = 0.1_real;				// Bias factor for limit constraint Baumgarte stabilization

			bool m_limit_active = false;						// Whether the constraint should limit the min and max relative distance between the bodies
			real m_limit_min = -std::numeric_limits<real>::max(); // The lower limit for relative distance, i.e. distance limit in negative direction
			real m_limit_max = std::numeric_limits<real>::max();  // The upper limit for relative distance, i.e. distance limit in positive direction

			bool m_motor_active = false;						// Whether the motor should be active
			real m_fmotor = 0.0_real;							// Linear motor speed in m/s
			real m_fmotor_max = 0.0_real;						// Max motor force applied per timestep in Newton
			bool m_body1_motor_factor = true;					// Whether body1 should be affected by motor forces
			bool m_body2_motor_factor = true;					// Whether body2 should be affected by motor forces

			// This is computed when the Joint is created
			glmvec3 m_anchor_world{ 0.0_real };
			glmvec3 m_anchor_body1;								// Anchor point in local space of body1
			glmvec3 m_anchor_body2;								// Anchor point in local space of body2
			glmvec3 m_axis_world{ 0.0_real };					// Translation axis in world space
			glmvec3 m_axis_body1{ 0.0_real };					// Axis in local space of body 1
			glmquat m_init_orientation_inv;						// Initial orientation of the two bodies

			// The following values are computed before each loop in setUp()
			// Translation constraint
			glmmat2 m_inv_constraint_mass_trans{ 0.0_real };	// Inverse effective constraint mass for translation constraint
			glmvec2 m_bias_trans{ 0.0_real };					// Baumgarte bias to be used for the translation constraint
			glmvec3 m_r1{ 0.0_real };							// vector from body1's center to body1's anchor in world space
			glmvec3 m_r2{ 0.0_real };							// vector from body2's center to body2's anchor in world space
			glmvec3 m_axis_body1_w{ 0.0_real };					// Translation axis of body1 moved to world space
			glmvec3 m_anchor_diff{ 0.0_real };					// Relative position difference of the two bodies' anchor points

			// Entries for the translation constraint Jacobian
			glmvec3 m_j11{ 0.0_real };
			glmvec3 m_j12{ 0.0_real };
			glmvec3 m_j13{ 0.0_real };
			glmvec3 m_j14{ 0.0_real };
			glmvec3 m_j21{ 0.0_real };
			glmvec3 m_j22{ 0.0_real };
			glmvec3 m_j23{ 0.0_real };
			glmvec3 m_j24{ 0.0_real };

			// Rotation Constraint
			glmmat3 m_inv_constraint_mass_rot{ 0.0_real };		// Inverse effective constraint mass for the rotaion constraint
			glmvec3 m_bias_rot{ 0.0_real };						// Baumgarte bias to be used for the rotation constraint

			// Limit Constraint
			real m_inv_constraint_mass_limit = 0.0_real;		// Inverse effective constraint mass for the limit constraint
			real m_bias_limit_min = 0.0_real;					// The Baumgarte bias value for the minimum limit constraint
			real m_bias_limit_max = 0.0_real;					// The Baumgarte bias value for the maximum limit constraint
			real m_current_distance = 0.0_real;					// The current distance between the bodies
			// Used for jacobian entries for limit constraint
			glmvec3 m_r1_anchor_axis{ 0.0_real };
			glmvec3 m_r2_axis{ 0.0_real };

			// Motor constraint
			real m_inv_constraint_mass_motor = 0.0_real;		// Inverse effective constraint mass for the motor
			real m_offset_motor = 0.0_real;						// Constraint error for the motor

		public:
			/// <summary>
			/// Constructor of the SliderJoint class
			/// </summary>
			/// <param name="body1">First body</param>
			/// <param name="body2">Second body</param>
			/// <param name="anchor">Anchor point in world space</param>
			/// <param name="anchor">Translation axis in world space</param>
			SliderJoint(std::shared_ptr<Body> body1, std::shared_ptr<Body> body2, glmvec3 anchor, glmvec3 axis) : Constraint(body1, body2), m_anchor_world{ anchor }, m_axis_world{ glm::normalize(axis) } {
				// Move anchor point into local space for each body
				m_anchor_body1 = m_body1->m_model_inv * glmvec4(m_anchor_world, 1.0_real);
				m_anchor_body2 = m_body2->m_model_inv * glmvec4(m_anchor_world, 1.0_real);
				m_axis_body1 = glm::normalize(m_body1->m_model_inv * glmvec4(m_axis_world, 0.0_real));
				// Compute current (inverse) orientation between the bodies
				m_init_orientation_inv = glm::inverse(m_body2->m_orientationLW) * m_body1->m_orientationLW;
			}
			~SliderJoint() {}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the translation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setTranslationBias(real new_bias) {
				m_bias_factor_trans = new_bias;
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the rotation constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setRotationBias(real new_bias) {
				m_bias_factor_rot = new_bias;
			}

			/// <summary>
			/// Use to change the Baumgarte stabilization bias factor for the translation distance limit constraint
			/// </summary>
			/// <param name="new_bias"></param>
			void setLimitBias(real new_bias) {
				m_bias_factor_limit = new_bias;
			}

			/// <summary>
			/// Enables distance limits for the slider joint
			/// The current relative distance of the bodies is used for the initial orientation,
			/// which then corresponds to a distance of 0.
			/// </summary>
			/// <param name="min_distance">Minimum relative distance between the bodies, i.e. how far can they translate in the negative direction; in range [-inf; 0]</param>
			/// <param name="max_distance">Maximum relative distance between the bodies, i.e. how far can they translate in the positive direction; in range [0; inf]</param>
			void enableLimit(real min_distance, real max_distance) {
				assert(min_distance < max_distance);
				assert(min_distance <= 0.0_real);
				assert(max_distance >= 0.0_real);

				m_limit_min = min_distance;
				m_limit_max = max_distance;
				m_limit_active = true;
			}

			/// <summary>
			/// Disables the distance limits for the hinge joint
			/// </summary>
			void disableLimit() {
				m_limit_active = false;
			}

			/// Use this to specify whether body1 should be affected by the motor force or not
			/// </summary>
			/// <param name="moved_by_motor">Should this body be moved by the motor?</param>
			void setBody1MotorEnabled(bool moved_by_motor) {
				m_body1_motor_factor = moved_by_motor;
			}

			/// <summary>
			/// Use this to specify whether body2 should be affected by the motor force or not
			/// </summary>
			/// <param name="moved_by_motor">Should this body be moved by the motor?</param>
			void setBody2MotorEnabled(bool moved_by_motor) {
				m_body2_motor_factor = moved_by_motor;
			}

			/// <summary>
			/// Enables the motor for the slider joint.
			/// If the motor is enabled, the constraint tries to maintain the given linear velocity along the slider axis between the bodies
			/// </summary>
			/// <param name="motor_speed">The linear motor speed in m/s</param>
			/// <param name="max_force">The maximum force in Newton that can be applied per iteration step. Use this to controll ramp up time</param>
			void enableMotor(real motor_speed, real max_force) {
				assert(max_force > 0.0_real);
				m_fmotor = motor_speed;
				m_fmotor_max = max_force;
				m_motor_active = true;
			}

			/// <summary>
			/// Disables the motor for the hinge joint
			/// </summary>
			void disableMotor() {
				m_motor_active = false;
				m_fmotor = 0.0_real;
				m_fmotor_max = 0.0_real;
			}

			/// <summary>
			/// Computes values that remain static within one timestep
			/// Note: Just like with the HingeJoint, we often use m_axis_body1_w even though the math would require m_axis_world; for the same reasons as with the HingeJoint
			/// </summary>
			/// <param name="dt">Simulation timestep</param>
			void setUp(real dt) {
				// Translation Constraint
				// Move body1's axis back to world space
				m_axis_body1_w = glm::normalize(m_body1->m_model * glmvec4(m_axis_body1, 0.0_real));
				// Compute two orthogonal unit vectors to the axis to use for the Jacobian and mass matrices
				glmvec3 n1 = geometry::orthoUnitVector(m_axis_body1_w);
				glmvec3 n2 = glm::normalize(glm::cross(m_axis_body1_w, n1));

				// Move anchor points back to world space for each body
				glmvec3 anchor1 = m_body1->m_model * glmvec4(m_anchor_body1, 1.0_real);
				glmvec3 anchor2 = m_body2->m_model * glmvec4(m_anchor_body2, 1.0_real);

				// compute vector from body center to bodies' anchor in world space
				m_r1 = anchor1 - m_body1->m_positionW;
				m_r2 = anchor2 - m_body2->m_positionW;
				m_anchor_diff = anchor2 - anchor1;

				// Compute constraint error for translation constraint
				glmvec2 offset_trans{ glm::dot(m_anchor_diff, n1), glm::dot(m_anchor_diff, n2) };
				m_bias_trans = (m_bias_factor_trans / dt) * offset_trans;

				// Compute Jacobian and mass matrix
				glmvec3 r1_anchor_diff = m_r1 + m_anchor_diff;
				glmvec3 r1_anchor_n1 = glm::cross(r1_anchor_diff, n1);
				glmvec3 r1_anchor_n2 = glm::cross(r1_anchor_diff, n2);
				glmvec3 r2_n1 = glm::cross(m_r2, n1);
				glmvec3 r2_n2 = glm::cross(m_r2, n2);
				m_j11 = -n1;
				m_j12 = -r1_anchor_n1;
				m_j13 = n1;
				m_j14 = r2_n1;
				m_j21 = -n2;
				m_j22 = -r1_anchor_n2;
				m_j23 = n2;
				m_j24 = r2_n2;

				// Compute effective mass matrix
				real a = m_body1->m_mass_inv + m_body2->m_mass_inv + glm::dot(r1_anchor_n1, m_body1->m_inertia_invW * r1_anchor_n1) + glm::dot(r2_n1, m_body2->m_inertia_invW * r2_n1);
				real b = glm::dot(r1_anchor_n1, m_body1->m_inertia_invW * r1_anchor_n2) + glm::dot(r2_n1, m_body2->m_inertia_invW * r2_n2);
				real c = glm::dot(r1_anchor_n2, m_body1->m_inertia_invW * r1_anchor_n1) + glm::dot(r2_n2, m_body2->m_inertia_invW * r2_n1);
				real d = m_body1->m_mass_inv + m_body2->m_mass_inv + glm::dot(r1_anchor_n2, m_body1->m_inertia_invW * r1_anchor_n2) + glm::dot(r2_n2, m_body2->m_inertia_invW * r2_n2);

				// Column major!
				glmmat2 constraint_mass_trans(a, c, b, d);
				// Only invert if matrix is actually invertible
				m_inv_constraint_mass_trans = glm::determinant(constraint_mass_trans) < Constraint::epsilon ? glmmat2(0.0_real) : glm::inverse(constraint_mass_trans);

				// Rotation Constraint
				// Compute constraint mass and invert it if possible
				glmmat3 constraint_mass_rot = m_body1->m_inertia_invW + m_body2->m_inertia_invW;
				m_inv_constraint_mass_rot = glm::determinant(constraint_mass_rot) < Constraint::epsilon ? glmmat3(0.0_real) : glm::inverse(constraint_mass_rot);

				// Compute constraint error (relative rotation) and Baumgarte bias
				glmquat offset = m_body2->m_orientationLW * m_init_orientation_inv * glm::inverse(m_body1->m_orientationLW);
				m_bias_rot = (m_bias_factor_rot / dt) * 2.0_real * glmvec3(offset.x, offset.y, offset.z);

				// Limit constraint
				if (m_limit_active) {
					m_r1_anchor_axis = glm::cross(r1_anchor_diff, m_axis_body1_w);
					m_r2_axis = glm::cross(m_r2, m_axis_body1_w);

					real limit_mass = m_body1->m_mass_inv + m_body2->m_mass_inv + glm::dot(m_r1_anchor_axis, m_body1->m_inertia_invW * m_r1_anchor_axis) + glm::dot(m_r2_axis, m_body2->m_inertia_invW * m_r2_axis);
					m_inv_constraint_mass_limit = limit_mass < Constraint::epsilon ? 0.0_real : 1.0_real / limit_mass;

					m_current_distance = glm::dot(m_anchor_diff, m_axis_body1_w);
					m_bias_limit_min = (m_bias_factor_limit / dt) * (m_current_distance - m_limit_min);
					m_bias_limit_max = (m_bias_factor_limit / dt) * (m_limit_max - m_current_distance);
				}

				if (m_motor_active) {
					m_inv_constraint_mass_motor = m_body1->m_mass_inv + m_body2->m_mass_inv;
					m_offset_motor = glm::dot(m_axis_body1_w, m_body1->m_linear_velocityW - m_body2->m_linear_velocityW) + m_fmotor;
				}
			}

			void solveVelocity() {
				// Solve limit constraint
				if (m_limit_active) {
					if (m_current_distance < m_limit_min) {
						glmvec3 j1 = -m_axis_body1_w;
						glmvec3 j2 = -m_r1_anchor_axis;
						glmvec3 j3 = m_axis_body1_w;
						glmvec3 j4 = m_r2_axis;

						// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
						real jv = glm::dot(j1, m_body1->m_linear_velocityW) + glm::dot(j2, m_body1->m_angular_velocityW) + glm::dot(j3, m_body2->m_linear_velocityW) + glm::dot(j4, m_body2->m_angular_velocityW);
						real lambda = m_inv_constraint_mass_limit * (-jv - m_bias_limit_min);

						glmvec3 impulse1 = j1 * lambda;
						glmvec3 impulse2 = j2 * lambda;
						glmvec3 impulse3 = j3 * lambda;
						glmvec3 impulse4 = j4 * lambda;

						m_body1->m_linear_velocityW += m_body1->m_mass_inv * impulse1;
						m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse2;
						m_body2->m_linear_velocityW += m_body2->m_mass_inv * impulse3;
						m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse4;
					}

					if (m_current_distance > m_limit_max) {
						glmvec3 j1 = m_axis_body1_w;
						glmvec3 j2 = m_r1_anchor_axis;
						glmvec3 j3 = -m_axis_body1_w;
						glmvec3 j4 = -m_r2_axis;

						// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
						real jv = glm::dot(j1, m_body1->m_linear_velocityW) + glm::dot(j2, m_body1->m_angular_velocityW) + glm::dot(j3, m_body2->m_linear_velocityW) + glm::dot(j4, m_body2->m_angular_velocityW);
						real lambda = m_inv_constraint_mass_limit * (-jv - m_bias_limit_max);

						glmvec3 impulse1 = j1 * lambda;
						glmvec3 impulse2 = j2 * lambda;
						glmvec3 impulse3 = j3 * lambda;
						glmvec3 impulse4 = j4 * lambda;

						m_body1->m_linear_velocityW += m_body1->m_mass_inv * impulse1;
						m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse2;
						m_body2->m_linear_velocityW += m_body2->m_mass_inv * impulse3;
						m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse4;
					}
				}

				// Handle motor constraint
				if (m_motor_active) {
					if (std::abs(m_offset_motor) > 0.0_real) {
						// Missing compontents of Jacobian matrix are 0
						glmvec3 j1 = m_axis_body1_w;
						glmvec3 j3 = -m_axis_body1_w;

						// compute dot product of 1x12 Jacobian matrix and 12x1 velocity vector
						real jv = glm::dot(m_axis_body1_w, m_body1->m_linear_velocityW - m_body2->m_linear_velocityW);

						real bias = m_fmotor; // Our bias is the motor speed. We introduce extra energy into the system here
						real lambda = m_inv_constraint_mass_motor * (-jv - bias);

						// Clamp lambda so it doesn't exceed the maximum allowed force
						lambda = std::clamp(lambda, -m_fmotor_max, m_fmotor_max);

						glmvec3 impulse1 = j1 * lambda;
						glmvec3 impulse2 = j3 * lambda;

						m_body1->m_linear_velocityW += m_body1_motor_factor * m_body1->m_mass_inv * impulse1;
						m_body2->m_linear_velocityW += m_body2_motor_factor * m_body2->m_mass_inv * impulse2;
					}
				}

				// Solve translation constraint
				glmvec3 abs_offset = glm::abs(m_anchor_diff);
				if (abs_offset.x > Constraint::epsilon || abs_offset.y > Constraint::epsilon || abs_offset.z > Constraint::epsilon) {
					// Calculate product of 2x12 Jacobian matrix and 12x1 velocity vector
					glmvec2 jv_trans(
						glm::dot(m_j11, m_body1->m_linear_velocityW) + glm::dot(m_j12, m_body1->m_angular_velocityW) + glm::dot(m_j13, m_body2->m_linear_velocityW) + glm::dot(m_j14, m_body2->m_angular_velocityW),
						glm::dot(m_j21, m_body1->m_linear_velocityW) + glm::dot(m_j22, m_body1->m_angular_velocityW) + glm::dot(m_j23, m_body2->m_linear_velocityW) + glm::dot(m_j24, m_body2->m_angular_velocityW)
					);

					glmvec2 lambda_trans = m_inv_constraint_mass_trans * (-jv_trans - m_bias_trans);

					// Compute constraint forces via J^t*lambda
					// Since we can only easily access the columns of J^t, we do the multiplication column-wise instead of using rows
					// see https://math.stackexchange.com/questions/1422045/matrix-multiplication-of-columns-times-rows-instead-of-rows-times-columns
					glmvec3 impulse1 = m_j11 * lambda_trans.x + m_j21 * lambda_trans.y;
					glmvec3 impulse2 = m_j12 * lambda_trans.x + m_j22 * lambda_trans.y;
					glmvec3 impulse3 = m_j13 * lambda_trans.x + m_j23 * lambda_trans.y;
					glmvec3 impulse4 = m_j14 * lambda_trans.x + m_j24 * lambda_trans.y;

					m_body1->m_linear_velocityW += m_body1->m_mass_inv * impulse1;
					m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse2;
					m_body2->m_linear_velocityW += m_body2->m_mass_inv * impulse3;
					m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse4;
				}

				// Solve rotation constraint
				// Compute product of 3x12 Jacobian and 12x1 velocity vector
				// Since the Jacobian is (0 -I 0 I) (I being the 3x3 identity matrix), this is pretty simple here
				glmvec3 jv_rot = m_body2->m_angular_velocityW - m_body1->m_angular_velocityW;
				glmvec3 lambda_rot = m_inv_constraint_mass_rot * (-jv_rot - m_bias_rot);

				glmvec3 impulse1_rot = -lambda_rot;
				glmvec3 impulse2_rot = lambda_rot;

				m_body1->m_angular_velocityW += m_body1_factor * m_body1->m_inertia_invW * impulse1_rot;
				m_body2->m_angular_velocityW += m_body2_factor * m_body2->m_inertia_invW * impulse2_rot;
			}
		};

	public:
		
		VPEWorld() {};				///Constructor of class VPEWorld
		virtual ~VPEWorld() {};		///Destructor of class VPEWorld

	//--------------------------------Begin-Cloth-Simulation-Stuff----------------------------------
	// by Felix Neumann

	public:
		class Cloth;
		class ClothConstraint;
		using callback_move_cloth = std::function<void(double, std::shared_ptr<Cloth>)>;			//call this function when the cloth moves
		using callback_erase_cloth = std::function<void(std::shared_ptr<Cloth>)>;					//call this function when the cloth is erased

		/// <summary>
		/// All bodies are stored in the map m_cloths. The key is a void*, which can be used to call
		/// back an owner if the cloth moves. With this key, the body can also be found.
		/// So best if there is a 1:1 correspondence. E.g., the owner can be a specific VESceneNode.
		/// </summary>
		std::unordered_map<void*, std::shared_ptr<VPEWorld::Cloth>> m_cloths;

		/// <summary>
		/// Add a new cloth to the physics world.
		/// </summary>
		/// <param name="pbody"> The new body.</param>
		void addCloth(std::shared_ptr<VPEWorld::Cloth> pCloth) {
			m_cloths.insert({ pCloth->m_owner, pCloth });
		}

		/// <summary>
		/// Retrieve a cloth using the owner.
		/// </summary>
		/// <param name="owner"> Pointer to the owner </param>
		/// <returns> Shared pointer to the cloth. </returns>
		auto getCloth(auto* owner) {
			return m_cloths[(void*)owner];
		}

		/// <summary>
		/// Delete all cloths.
		/// </summary>
		void clearCloths() {
			for (std::pair<void*, std::shared_ptr<Cloth>> cloth : m_cloths)
				if (cloth.second->m_on_erase)
					cloth.second->m_on_erase(cloth.second);

			m_cloths.clear();
		}

		/// <summary>
		/// Erase one cloth.
		/// </summary>
		/// <param name="body"> Shared pointer to the cloth. </param>
		void eraseCloth(std::shared_ptr<Cloth> cloth) {
			if (cloth->m_on_erase)
				cloth->m_on_erase(cloth);

			m_bodies.erase(cloth->m_owner);
		}

		/// <summary>
		/// Erase one cloth.
		/// </summary>
		/// <param name="owner"> A void pointer to the owner of the cloth. </param>
		void eraseCloth(auto* owner) {
			std::shared_ptr<Cloth> cloth = m_cloths[(void*)owner];
			if (cloth->m_on_erase)
				cloth->m_on_erase(cloth);

			m_bodies.erase(owner);
		}

		/// <summary>
		/// Used to store the information about which vertices form triangles which is fetched from
		/// the index vector passed to the cloth in its constructor.Triangles are used to create the
		/// constraints. For each point its index within the mass points vector is stored.
		/// </summary>
		struct ClothTriangle
		{
			uint32_t massPoint0Index;
			uint32_t massPoint1Index;
			uint32_t massPoint2Index;
		};

		/// <summary>
		/// A mass point is a vertex of the cloth.
		/// </summary>
		class ClothMassPoint
		{
		public:
			std::vector<uint32_t> m_associatedVertices{};											// Indices of all vertices of VEClothEntity with the same initial position as this mass point.
			glmvec3 pos;																			// Current position of the mass points
			glmvec3 prevPos;																		// Previous position
			const glmvec3 initialPos;																// Initial position before any simulation was applied. Used for setTransform function of cloth.
			glmvec3 vel;																			// Current velocity
			real invMass;																			// 1 - the mass of the mass point. Dynamically calulated depending on the associated triangles.
			bool isFixed;																			// true ... transformations are fully applied, false ... point is dragged along by the simulation

		private:
			const real c_small = 0.01_real;															// Small threshold value
			const real c_verySmall = c_small / 50.0_real;											// Even smaller threshold value
			const real c_collisionMargin = 0.045_real;												// Margin for collision detection with polytopes to avoid glitches
			const real c_friction = 300._real;														// Amount of friction when colliding with polytopes or the ground
			const real c_damping = 0.1_real;														// Amount of damping

		public:
			/// <summary>
			/// Constructor.
			/// </summary>
			/// <param name="pos"> Initial position of the mass point. </param>
			/// <param name="isFixed"> Whether the point is fixed. </param>
			ClothMassPoint(glm::vec3 pos, bool isFixed = false) : pos{ pos }, prevPos{ pos },
				initialPos{ pos }, vel{ glmvec3(0._real) }, isFixed{ isFixed }, invMass{ 0 } {}

			/// <summary>
			/// Apply some external force like gravity or wind.
			/// </summary>
			/// <param name="force"> The force to apply. </param>
			/// <param name="dt"> Delta time. </param>
			void applyExternalForce(glmvec3 force, real dt)
			{
				if (!isFixed)
				{
					vel += force * dt;
					prevPos = pos;
					pos += vel * dt;
				}
			}

			/// <summary>
			/// Pushes the mass point slightly above the ground if it was below.
			/// </summary>
			/// <param name="dt"> Delta time. </param>
			void resolveGroundCollision(real dt)
			{
				if (pos.y < 0)
				{
					pos.y = 0 + c_small;
					vel.y = 0;
					vel -= vel * c_friction * dt;
				}
			}

			/// <summary>
			/// Checks for collisions with the bodies and resolves them if there are some.
			/// </summary>
			/// <param name="bodies"> The bodies to do collision checks with. </param>
			/// <param name="dt"> Delta time. Only affects how much friction is applied in case
			/// of a collision. Can be 0 to apply no friction. </param>
			void resolvePolytopeCollisions(const std::vector<std::shared_ptr<Body>>& bodies,
				real dt = 0._real)
			{
				if (isFixed)																		// Fixed point can go through bodies so that the cloth is always where you expect it to be.
					return;																			// For example attached as a cape to an avatar.

				for (auto body : bodies)
				{
					glmvec3 massPointLocalPos = body->m_model_inv * glmvec4(pos, 1);				// Transform the mass point's position into the body's local space

					if (glm::length(massPointLocalPos) < body->boundingSphereRadius())				// Check if the mass point is within the body's bounding sphere	
						resolvePolytopeCollision(body, massPointLocalPos, dt);						// Resolve possible collision
				}
			}

			/// <summary>
			/// Dampen the cloth's movement to simulate air resistance. Never set it to zero to
			/// avoid losing a degree of freedom.
			/// </summary>
			/// <param name="dt"> Delta time. </param>
			void damp(real dt)
			{
				if (vel.x + vel.y + vel.z > c_verySmall)
					vel -= vel * c_damping * std::min(dt, 1._real);
			}

		private:
			/// <summary>
			/// Check if the mass point is within the polytope and push it to the nearest point
			/// outside if so, 
			/// </summary>
			/// <param name="body"> Body that might collide. </param>
			/// <param name="massPointLocalPos"> The position of the mass point in the body's local
			/// space (body->invModel * massPoint.pos). </param>
			/// <param name="dt"> Delta time. Only affects how much friction is applied in case
			/// of a collision. Can be 0 to apply no friction. </param>
			void resolvePolytopeCollision(const std::shared_ptr<Body> body,
				glmvec3 massPointLocalPos, real dt)
			{
				std::pair<glmvec3, real> nearestProjectionPoint{ {}, INFINITY };					// First: projection of mass point onto face, second: distance

				for (const Face& face : body->m_polytope->m_faces)									// Iterate over all faces of the polytope
				{
					real t = dot(face.m_face_vertex_ptrs[0]->m_positionL +							// Calulate t (distance from face to point along its normal)
						face.m_normalL * c_collisionMargin -										// Extra margin so that cloth is in front of polytope
						massPointLocalPos, face.m_normalL);

					if (t < c_small)																// If the point is in front of a face,
						return;																		// there is no collision

					if (t < nearestProjectionPoint.second)											// Check if the distance is smaller than the previous smallest
						nearestProjectionPoint = { massPointLocalPos + t * face.m_normalL, t };		// Store the projection point and its distance
				}
				// Possible simulation improvement: don't correct straight towards the face 
				prevPos = pos;																		// but a bit towards the general movement of the cloth
				pos = body->m_model * (glmvec4(nearestProjectionPoint.first, 1));					// Set the mass point's position which was inside the polytope to the projection point
				vel -= vel * c_friction * dt;														// Apply friction
			}
		};

		/// <summary>
		/// A constraint is always defined between two mass points and has got a should-length.
		/// If the actual length between the points differs, their positions are corrected towards
		/// where they should be.
		/// </summary>
		class ClothConstraint
		{
		public:
			ClothMassPoint* point0;
			ClothMassPoint* point1;
			real length;																			// The inital legnth between the points
			real compliance;																		// The inverse of stiffness. So if compliance is 0, the cloth is infinitely stiff
			const real c_threshold = 0.0001_real;													// Points are only modified if the length difference is greater tthan this

			/// <summary>
			/// Constructor that calculates the length of the constraint and sets the compliance.
			/// </summary>
			/// <param name="point0"> First mass point. </param>
			/// <param name="point1"> Second mass point. </param>
			/// <param name="compliance"> Compliance. The inverse of stiffness. </param>
			ClothConstraint(ClothMassPoint* point0, ClothMassPoint* point1, real compliance)
				: point0{ point0 }, point1{ point1 }, compliance{ compliance }
			{
				length = glm::distance(point0->pos, point1->pos);
			}

			/// <summary>
			/// Solves the constraint, meaning it corrects the position of the mass points towards
			/// where they are expected to be.
			/// The method used for solving is called XPBD and was developed by Miles Macklin,
			/// Matthias Muller and Nuttapong Chentanez for NVIDIA.
			/// https://matthias-research.github.io/pages/publications/XPBD.pdf
			/// </summary>
			/// <param name="dt"> Delta time. </param>
			void solve(real dt) const
			{
				if (point0->isFixed && point1->isFixed)												// Do not simulate the point if it is fixed
					return;																			// Fixed points should always remain where they were placed

				glmvec3 pos0 = point0->pos;
				glmvec3 pos1 = point1->pos;

				real lengthBetweenPoints = glm::distance(pos0, pos1);								// get the current length between the mass points

				//real lengthBetweenPoints =														// Approximation #1 - Leads to very fidgety simulation behaviour
				//	geometry::alphaMaxPlusBetaMin(
				//		geometry::alphaMaxPlusBetaMin(
				//			pos0.x - pos1.x, pos0.y - pos1.y), pos0.z - pos1.z);

				//real lengthBetweenPoints =
				//	geometry::alphaMaxPlusBetaMedPlusGammaMin(pos0.x - pos1.x, pos0.y - pos1.y,		// Approximation #2 - similar effect to #1
				//		pos0.z - pos1.z);

				real lengthDifference = lengthBetweenPoints - length;								// Get the difference of initial length and current length

				if (fabs(lengthDifference) > c_threshold)											// Threshold
				{
					glmvec3 directionBetweenPoints = (pos1 - pos0) / lengthBetweenPoints;

					real lambda = -lengthDifference /												// Amount of correction towards the should-be position.
						(point0->invMass + point1->invMass + compliance / (dt * dt));				// Depends on length difference, compliance, masses and delta time 

					glmvec3 correctionVec0 = -lambda * point0->invMass * directionBetweenPoints;	// Calculate the corrections vectors
					glmvec3 correctionVec1 = lambda * point1->invMass * directionBetweenPoints;

					if (!point0->isFixed)															// Only apply the correction to one point if the other one is fixed
					{
						point0->pos += correctionVec0;												// Update the position
						point0->vel = (point0->pos - point0->prevPos) / dt;							// Update the velocity
					}

					if (!point1->isFixed)
					{
						point1->pos += correctionVec1;
						point1->vel = (point1->pos - point1->prevPos) / dt;
					}
				}
			}
		};

		/// <summary>
		/// A cloth is a collection of mass points and constraints which it generates upon
		/// construction based on the vertices and indices that were passed to it via the
		/// constructor. It offers the user the possibility to interact with the cloth and keeps
		/// track of nearby rigid bodies for collision detection and resolving.
		/// </summary>
		class Cloth
		{
		public:
			std::string	m_name;																		// Name of the cloth
			void* m_owner;																			// Pointer to owner of this body, must be unique (owner is called if cloth moves)									
			callback_move_cloth m_on_move;															// Called if the cloth moves
			callback_erase_cloth m_on_erase;														// Called if the cloth is erased

		private:
			VPEWorld* m_physics;																	// Pointer to the physics world to access parameters
			std::vector<ClothMassPoint> m_massPoints{};												// All mass points of the cloth
			std::vector<ClothConstraint> m_constraints{};											// All constraints (bending and stretching) of the cloth
			std::vector<vh::vhVertex> m_vertices;													// The vertices of the mesh that was used to create the cloth
			real m_maxMassPointDistance;															// The max distance between two arbitrary mass points at their inital position
			const int c_substeps;																	// How many times per frame the constraints should be solved (higher = less stretchy)
			const real c_movementSimulation;														// How much mass points not fixed should be moved by transformation (0 to 1)
			std::vector<std::shared_ptr<Body>> m_bodiesNearby;										// A vector that stores all bodies nearby for collision detection
			int_t m_gridX;																			// X Coordinate in grid for broadphase
			int_t m_gridZ;																			// Z Coordinate in grid for broadphase
			int_t m_bodiesNearbyCount;																// Number of nearby bodies during previous broadphase pass

		public:
			/// <summary>
			/// Constructs a cloth from a mesh (vector of vertices and vector of indices).
			/// The method used for integration is called XPBD and was developed by Miles Macklin,
			/// Matthias Muller and Nuttapong Chentanez for NVIDIA.
			/// Polytope collision checking and simulated transformations are my own additions.
			/// https://matthias-research.github.io/pages/publications/XPBD.pdf
			/// </summary>
			/// <param name="physics"> Pointer to the physics world. </param>
			/// <param name="name"> Name of the cloth. </param>
			/// <param name="owner"> Pointer to the owner. The callback knows how to use this
			/// pointer. </param>
			/// <param name="on_move"> Callback that is called when the cloth moves. </param>
			/// <param name="on_erase"> Callback that is called when the cloth is erased. </param>
			/// <param name="vertices"> Vertices of the mesh the cloth is created from. </param>
			/// <param name="indices"> Indices of the mesh the cloth is created from. </param>
			/// <param name="bendingCompliance"> Opposite of stiffness. 0 means max stiff. </param>
			/// <param name="fixedPointsPositions"> Points at which the cloth should be attached.
			/// The positions need to match precisely, so best copy the exact numbers from the obj
			/// file. </param>
			/// <param name="substeps"> How many times per frame the constraints should be solved
			/// (higher = less stretchy). </param>
			/// <param name="movementSimulation"> How much mass points not fixed should be moved by
			/// transformation (0 to 1). </param>
			Cloth(VPEWorld* physics, std::string name, void* owner, callback_move_cloth on_move,
				callback_erase_cloth on_erase, std::vector<vh::vhVertex> vertices,
				std::vector<uint32_t> indices, std::vector<glmvec3> fixedPointsPositions,
				real bendingCompliance = 1, int substeps = 4, real movementSimulation = 0.8)
				: m_physics{ physics }, m_name{ name }, m_owner{ owner }, m_on_move{ on_move },
				m_on_erase{ on_erase }, m_vertices{ vertices }, c_substeps{ substeps },
				c_movementSimulation{ movementSimulation }, m_gridX { -100 }, m_gridZ { -100 },
				m_bodiesNearbyCount { 0 }
			{
				createMassPoints(vertices, fixedPointsPositions);
				generateConstraints(createTriangles(indices), bendingCompliance);
				calcMaxMassPointDistance();
				applyTransformation(glm::rotate(													// Apply a slight rotation to give the sim a degree of freedom for all three dimensions
					glm::mat4(1.0f), glm::radians(0.1f), glm::vec3(0.0f, 1.0f, 0.0f)), true);
			}

			/// <summary>
			/// Solves constraints and does collision checking and resolving for all mass points.
			/// The method that is used called XPBD and was developed by Miles Macklin, Matthias
			/// Muller and Nuttapong Chentanez for NVIDIA.
			/// https://matthias-research.github.io/pages/publications/XPBD.pdf
			/// Polytope collision checking is my own addition.
			/// </summary>
			/// <param name="rigidBodyGrid"> The broad phase grid of the rigid bodies. </param>
			/// <param name="dt"> Delta time. </param>
			void integrate(const std::unordered_map<intpair_t, body_map>& rigidBodyGrid, real dt)
			{
				updateBodiesNearby(rigidBodyGrid);													// Check which bodies from the grid are within the cell and nearby

				real rDt = dt / c_substeps;															// Split up the delta time depending on how many substeps there are

				for (int i = 0; i < c_substeps; ++i)												// Solve amount of substep times
				{
					for (ClothMassPoint& massPoint : m_massPoints)									// Apply gravity and damping to all mass points
					{
						massPoint.damp(rDt);
						massPoint.applyExternalForce(glmvec3{ 0, m_physics->c_gravity, 0 }, rDt);
					}

					for (const ClothConstraint& constraint : m_constraints)							// Solve all constraints
						constraint.solve(rDt);

					if (m_bodiesNearby.size())														// If there are any rigid bodies nearby
						for (ClothMassPoint& massPoint : m_massPoints)								// iterate over each mass point
							massPoint.resolvePolytopeCollisions(m_bodiesNearby, rDt);				// and do collision checks and resolve them if there are any

					if (m_massPoints[0].pos.y < m_maxMassPointDistance)								// If the ground is near
						for (ClothMassPoint& massPoint : m_massPoints)								// iterate over each mass point
							massPoint.resolveGroundCollision(rDt);									// and do a collision check and resolve it if there is one
				}
			}

			/// <summary>
			/// Create an updated vertices vector based on the new positions of the mass points
			/// </summary>
			/// <returns> A vector with vertices of same length as the initial vertices vector with
			/// updated position data. </returns>
			std::vector<vh::vhVertex> generateVertices()
			{
				int vertexCount = 0;

				for (const ClothMassPoint& massPoint : m_massPoints)
					for (const size_t vertexIndex : massPoint.m_associatedVertices)
					{
						m_vertices[vertexIndex].pos = massPoint.pos;
						vertexCount++;
					}

				return m_vertices;
			}

			/// <summary>
			/// Apply transformation to the cloth.
			/// </summary>
			/// <param name="transformation"> The transformation matrix. </param>
			/// <param name="simulateMovement"> If false, all points are fully transformed. If true,
			/// only fixed points are and the rest are dragged along by the simulation. </param>
			void applyTransformation(glmmat4 transformation, bool simulateMovement)
			{
				for (ClothMassPoint& massPoint : m_massPoints)										// Iterate over all mass points
				{
					if (!simulateMovement || massPoint.isFixed)										// Apply the full transformation if the point is fixed or movement is not																
					{																				// simulated
						massPoint.prevPos = massPoint.pos;
						massPoint.pos = transformation * glmvec4(massPoint.pos, 1);
					}

					else if (simulateMovement && !massPoint.isFixed)								// Elsewise interpolate the point to be dragged along
					{
						glmvec3 transformedPos = transformation * glmvec4(massPoint.pos, 1);
						glmvec3 posToTransPos = transformedPos - massPoint.pos;
						massPoint.prevPos = massPoint.pos;
						massPoint.pos = massPoint.pos + posToTransPos * c_movementSimulation;		// If c_movementSimulation is 0, the point is fully dragged along
					}																				// This results in a lot of stretchyness and unrealistic behaviour

					massPoint.resolvePolytopeCollisions(m_bodiesNearby, 0);							// Check for and solve collisions at the new position
				}
			}

			/// <summary>
			/// Apply a transformation to the initial position of the cloth.
			/// </summary>
			/// <param name="transformation"> The transformation matrix. </param>
			/// <param name="simulateMovement"> If false, all points are fully transformed. If true,
			/// only fixed points are and the rest are dragged along by the simulation. </param>
			void setTransformation(glmmat4 transformation, bool simulateMovement)
			{
				for (ClothMassPoint& massPoint : m_massPoints)										// Iterate over all mass points
				{
					if (!simulateMovement || massPoint.isFixed)										// Apply the full transformation if the point is fixed or movement is not																
					{																				// simulated
						massPoint.prevPos = massPoint.pos;
						massPoint.pos = transformation * glmvec4(massPoint.initialPos, 1);			// Use the initial position of the mass point
					}

					else if (simulateMovement && !massPoint.isFixed)								// Elsewise interpolate the point to be dragged along
					{
						glmvec3 transformedPos = transformation * glmvec4(massPoint.initialPos, 1);	// Use the initial position of the mass point
						glmvec3 posToTransPos = transformedPos - massPoint.pos;
						massPoint.prevPos = massPoint.pos;
						massPoint.pos = massPoint.pos + posToTransPos * c_movementSimulation;		// If c_movementSimulation is 0, the point is fully dragged along
					}																				// This results in a lot of stretchyness and unrealistic behaviour

					massPoint.resolvePolytopeCollisions(m_bodiesNearby, 0);							// Check for and solve collisions at the new position
				}
			}

		private:
			/// <summary>
			/// Checks which bodies are within the cloth's grid cell and neighbors and sufficiently
			/// nearby
			/// </summary>
			/// <param name="rigidBodyGrid"> The rigid body grid. </param>
			void updateBodiesNearby(const std::unordered_map<intpair_t, body_map>& rigidBodyGrid)
			{
				int_t gridX = static_cast<int_t>(m_massPoints[0].pos.x / m_physics->m_width);		// Cell coordinates of cloth in grid
				int_t gridZ = static_cast<int_t>(m_massPoints[0].pos.z / m_physics->m_width);

				int_t bodiesNearbyCount = 0;

				for (const auto& cell : rigidBodyGrid)												// Count how many rigid bodies are currently in cell or neighbor cells
					if (std::abs(cell.first.first - gridX) < 2
						&& std::abs(cell.first.second - gridZ) < 2)
						bodiesNearbyCount += (int_t) cell.second.size();

				if (gridX != m_gridX || gridZ != m_gridZ ||											// Only check for changes if either the cloth's cell has changed or there
					bodiesNearbyCount != m_bodiesNearbyCount)										// are new bodies nearby.
				{
					m_bodiesNearby.clear();															// Reset vector of bodies nearby

					for (const auto& cell : rigidBodyGrid)											// Iterate over all non-empty cells
						if (std::abs(cell.first.first - gridX) < 2									// Check if it is the cell of the cloth or a neighbor
							&& std::abs(cell.first.second - gridZ) < 2)
							for (auto it = cell.second.begin(); it != cell.second.end(); ++it)		// If so, add all bodies within the cell
								m_bodiesNearby.push_back(it->second);
				}

				auto it = m_bodiesNearby.begin();													// Iterate over all bodies of cloth's current cell and its neighbors to do an 
				while (it != m_bodiesNearby.end())													// additional check if the bodies are near enough to collide; This is cheaper than
				{																					// iterating through all mass points for a body that can't collide anyway														
					glmvec3 clothLocalPos =
						(*it)->m_model_inv * glmvec4(m_massPoints[0].pos, 1);						// Tranform position of cloth into bodies local space

					if (glm::length(clothLocalPos) > ((*it)->boundingSphereRadius() +				// Remove nearby body if it cannot touch cloth
						m_maxMassPointDistance) * 2)
					{
						it = m_bodiesNearby.erase(it);
						--bodiesNearbyCount;
					}
					else
						++it;
				}

				m_gridX = gridX;																	// Store current values for next call
				m_gridZ = gridZ;
				m_bodiesNearbyCount = bodiesNearbyCount;
			}

			/// <summary>
			/// Creates a mass point for each unique vertex-position
			/// nearby
			/// </summary>
			/// <param name="vertices"> The vertices of the mesh. </param>
			/// <param name="fixedPointsPositions"> The positions of the vertices that should be
			/// fixed. </param>
			void createMassPoints(const std::vector<vh::vhVertex>& vertices,
				const std::vector<glmvec3> fixedPointsPositions)
			{
				std::map<std::vector<real>, int> alreadyAddedPositions{};							// Already stored positions for duplicate removal
																									// first is the position, second the index of the corresponding mass point
				for (size_t i = 0; i < vertices.size(); ++i)
				{
					glmvec3 vertexPosGlm = vertices[i].pos;											// Convert glm::vec3 to std::vector for stl algorithms to work
					std::vector vertexPos = { vertexPosGlm.x, vertexPosGlm.y, vertexPosGlm.z };

					if (alreadyAddedPositions.count(vertexPos))										// Add an associated vertex to the mass point if it already exists
					{
						int massPointIndex = alreadyAddedPositions[vertexPos];
						m_massPoints[massPointIndex].m_associatedVertices.push_back((uint32_t) i);
					}
					else																			// Create a new mass point if none exists yet
					{
						alreadyAddedPositions[vertexPos] = (int) m_massPoints.size();

						glm::vec3 vertexPosGlm = { vertexPos[0], vertexPos[1], vertexPos[2] };		// Convert from std::vector back to glm::vec3

						ClothMassPoint massPoint(vertexPosGlm);

						if (std::find(fixedPointsPositions.begin(), fixedPointsPositions.end(),		// Fixate the point if its position is within the fixed points vector
							vertexPosGlm) != fixedPointsPositions.end())
							massPoint.isFixed = true;

						m_massPoints.push_back(massPoint);
						m_massPoints[m_massPoints.size() - 1].m_associatedVertices.
							push_back((uint32_t) i);
					}
				}
			}

			/// <summary>
			/// Check the distance between all mass points and store the longest one for collision
			/// checking.
			/// </summary>
			void calcMaxMassPointDistance()
			{
				real maxDistance = 0;

				for (const ClothMassPoint& point0 : m_massPoints)
					for (const ClothMassPoint& point1 : m_massPoints)
					{
						real distance = glm::distance(point0.pos, point1.pos);
						if (distance > maxDistance)
							m_maxMassPointDistance = distance;
					}
			}

			/// <summary>
			/// Creates triangles of mass points based on the triangles of vertices of the mesh.
			/// This is necessary since in the mesh there might be multiple vertices at a position
			/// where there is only one mass point.
			/// Also sets the inverse mass of each mass point since it depends on the triangle size.
			/// </summary>
			/// <param name="indices"> The indices vector of the mesh. </param>
			/// <returns> A vector of all triangles of mass points of the mesh. </returns>
			std::vector<ClothTriangle> createTriangles(std::vector<uint32_t> indices)
			{
				std::vector<ClothTriangle> triangles;
				ClothTriangle triangle{};

				for (uint32_t indicesIndex = 0; indicesIndex < indices.size(); ++indicesIndex)		// Iterate over all vertex indices, 3 vertices in a row form a triangle
				{
					uint32_t vertexIndex = indices[indicesIndex];									// The index of the vertex
					uint32_t associatedMassPointIndex = 0;

					for (uint32_t massPointIndex = 0; massPointIndex < m_massPoints.size();			// Find the index of the mass point that corresponds to the vertex
						++massPointIndex)
					{
						std::vector<uint32_t>& associatedVertices =
							m_massPoints[massPointIndex].m_associatedVertices;

						if (std::find(associatedVertices.begin(), associatedVertices.end(),			// The mass point corresponds if its associated vertices vector 
							vertexIndex) != associatedVertices.end())								// contains the index of the vertex
						{
							associatedMassPointIndex = massPointIndex;
							break;																	// Stop searching once the corresponding mass point was found
						}
					}

					if (indicesIndex % 3 == 0)														// Add the index of the mass point to the triangle
						triangle.massPoint0Index = associatedMassPointIndex;
					else if (indicesIndex % 3 == 1)
						triangle.massPoint1Index = associatedMassPointIndex;
					else																			// If it is the final corner of the triangle, calculate the inverse mass
					{																				// for each point and add the triangle to the triangles vector
						triangle.massPoint2Index = associatedMassPointIndex;

						ClothMassPoint& p0 = m_massPoints[triangle.massPoint0Index];
						ClothMassPoint& p1 = m_massPoints[triangle.massPoint1Index];
						ClothMassPoint& p2 = m_massPoints[triangle.massPoint2Index];

						real d0 = glm::distance(p0.pos, p1.pos);
						real d1 = glm::distance(p1.pos, p2.pos);
						real d2 = glm::distance(p2.pos, p0.pos);

						real semiPerimeter = (d0 + d1 + d2) / 2;

						real area = std::sqrt(semiPerimeter * (semiPerimeter - d0) *
							(semiPerimeter - d1) * (semiPerimeter - d2));

						real invMass = 1 / area / 3;

						p0.invMass = invMass;
						p1.invMass = invMass;
						p2.invMass = invMass;

						triangles.push_back(triangle);
					}
				}

				return triangles;
			}

			/// <summary>
			/// Generates constraints between mass points used to simulate the cloth.
			/// Two types of constraints are being generated: Edge-constraints between all pairs of
			/// mass points that form an edge of a triangle and bending-constraints between all
			/// pairs of points of triangles with a shared edge that are themselves not part of the
			/// shared edge.
			/// The method of using edge and bending constraints to simulate a cloth is commonly
			/// used with XPBD, a soft-body simulation method developed by Miles Macklin, Matthias
			/// Muller and Nuttapong Chentanez for NVIDIA.
			/// https://matthias-research.github.io/pages/publications/XPBD.pdf
			/// The algorithm for finding neighboring triangles is from this video:
			/// https://www.youtube.com/watch?v=z5oWopN39OU
			/// </summary>
			/// <param name="bendingCompliance"> The inverse of stiffness. 0 = max stiff </param>
			void generateConstraints(const std::vector<ClothTriangle>& triangles,
				real bendingCompliance)
			{
				std::vector<std::array<uint32_t, 3>> edges;											// Edge List: Each entry contains the indices of two mass points of an
																									// edge in sorted order and the third mass point index of the triangle
				for (uint32_t triangleIndex = 0; triangleIndex < triangles.size();					// Iterate over all triangles
					++triangleIndex)
				{
					ClothTriangle triangle = triangles[triangleIndex];

					std::array<uint32_t, 3> edge0 = {
						std::min(triangle.massPoint0Index, triangle.massPoint1Index),
						std::max(triangle.massPoint0Index, triangle.massPoint1Index),
						triangle.massPoint2Index,
					};

					std::array<uint32_t, 3> edge1 = {
						std::min(triangle.massPoint1Index, triangle.massPoint2Index),
						std::max(triangle.massPoint1Index, triangle.massPoint2Index),
						triangle.massPoint0Index,
					};

					std::array<uint32_t, 3> edge2 = {
						std::min(triangle.massPoint0Index, triangle.massPoint2Index),
						std::max(triangle.massPoint0Index, triangle.massPoint2Index),
						triangle.massPoint1Index,
					};

					edges.push_back(edge0);
					edges.push_back(edge1);
					edges.push_back(edge2);
				}

				std::sort(edges.begin(), edges.end());												// Sort the edge list. Shared edges are now right after each other.

				for (uint32_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)					// Iterate over all edges
					if (edgeIndex == edges.size() - 1 ||											// If it is the last edge or one of its edge mass point indices differs
						edges[edgeIndex][0] != edges[edgeIndex + 1][0] ||							// from the next entry, the edge is unique
						edges[edgeIndex][1] != edges[edgeIndex + 1][1])
					{
						ClothConstraint newEdgeConstraint(&m_massPoints[edges[edgeIndex][0]],		// Create edge constraint between the points of the edge
							&m_massPoints[edges[edgeIndex][1]], 0);

						m_constraints.push_back(newEdgeConstraint);
					}
					else																			// Elsewise the edge is shared between triangles
					{
						ClothConstraint newBendingConstraint(&m_massPoints[edges[edgeIndex][2]],	// Create bending constraint between the points not part of the edge
							&m_massPoints[edges[edgeIndex + 1][2]], bendingCompliance);

						m_constraints.push_back(newBendingConstraint);
					}
			}
		};

	//---------------------------------End-Cloth-Simulation-Stuff-----------------------------------

	};

};

//-------------------------------------------------------------------------------------------------------
//Geometry functions

namespace geometry {


	//https://box2d.org/posts/2014/02/computing-a-basis/
	inline void computeBasis(const glmvec3& a, glmvec3& b, glmvec3& c)
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
	inline bool inside(glmvec2 p, glmvec2 p1, glmvec2 p2) {
		return (p2.y - p1.y) * p.x + (p1.x - p2.x) * p.y + (p2.x * p1.y - p1.x * p2.y) >= 0;
	}

	// calculate intersection point
	inline glmvec2 intersection(glmvec2 cp1, glmvec2 cp2, glmvec2 s, glmvec2 e) {
		glmvec2 dc = { cp1.x - cp2.x, cp1.y - cp2.y };
		glmvec2 dp = { s.x - e.x, s.y - e.y };

		real n1 = cp1.x * cp2.y - cp1.y * cp2.x;
		real n2 = s.x * e.y - s.y * e.x;
		real n3 = 1.0_real / (dc.x * dp.y - dc.y * dp.x);

		return { (n1 * dp.x - n2 * dc.x) * n3, (n1 * dp.y - n2 * dc.y) * n3 };
	}

	// Sutherland-Hodgman clipping
	//https://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping#C.2B.2B
	inline void SutherlandHodgman(auto& subjectPolygon, auto& clipPolygon, auto& newPolygon) {
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

	/// <summary>
	/// Returns a unit vector that is orthogonal to vec
	/// </summary>
	inline glmvec3 orthoUnitVector(const glmvec3& vec) {
		assert(glm::length(vec) > c_eps);
		glmvec3 vec_abs = glm::abs(vec);
		int min_index = vec_abs.x < vec_abs.y ? (vec_abs.x < vec_abs.z ? 0 : 2) : (vec_abs.y < vec_abs.z ? 1 : 2);

		intpair_t indices(min_index == 0 ? intpair_t{1, 2} : (min_index == 1 ? intpair_t{0, 2} : std::pair<int, int>{ 0, 1 }));
		glmvec3 ortho(0.0_real);
		ortho[indices.first] = -vec[indices.second];
		ortho[indices.second] = vec[indices.first];

		return glm::normalize(ortho);
	}

	//--------------------------------Begin-Cloth-Simulation-Stuff----------------------------------
	// by Felix Neumann

	/// <summary>
	/// Alpha Max Plus Beta Min - Approximates square root of the sum of two squares (magnitude of a
	/// 2d vector).
	/// https://en.wikipedia.org/wiki/Alpha_max_plus_beta_min_algorithm
	/// </summary>
	inline real alphaMaxPlusBetaMin(real a, real b)
	{
		real absA = fabs(a);
		real absB = fabs(b);
		if (absA > absB)
			return (real)(0.96043387010342 * absA + 0.397824734759316 * absB);

		return (real)(0.96043387010342 * absB + 0.397824734759316 * absA);
	}

	/// <summary>
	/// Alpha Max Plus Beta Min extented to 3 dimensions. Approximates the magnitude of a 3d vector.
	/// https://math.stackexchange.com/questions/1282435/
	/// https://stackoverflow.com/questions/1582356/
	/// </summary>
	inline real alphaMaxPlusBetaMedPlusGammaMin(real a, real b, real c)
	{
		real absA = fabs(a);
		real absB = fabs(b);
		real absC = fabs(c);

		real min = std::min(absA, std::min(absB, absC));
		real max = std::max(absA, std::max(absB, absC));
		real med = std::max(std::min(absA, absB), std::min(std::max(absA, absB), absC));

		return (real)(0.939808635172325 * max + 0.389281482723725 * med + 0.29870618761438 * min);
	}
	//---------------------------------End-Cloth-Simulation-Stuff-----------------------------------
}


