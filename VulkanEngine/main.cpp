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


#if 1
using real = double;
using int_t = int64_t;
using uint_t = uint64_t;
#define glmvec2 glm::dvec2
#define glmvec3 glm::dvec3
#define glmmat3 glm::dmat3
#define glmvec4 glm::dvec4
#define glmmat4 glm::dmat4
#define glmquat glm::dquat
const double c_eps = 1.0e-8;
#else
using real = float;
using int_t = int32_t;
using uint_t = uint32_t;
#define glmvec2 glm::vec2
#define glmvec3 glm::vec3
#define glmvec4 glm::vec4
#define glmmat3 glm::mat3
#define glmmat4 glm::mat4
#define glmquat glm::quat
const double c_eps = 1.0e-5;
#endif

constexpr real operator "" _real(long double val) { return (real)val; };


const real c_gravity = -9.81_real;					//Gravity acceleration
const double c_small = 0.01_real;					//A small value
const double c_very_small = c_small / 50.0;
const real c_collision_margin_factor = 1.001_real;	//This factor makes physics bodies a little larger to prevent visible interpenetration
const real c_collision_margin = 0.005_real;			//Also a little slack for detecting collisions
const real c_sep_velocity = 0.01_real;				//Limit such that a contact is seperating and not resting
const real c_bias = 0.2_real;						//A small addon to reduce interpenetration
const real c_slop = 0.001_real;						//This much penetration does not cause bias

real	g_resting_factor = 3.0_real;				//Factor for determining when a collision velocity is actually just resting
double	g_sim_frequency = 60.0;						//Simulation frequency in Hertz
double	g_sim_delta_time = 1.0 / g_sim_frequency;	//The time to move forward the simulation
int		g_solver = 0;								//Select which solver to use
int		g_clamp_position = 1;
int		g_use_vbias = 1;								//If true, the the bias is used for resting contacts
real	g_pbias_factor = 0.5;
int		g_use_warmstart = 1;						//If true then warm start resting contacts
int		g_loops = 30;								//Number of loops in each simulation step
bool	g_deactivate = true;						//Do not move objects that are deactivated
real	g_damping = 1.0;							//damp motion of slowly moving resting objects 
real	g_restitution = 0.2;
real	g_friction = 1.0;

template <typename T> 
inline void hash_combine(std::size_t& seed, T const& v) {		//For combining hashes
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

//Pairs of data
using intpair_t = std::pair<int_t, int_t>;		//Pair of integers
using voidppair_t = std::pair<void*, void*>;	//Pair of void pointers

//Algorithms from namespace geometry, from below this file
namespace geometry {
	std::pair<glmvec3, glmvec3> closestPointsLineLine(glmvec3 p1, glmvec3 p2, glmvec3 p3, glmvec3 p4);
	real distancePointLine(glmvec3 p, glmvec3 a, glmvec3 b);
	real distancePointLinesegment(glmvec3 p, glmvec3 a, glmvec3 b);
	void computeBasis(const glmvec3& a, glmvec3& b, glmvec3& c);

	struct point2D {
		real x, y;
	};

	template<typename T>
	void SutherlandHodgman(T& subjectPolygon, T& clipPolygon, T& newPolygon);
}

//Hash functions for storing stuff in maps
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
	struct hash<geometry::point2D> {
		std::size_t operator()(const geometry::point2D& p) const {
			size_t seed = std::hash<real>()(p.x);
			hash_combine(seed, p.y);
			return seed;
		}
	};
	template<>
	struct less<geometry::point2D> {
		bool operator()(const geometry::point2D& l, const geometry::point2D& r) const {
			return (std::hash<geometry::point2D>()(l) < std::hash<geometry::point2D>()(r));
		}
	};

	//For outputting vectors/matrices to a string stream
	ostream& operator<<(ostream& os, const glmvec3& v) {
		os << "(" << v.x << ',' << v.y << ',' << v.z << ")";
		return os;
	}

	ostream& operator<<(ostream& os, const glmquat& q) {
		os << "(" << q.x << ',' << q.y << ',' << q.z << ',' << q.w << ")";
		return os;
	}

	ostream& operator<<(ostream& os, const glmmat3& m) {
		os << "(" << m[0][0] << ',' << m[0][1] << ',' << m[0][2] << ")\n";
		os << "(" << m[1][0] << ',' << m[1][1] << ',' << m[1][2] << ")\n";
		os << "(" << m[2][0] << ',' << m[2][1] << ',' << m[2][2] << ")\n";
		return os;
	}

	//Turn vector into a string
	std::string to_string(const glmvec3 v) {
		return std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z);
	}
}


//These are short hand notations to save on typing for transforming points/vectors from one coordinate space into another. 
//Naming follows this code:
//R...coordinate space of reference object
//I...coordinate space of incident object
//RT...tangent space of a face of the reference object
//W...world space
//P...point
//V...vector
//N...normal vector
#define ITORP(X) glmvec3{contact.m_body_inc.m_to_other * glmvec4{X, 1.0}}
#define ITORV(X) glmmat3{contact.m_body_inc.m_to_other} * (X)
#define ITORN(X) contact.m_body_inc.m_to_other_it * (X)
#define ITORTP(X) glmvec3{face_ref->m_LtoT * contact.m_body_inc.m_to_other * glmvec4{X, 1.0}}
#define ITTOWP(X) glmvec3{contact.m_body_inc.m_body->m_model * face_inc->m_TtoL * glmvec4{X, 1.0}}

#define ITOWP(X) glmvec3{contact.m_body_inc.m_body->m_model * glmvec4{X, 1.0}}
#define ITOWN(X) contact.m_body_inc.m_body->m_model_it * (X)

#define RTOIP(X) glmvec3{contact.m_body_ref.m_to_other * glmvec4{X, 1.0}}
#define RTOIN(X) contact.m_body_ref.m_to_other_it*(X)
#define RTOWP(X) glmvec3{contact.m_body_ref.m_body->m_model * glmvec4{X,1.0}}
#define RTOWN(X) contact.m_body_ref.m_body->m_model_it*(X)
#define RTORTP(X) glmvec3{face_ref->m_LtoT * glmvec4{X, 1.0}}

#define WTOTIP(X) glmvec3{face_inc->m_LtoT * contact.m_body_inc.m_body->m_model_inv * glmvec4{X, 1.0} }

#define RTTORP(X) glmvec3{face_ref->m_TtoL * glmvec4{(X), 1.0}}
#define RTTOWP(X) glmvec3{contact.m_body_ref.m_body->m_model * face_ref->m_TtoL * glmvec4{(X), 1.0}}

//Access body names
#define RNAME contact.m_body_ref.m_body->m_name
#define INAME contact.m_body_inc.m_body->m_name

//WTOTIP(posRW);
//glmvec3 posIW = ITTOWP(posIT);

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
		//Basic geometric objects

		/// <summary>
		/// This struct stores forces that can act on bodies.
		/// </summary>
		struct Force {
			glmvec3 m_positionL{0,0,0};			//Position in local space
			glmvec3 m_forceL{0,0,0};			//Force vector in local space
			glmvec3 m_forceW{0,0,0};			//Force vector in world space 
			glmvec3 m_accelW{0,c_gravity,0};	//Acceleration in world space (default is earth gravity)
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
			Vertex&			m_first_vertexL;		//first edge vertex
			Vertex&			m_second_vertexL;		//second edge vertex
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
			real	 m_factor{1.0};
		};

		/// <summary>
		/// Used to store the signed edge in a container.
		/// </summary>
		using SignedEdge = std::pair<Edge*,real>;

		/// <summary>
		/// A polytope consists of a number of faces. Each face consists of a sequence of edges,
		/// with the endpoint of the last edge being the starting point of the first edge.
		/// </summary>
		struct Face {
			uint_t						m_id;						//Unique number within a polytope
			std::vector<Vertex*>		m_face_vertex_ptrs{};		//pointers to the vertices of this face in correct orientation
			std::vector<geometry::point2D>	m_face_vertex2D_T{};	//vertex 2D coordinates in tangent space
			std::vector<SignedEdge>		m_face_edge_ptrs{};			//pointers to the edges of this face and orientation factors
			glmvec3						m_normalL{};				//normal vector in local space
			glmmat4						m_LtoT;						//local to face tangent space
			glmmat4						m_TtoL;						//face tangent to local space
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

		struct Collider {};

		struct Polytope : public Collider {
			std::vector<Vertex>	m_vertices{};		//positions of vertices in local space
			std::vector<Edge>	m_edges{};			//list of edges
			std::vector<Face>	m_faces{};			//list of faces

			real m_bounding_sphere_radius{ 1.0_real };

			using inertia_tensor_t = std::function<glmmat3(real, glmvec3&)>;
			inertia_tensor_t inertiaTensor;

			/// <summary>
			/// Constructor for Polytope. Take in a list of vector positions and indices and create the polygon data struct.
			/// </summary>
			/// <param name="vertices">Vertex positions in local space.</param>
			/// <param name="edgeindices">One pair of vector indices for each edge. </param>
			/// <param name="face_edge_indices">For each face a list of pairs edge index - orientation factor (1.0 or -1.0)</param>
			/// <param name="inertia_tensor"></param>
			Polytope(	const std::vector<glmvec3>& vertices, 
						const std::vector<std::pair<uint_t, uint_t>>&& edgeindices, 
						const std::vector < std::vector<signed_edge_t> >&& face_edge_indices,
						inertia_tensor_t inertia_tensor)
							: Collider{}, m_vertices{}, m_edges{}, m_faces{}, inertiaTensor{ inertia_tensor } {

				//add vertices
				uint_t id = 0;
				m_vertices.reserve(vertices.size());
				std::ranges::for_each(vertices, [&](const glmvec3& v) {		
					m_vertices.emplace_back( id++, v );
						if (real l = glm::length(v) > m_bounding_sphere_radius) m_bounding_sphere_radius = l;
					}
				);

				//add edges
				id = 0;
				m_edges.reserve(edgeindices.size());
				for (auto& edgepair : edgeindices) {	//compute edges from indices
					Edge* edge = &m_edges.emplace_back( id++, m_vertices[edgepair.first], m_vertices[edgepair.second], m_vertices[edgepair.second].m_positionL - m_vertices[edgepair.first].m_positionL);
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
						face.m_face_edge_ptrs.push_back( SignedEdge{ pe, edge.m_factor } );	//add new face to face vector
						if (edge.m_factor > 0) { face.m_face_vertex_ptrs.push_back(&pe->m_first_vertexL); } //list of vertices of this face
						else { face.m_face_vertex_ptrs.push_back(&pe->m_second_vertexL); }

						m_edges[edge.m_edge_idx].m_edge_face_ptrs.insert(&face);	//we touch each face only once, no need to check if face already in list
						m_edges[edge.m_edge_idx].m_first_vertexL.m_vertex_face_ptrs.insert(&face);	//sets cannot hold duplicates
						m_edges[edge.m_edge_idx].m_second_vertexL.m_vertex_face_ptrs.insert(&face);
					}
					glmvec3 edge0 = m_edges[face_edge_idx[0].m_edge_idx].m_edgeL * face_edge_idx[0].m_factor;
					glmvec3 edge1 = m_edges[face_edge_idx[1].m_edge_idx].m_edgeL * face_edge_idx[1].m_factor;

					glmvec3 tangent = glm::normalize( edge0 );
					face.m_normalL = glm::normalize( glm::cross( tangent, edge1 ));
					glmvec3 bitangent = glm::cross(face.m_normalL, tangent);

					face.m_TtoL = glm::translate(glmmat4(1), face.m_face_vertex_ptrs[0]->m_positionL) * glmmat4 { glmmat3{bitangent, face.m_normalL, tangent } };
					face.m_LtoT = glm::inverse(face.m_TtoL);

					for (auto& vertex : face.m_face_vertex_ptrs) {
						glmvec4 pt = face.m_LtoT * glmvec4{ vertex->m_positionL, 1.0 };
						face.m_face_vertex2D_T.emplace_back( pt.x, pt.z);
					}
				}
			};
		};

		/// <summary>
		/// This is the template for each cube. It contains the basic geometric properties of a cube:
		/// Vertces, edges and faces.
		/// </summary>
		Polytope g_cube {
			{ { -0.5,-0.5,-0.5 }, { -0.5,-0.5,0.5 }, { -0.5,0.5,0.5 }, { -0.5,0.5,-0.5 }, { 0.5,-0.5,0.5 }, { 0.5,-0.5,-0.5 }, { 0.5,0.5,-0.5 }, { 0.5,0.5,0.5 } },
			{ {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,0}, {5,0}, {1,4}, {3,6}, {7,2} }, //edges
			{	{ {0}, {1}, {2}, {3} },							//face 0
				{ {4}, {5}, {6}, {7} },							//face 1
				{ {0,-1.0}, {8,-1.0}, {4,-1.0}, {9,-1.0} },		//face 2
				{ {2,-1.0}, {11,-1.0}, {6,-1.0}, {10,-1.0} },	//face 3
				{ {3,-1.0}, {10}, {5,-1.0}, {8} },				//face 4
				{ {1,-1.0}, {9}, {7,-1.0}, {11} }				//face 5
			},
			[&](real mass, glmvec3& s) { //callback for calculating the inertia tensor of this polytope
				return mass * glmmat3{ {s.y * s.y + s.z * s.z,0,0}, {0,s.x * s.x + s.z * s.z,0}, {0,0,s.x * s.x + s.y * s.y} } / 12.0;
			}
		};

		//--------------------------------------------------------------------------------------------------
		//Physics engine stuff

		class Body;
		using body_callback = std::function<void(double, std::shared_ptr<Body>)>;

		/// <summary>
		/// THis class implements the basic physics properties of a rigid body.
		/// </summary>
		class Body {
		public:
			std::string	m_name;							//The name of this body
			void*		m_owner = nullptr;		//pointer to owner of this body, must be unique (owner is called if body moves)
			Polytope*	m_polytope = nullptr;			//geometric shape
			glmvec3		m_scale{ 1,1,1 };				//scale factor in local space
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmquat		m_orientationLW{ 1, 0, 0, 0 };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };	//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space
			body_callback* m_on_move = nullptr;			//called if the body moves
			real		m_mass_inv{ 0.0 };				//1 over mass
			real		m_restitution{ 0.0 };			//coefficient of restitution eps
			real		m_friction{ 1.0 };				//coefficient of friction mu

			std::unordered_map<uint64_t, Force> m_forces;//forces acting on this body

			//-----------------------------------------------------------------------------
			//These members are computed by the class, only change if you know what you are doing

			glmmat3		m_inertiaL{ 1.0 };				//computed when the body is created
			glmmat3		m_inertia_invL{ 1.0 };			//computed when the body is created

			//computed when the body moves
			glmmat4		m_model{ glmmat4{1} };			//model matrix at time slots
			glmmat4		m_model_inv{ glmmat4{1} };		//model inverse matrix at time slots
			glmmat3		m_model_it;						//orientation inverse transpose for bringing normal vector to world
			glmmat3		m_inertiaW{ glmmat4{1} };		//inertia tensor in world frame
			glmmat3		m_inertia_invW{ glmmat4{1} };	//inverse inertia tensor in world frame
			int_t		m_grid_x{ 0 };					//grid coordinates for broadphase
			int_t		m_grid_z{ 0 };
			glmvec3		m_pbias{0,0,0};					//extra energy if body overlaps with another body
			uint32_t	m_num_resting{0};
			real		m_damping{0.0};

			Body() { inertiaTensorL(); updateMatrices(); };

			Body(std::string name, void* owner, Polytope* polytope, glmvec3 scale, glmvec3 positionW, glmquat orientationLW = {1,0,0,0}, 
				body_callback* on_move = nullptr, glmvec3 linear_velocityW = glmvec3{ 0,0,0 }, glmvec3 angular_velocityW = glmvec3{0,0,0}, 
				real mass_inv = 0.0, real restitution = 0.0_real, real friction = 1.0_real ) :
					m_name{name}, m_owner {owner}, m_polytope{ polytope }, m_scale{ scale }, m_positionW{ positionW }, m_orientationLW{ orientationLW },
					m_on_move{on_move}, m_linear_velocityW{linear_velocityW}, m_angular_velocityW{ angular_velocityW }, 
					m_mass_inv{ mass_inv }, m_restitution{ restitution }, m_friction{ friction } { 
				m_scale *= c_collision_margin_factor;
				inertiaTensorL();
				updateMatrices(); 
			};

			bool stepPosition(double dt, glmvec3& pos, glmquat& quat) {
				bool active = !g_deactivate;

				if ( fabs(m_linear_velocityW.x) > c_small || fabs(m_linear_velocityW.z) > c_small
					|| fabs(m_linear_velocityW.y) > -g_resting_factor * c_gravity * g_sim_delta_time 
					|| m_num_resting < 3 ) {
					if(g_clamp_position==1) pos += m_linear_velocityW * (real)dt;
					active = true;
				}
				if (g_clamp_position==0) pos += m_linear_velocityW * (real)dt;
				pos += g_pbias_factor * m_pbias * (real)dt;
				m_pbias = glmvec3{ 0,0,0 };

				auto avW = glmmat3{ m_model_inv } * m_angular_velocityW;
				real len = glm::length(avW);
				if (len > c_small) {
					if(g_clamp_position == 1) quat = glm::rotate(quat, len * (real)dt, avW / len);
					active = true;
				}
				if (g_clamp_position == 0 && len != 0.0) quat = glm::rotate(quat, len * (real)dt, avW / len);

				if (active) {
					m_damping = 0.0;
				}
				else {
					m_damping = std::min( m_damping + g_damping, 60.0 );
				}

				return active;
			};

			bool stepVelocity(double dt) { 
				glmvec3 sum_accelW{ 0 };
				glmvec3 sum_forcesW{ 0 };
				glmvec3 sum_torquesW{ 0 };
				for (auto& force : m_forces) { 
					sum_accelW += force.second.m_accelW;
					sum_forcesW  += glmmat3{ m_model } * force.second.m_forceL + force.second.m_forceW;
					sum_torquesW += glm::cross(glmmat3{m_model}*force.second.m_positionL, glmmat3{m_model}*force.second.m_forceL);
				}
				m_linear_velocityW += dt * (m_mass_inv * sum_forcesW + sum_accelW);
				m_angular_velocityW += dt * m_inertia_invW * ( sum_torquesW - glm::cross(m_angular_velocityW, m_inertiaW * m_angular_velocityW));

				m_linear_velocityW.x *= 1.0 / (1.0 + g_sim_delta_time * m_damping);
				m_linear_velocityW.z *= 1.0 / (1.0 + g_sim_delta_time * m_damping);
				m_angular_velocityW *= 1.0 / (1.0 + g_sim_delta_time * m_damping);
				return true;
			}

			glmvec3 totalVelocityW(glmvec3 positionW) {
				return m_linear_velocityW + glm::cross(m_angular_velocityW, positionW - m_positionW);
			}

			real boundingSphereRadius() { 
				return std::max( m_scale.x, std::max(m_scale.y, m_scale.z)) * m_polytope->m_bounding_sphere_radius;
			}

			real mass() {
				return m_mass_inv <= c_eps ? 1.0/(c_eps*c_eps*c_eps) : 1.0 / m_mass_inv;
			}

			glmmat3 inertiaTensorL() {
				m_inertiaL = m_polytope->inertiaTensor(mass(), m_scale);
				m_inertia_invL = glm::inverse(m_inertiaL);
				return m_inertiaL;
			}

			static glmmat4 computeModel( glmvec3& pos, glmquat& orient, glmvec3& scale ) { 
				return glm::translate(glmmat4{ 1.0 }, pos) * glm::mat4_cast(orient) * glm::scale(glmmat4{ 1.0 }, scale);
			};

			void updateMatrices() {
				glmmat4 rot4 = glm::mat4_cast(m_orientationLW);
				glmmat3 rot3{rot4};

				m_model = glm::translate(glmmat4{ 1.0 }, m_positionW) * rot4 * glm::scale(glmmat4{ 1.0 }, m_scale);
				m_model_inv = glm::inverse(m_model);
				m_model_it = glm::transpose(glm::inverse(glmmat3{ rot3 }));	//transform for a normal vector

				m_inertiaW = rot3 * m_inertiaL * glm::transpose(rot3);
				m_inertia_invW = rot3 * m_inertia_invL * glm::transpose(rot3);
			}

			/// <summary>
			/// Support mapping function of polytope.
			/// </summary>
			/// <param name="dirL">Search direction in local space.</param>
			/// <returns>Pointer to support vertex of body B.</returns>
			Vertex* support(glmvec3 dirL) {
				Vertex* result(nullptr);
				real max_dp{ -std::numeric_limits<real>::max() };
				for ( auto & vert : m_polytope->m_vertices ) {
					if (real dp = glm::dot(dirL, vert.m_positionL); dp > max_dp) { result = &vert; max_dp = dp; }
				}
				return result;
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
					unknown,	//Unknown
					any,		//Any
					colliding,	//Colliding, bodies are bumping into each other
					resting,	//No collision, bodies are resting, maybe sliding
					separating	//Bodies are separating into different directions
				};

				glmvec3 m_positionW{0.0};			//Position in world coordinates
				type_t	m_type{type_t::unknown};	//Type of contact point
				real	m_restitution;				//Restitution of velcoties after a collision (if reting then = 0)
				real	m_friction;					//Friction coefficient
				glmvec3 m_r0W;						//Position in world coordinates relative to body 0 center
				glmvec3 m_r1W;						//Position in world relative to body 1 center
				glmmat3	m_K;			//mass matrix
				glmmat3	m_K_inv;		//inverse mass matrix
				real	m_vbias{ 0.0 };	//extra energy if bodies overlap
				real	m_f{ 0.0 };		//accumulates force impulses along normal (1D) during a loop run
				glmvec2	m_t{ 0.0 };		//accumulates force impulses along tangent (2D) during a loop run
			};

			uint64_t	m_last_loop{ std::numeric_limits<uint64_t>::max() }; //number of last loop this contact was valid
			BodyPtr		m_body_ref;							//reference body, we will use its local space mostly
			BodyPtr		m_body_inc;							//incident body, we will transfer its points/vectors to the ref space
			glmvec3		m_separating_axisW{0.0};			//Axis that separates the two bodies in world space

			glmvec3					m_normalW{ 0.0 };		//Contact normal
			std::array<glmvec3, 2>	m_tangentW;				//Contact tangent vector (calculated for each normal)

			std::vector<ContactPoint> m_contact_points{};		//Contact points in contact manifold in world space
			std::vector<ContactPoint> m_old_contact_points{};	//Contact points in contact manifold in world space in prev loop
			uint32_t				  m_num_old_points{0};		//Only warmstart if you have atleast N old resting points

			/// <summary>
			/// This function adds a new contact point to a contact manifold. 
			/// </summary>
			/// <param name="positionW">Position in world coordinates.</param>
			/// <param name="normalW">Normal vector in world coordinates.</param>
			/// <param name="penetration">Interpenetration depth. If <0 then there is interpenetration.</param>
			void addContactPoint(glmvec3 positionW, glmvec3 normalW, real penetration) {
				if (m_contact_points.size() == 0) {									//If first contact point
					m_normalW = normalW;											//Use its normal vector
					geometry::computeBasis(normalW, m_tangentW[0], m_tangentW[1]);	//Calculate a tangent base
				}
				auto r0W = positionW - m_body_ref.m_body->m_positionW;	//Position relative to body 0 center in world coordinates
				auto r1W = positionW - m_body_inc.m_body->m_positionW;	//Position relative to body 1 center in world coordinates
				auto vrel = m_body_inc.m_body->totalVelocityW(positionW) - m_body_ref.m_body->totalVelocityW(positionW);
				auto d = glm::dot(vrel, normalW);

				ContactPoint::type_t type;
				real vbias = 0.0;
				if (d > c_sep_velocity) {											//Separatting contact
					type = Contact::ContactPoint::type_t::separating;
				}
				else if (d > g_resting_factor * c_gravity * g_sim_delta_time ) {	//Resting contact
					type = Contact::ContactPoint::type_t::resting;
					m_body_ref.m_body->m_num_resting++;
					m_body_inc.m_body->m_num_resting++;
					vbias = (penetration < 0.0) ? c_bias * g_sim_frequency * std::max(0.0, -penetration - c_slop) : 0.0;
				}
				else { 
					type = Contact::ContactPoint::type_t::colliding;				//Colliding contact
				}

				auto restitution = std::max(m_body_ref.m_body->m_restitution, m_body_inc.m_body->m_restitution);
				restitution = (type == Contact::ContactPoint::type_t::colliding ? restitution : 0.0);
				auto friction = (m_body_ref.m_body->m_friction + m_body_inc.m_body->m_friction) / 2.0;

				auto mc0 = matrixCross3(r0W);	//Turn cross product into a matrix multiplication
				auto mc1 = matrixCross3(r1W);

				auto K = glmmat3{ 1.0 } * m_body_inc.m_body->m_mass_inv - mc1 * m_body_inc.m_body->m_inertia_invW * mc1 + //mass matrix
						 glmmat3{ 1.0 } * m_body_ref.m_body->m_mass_inv - mc0 * m_body_ref.m_body->m_inertia_invW * mc0;

				auto K_inv = glm::inverse(K);	//Inverse of mass matrix (roughly 1/mass)

				m_contact_points.emplace_back(positionW, type, restitution, friction, r0W, r1W, K, K_inv, vbias);
			}
		};

		//--------------------------------------------------------------------------------------------------

	public:

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
		double			m_current_time{ 0.0 };			//Current time
		double			m_last_time{ 0.0 };				//Last time the sim was interpolated
		double			m_last_slot{ 0.0 };				//Last time the sim was calculated
		double			m_next_slot{ g_sim_delta_time };//Next time for simulation

		/// <summary>
		/// All bodies are stored in the map m_bodies. The key is a void*, which can be used 
		/// to call back an owner if the body moves. With this key, the body can also be found.
		/// So best if there is a 1:1 correspondence. E.g., the owner can be a specific VESceneNode.
		/// </summary>
		using body_map = std::unordered_map<void*, std::shared_ptr<Body>>;
		body_map		m_bodies;						//main container of all bodies
		
		/// <summary>
		/// The broadphase uses a 2D grid of cells, each body is stored in exactly one cell.
		/// Only cells which actually contain bodies are stored in the map.
		/// </summary>
		const double	c_width{5};							//grid cell width (m)
		std::unordered_map<intpair_t, body_map > m_grid;	//broadphase grid of cells.

		std::shared_ptr<Body> m_ground = std::make_shared<Body>( Body{ "Ground", nullptr, &g_cube, {1000, 1000, 1000}, {0, -500.0_real, 0}, {1,0,0,0} });
		body_map		m_global_cell{ { nullptr, m_ground } };	//cell containing only the ground

		/// <summary>
		/// A contact is a struct that contains contact information for a pair of bodies A and B.
		/// The map hash function alsways uses the smaller of A and B first, so there is only one contact for
		/// A/B and B/A.
		/// </summary>
		std::unordered_map<voidppair_t, Contact> m_contacts;	//possible contacts resulting from broadphase

		std::unordered_map<std::string, std::vector<std::string>> m_debug_string;	//debug information
		std::unordered_map<std::string, std::vector<std::pair<uint64_t, real>>> m_debug_real;	//debug information

		//-----------------------------------------------------------------------------------------------------
		//debugging

		void debug_string(std::string key, std::string value) {
			if (m_mode != simulation_mode_t::SIMULATION_MODE_DEBUG) return;
			m_debug_string[key].emplace_back(value);
		}

		void debug_string(const std::string& key1, const std::string& key2, const std::string& rest, std::string value) {
			if (m_mode != simulation_mode_t::SIMULATION_MODE_DEBUG) return;
			if (key1 < key2) m_debug_string[key1 + key2 + rest].emplace_back(value);
			else m_debug_string[key2 + key1 + rest].emplace_back(value);
		}

		void debug_real(const std::string& key, uint64_t x, real value) {
			if (m_mode != simulation_mode_t::SIMULATION_MODE_DEBUG) return;
			m_debug_real[key].emplace_back( x, value );
		}

		void debug_real(const std::string& key1, const std::string& key2, const std::string& rest, uint64_t x, real value) {
			if (m_mode != simulation_mode_t::SIMULATION_MODE_DEBUG) return;
			if( key1 < key2 ) m_debug_real[key1 + key2 + rest].emplace_back(x, value);
			else m_debug_real[key2 + key1 + rest].emplace_back( x, value );
		}

		std::shared_ptr<Body> pickBody() {
			glmvec3 pos{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
			glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
			auto compare = [&](auto& a, auto& b) { return glm::dot(glm::normalize(a.second->m_positionW - pos), dir) < glm::dot(glm::normalize(b.second->m_positionW - pos), dir); };
			return std::ranges::max_element(m_bodies, compare)->second;
		}

		//-----------------------------------------------------------------------------------------------------

		/// <summary>
		/// This callback is used for updating the visual boy whenever the physics box moves.
		/// It is also used for extrapolating the new position between two simulation slots.
		/// </summary>
		std::function<void(double, std::shared_ptr<Body>)> onMove = [&](double dt, std::shared_ptr<Body> body) {
			VESceneNode* cube = static_cast<VESceneNode*>(body->m_owner);		//Owner is a pointer to a scene node
			glmvec3 pos = body->m_positionW;									//New position of the scene node
			glmquat orient = body->m_orientationLW;								//New orientation of the scende node
			body->stepPosition(dt, pos, orient);								//Extrapolate
			cube->setTransform(Body::computeModel(pos, orient, body->m_scale));	//Set the scene node data

		};

		std::shared_ptr<Body> m_body; // the body we can move with the debug panel (always the latest body created)

		/// <summary>
		/// Add a new body to the physics world.
		/// </summary>
		/// <param name="pbody">The new body.</param>
		void addBody( std::shared_ptr<Body> pbody ) {
			m_bodies.insert( { pbody->m_owner, pbody } );							//Put into map
			pbody->m_grid_x = static_cast<int_t>(pbody->m_positionW.x / c_width);	//2D coordinates in the broadphase grid
			pbody->m_grid_z = static_cast<int_t>(pbody->m_positionW.z / c_width);
			m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].insert({ pbody->m_owner, pbody }); //Put into broadphase grid
		}

		void createRandomBodies( auto n) {
			for (int i = 0; i < n; ++i) {
				glmvec3 pos = { rnd_unif(rnd_gen), 20 * rnd_unif(rnd_gen) + 10.0, rnd_unif(rnd_gen) };
				glmvec3 vel = { rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				real angle = rnd_unif(rnd_gen) * 10 * 3 * M_PI / 180.0;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5 };
				VESceneNode* cube;
				VECHECKPOINTER(cube = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ "Body" + std::to_string(m_bodies.size()), cube, &g_cube, scale, pos, glm::rotate(angle, glm::normalize(orient)), &onMove, vel, vrot, 1.0 / 100.0, g_restitution, g_friction };
				body.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body));
			}
		}

		void clear() {
			for (auto& body : m_bodies) getSceneManagerPointer()->deleteSceneNodeAndChildren( ((VESceneNode*) body.second->m_owner)->getName());
			m_bodies.clear();
			m_grid.clear();
		}

		void eraseBody(auto body) {
			getSceneManagerPointer()->deleteSceneNodeAndChildren(((VESceneNode*)body->m_owner)->getName());
			m_bodies.erase(body->m_owner);
			m_grid[intpair_t{ body->m_grid_x, body->m_grid_z }].erase(body->m_owner);
		}
		
		/// <summary>
		/// If a body moves, it may be transferred to another broadphase grid cell.
		/// </summary>
		/// <param name="pbody">The body that moved.</param>
		void moveBodyInGrid(std::shared_ptr<Body> pbody) {
			int_t x = static_cast<int_t>(pbody->m_positionW.x / c_width);	//2D grid coordinates
			int_t z = static_cast<int_t>(pbody->m_positionW.z / c_width);	
			if (x != pbody->m_grid_x || z != pbody->m_grid_z) {				//Did they change?
				m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].erase(pbody->m_owner); //Remove body from old cell
				pbody->m_grid_x = x;
				pbody->m_grid_z = z;
				m_grid[intpair_t{ x, z }].insert({ pbody->m_owner, pbody }); //Put body in new cell
			}
		}

		void addBias(glmvec3& old_bias, glmvec3& new_bias ) {
			auto B = new_bias;
			auto l = glm::length(old_bias);
			if (l > 0.0) {
				auto f = std::max( glm::dot(new_bias, old_bias/ l) - l, 0.0 );
				B = new_bias - f * old_bias / l;
			}
			old_bias += B;
		}

		/// <summary>
		/// Callback for frame ended event.
		/// </summary>
		/// <param name="event"></param>
		void onFrameEnded(veEvent event) {
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
				glmvec3 vel = 35.0 * rnd_unif(rnd_gen) * dir / glm::length(dir);
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				real angle = rnd_unif(rnd_gen) * 10 * 3 * M_PI / 180.0;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5 };
				VESceneNode* cube;
				VECHECKPOINTER(cube = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ "Body" + std::to_string(m_bodies.size()), cube, &g_cube, scale, positionCamera + 2.0 * dir, glm::rotate(angle, glm::normalize(orient)), &onMove, vel, vrot, 1.0 / 100.0, g_restitution, g_friction };
				body.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body));
			}
								
			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
					
				VESceneNode* cube0;
				static int dy = 0;
				VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body0{ "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{1.0}, glmvec3{positionCamera.x, 0.5 + (dy++),positionCamera.z + 4}, glmquat{ 1,0,0,0 }, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, g_restitution, g_friction };
				body0.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body0));
				
				/*VESceneNode* cube1;
				VECHECKPOINTER(cube1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(++cubeid), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				//glmquat orient{ glm::rotate(45.0*2.0*M_PI/360.0, glmvec3{1,0,0}) };
				//glmquat orient2{ glm::rotate(45.0 * 2.0 * M_PI / 360.0, glmvec3{0,0,-1}) };
				glmquat orient{ }, orient2{ };
				Body body1{ "Body" + std::to_string(m_bodies.size()), cube1, &g_cube, glmvec3{1.0}, positionCamera + glmvec3{0,0.5,4}, orient2*orient, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, g_restitution, g_friction};
				body1.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body1));
				*/
			}

			static real dx = 0.0;
			if (event.idata1 == GLFW_KEY_Y && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };

				VESceneNode* cube0;
				VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body0{ "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{1.0}, glmvec3{dx++, 0.5, 0.0}, glmquat{1,0,0,0}, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, g_restitution, g_friction };
				body0.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body0));
			}

			if (event.idata1 == GLFW_KEY_Z && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
				glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
				VESceneNode* cube0;
				VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(m_bodies.size()), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ "Body" + std::to_string(m_bodies.size()), cube0, &g_cube, glmvec3{1.0}, positionCamera + 2.0 * dir, glmquat{1,0,0,0}, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, g_restitution, g_friction };
				body.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body));
			}

			return false;
		};

		/// <summary>
		/// Callback for the frame started event. This is the main physics engine entry point.
		/// </summary>
		/// <param name="event">The vent data.</param>
		void onFrameStarted(veEvent event) {
			if (m_mode == SIMULATION_MODE_REALTIME) {	//if the engine is in realtime mode, advance time
				m_current_time = m_last_time + event.dt;
			}

			auto last_loop = m_loop;
			while (m_current_time > m_next_slot) {	//compute position/vel at time slots
				++m_loop;			//increase loop counter
				broadPhase();		//run the broad phase
				narrowPhase();		//Run the narrow phase
				warmStart();		//Warm start the resting contacts if possible

				//calculateImpulses(Contact::ContactPoint::type_t::any, g_loops, g_sim_delta_time/2.0); //calculate impulses
				if (m_run) {
					for (auto& c : m_bodies) {
						auto& body = c.second;
						body->stepVelocity(g_sim_delta_time);	//Integration step for velocity
					}
				}
				calculateImpulses(Contact::ContactPoint::type_t::any, g_loops, g_sim_delta_time); //calculate impulses

				if (m_run) {
					for (auto& c : m_bodies) {	//integrate positions and update the matrices for the bodies
						auto& body = c.second;
						bool active = body->stepPosition(g_sim_delta_time, body->m_positionW, body->m_orientationLW);
						body->updateMatrices();
					}
				}
				m_last_slot = m_next_slot;
				m_next_slot += g_sim_delta_time;
			}
			if (m_loop > last_loop) {
				for (auto& c : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, not stored anywhere
					moveBodyInGrid(c.second);
				}
			}
			for (auto& c : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, not stored anywhere
				if (m_run && c.second->m_on_move != nullptr) {
					(*c.second->m_on_move)(m_current_time - m_last_slot, c.second); //predict new pos/orient
				}
			}
			m_last_time = m_current_time;
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
				contact.m_old_contact_points = std::move(contact.m_contact_points);
				contact.m_contact_points.clear();
				contact.m_num_old_points = 0;

				if (contact.m_last_loop == m_loop) {	//is contact still possible?
					if (contact.m_body_ref.m_body->m_owner == nullptr) {
						groundTest(contact); //is the ref body the ground?
					}
					else {
						glmvec3 diff = contact.m_body_inc.m_body->m_positionW - contact.m_body_ref.m_body->m_positionW;
						real rsum = contact.m_body_ref.m_body->boundingSphereRadius() + contact.m_body_inc.m_body->boundingSphereRadius();
						if (glm::dot(diff, diff) > rsum * rsum) { ++it;  continue; }
						SAT(it->second);						//yes - test it
					}
					++it;
				}
				else { it = m_contacts.erase(it); }				//no - erase from container
			}
		}

		/// <summary>
		/// A resting point can be warmstarted with its previous normal force. This
		/// increases stacking stability. We warmstart with old resting points at the same position.
		/// But we need a minimum number of old points, otherwise there is no warm starting.
		/// </summary>
		void warmStart() {
			if (g_use_warmstart == 0) return;
			for (auto& c : m_contacts) {
				auto& contact = c.second;
				auto num0 = contact.m_body_ref.m_body->m_num_resting;
				auto num1 = contact.m_body_inc.m_body->m_num_resting;

				if (num0 > 3 && num1 > 3) {
					for (auto& cp : contact.m_contact_points) {
						if (cp.m_type != Contact::ContactPoint::resting || cp.m_f != 0.0) continue; //warmstart only once
						for (auto& coldp : contact.m_old_contact_points) {
							if (coldp.m_type != Contact::ContactPoint::resting) continue;			//warmstart only resting points
							if (glm::length(cp.m_positionW - coldp.m_positionW) < c_small) {	//if old point is at same position
								cp.m_f = coldp.m_f;				//remember old normal force
								contact.m_num_old_points++;		//increas number of old points
							}
						}
					}
				}
			}

			for (auto& c : m_contacts) {
				auto& contact = c.second;
				if (contact.m_num_old_points > 0) {					//only warmstart if we have enough resting points
					for (auto& cp : contact.m_contact_points) {
						if (cp.m_f != 0.0) {
							auto F = cp.m_f * contact.m_normalW;
							contact.m_body_ref.m_body->m_linear_velocityW += -F * contact.m_body_ref.m_body->m_mass_inv;
							contact.m_body_ref.m_body->m_angular_velocityW += contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
							contact.m_body_inc.m_body->m_linear_velocityW += F * contact.m_body_inc.m_body->m_mass_inv;
							contact.m_body_inc.m_body->m_angular_velocityW += contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
						}
					}
				}
				else {
					for (auto& cp : contact.m_contact_points) {
						cp.m_f = 0.0;
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
				if (vW.y <= c_collision_margin) {							//close to the ground?
					min_depth = std::min(min_depth, vW.y);					//remember smalles y coordinate for calculating bias
					contact.addContactPoint(vW, glmvec3{ 0,1,0 }, vW.y);	//add the contact point
				}
			}
			if (min_depth < 0.0) { 
				contact.m_body_inc.m_body->m_pbias += glmvec3{ 0, -min_depth * g_sim_frequency, 0 }; //If penetrating, calculate bias
			}
		}


		uint64_t calculateContactPointImpules(Contact &contact, Contact::ContactPoint::type_t contact_type) {
			uint64_t res = 0;
			int i = -1;
			for (auto& cp : contact.m_contact_points) {
				++i;
				if (Contact::ContactPoint::type_t::any == contact_type || cp.m_type == contact_type) {
					auto vref = contact.m_body_ref.m_body->totalVelocityW(cp.m_positionW);
					auto vinc = contact.m_body_inc.m_body->totalVelocityW(cp.m_positionW);
					auto vrel = vinc - vref;
					auto dN = glm::dot(vrel, contact.m_normalW);
					real f{ 0.0 }, t0{ 0.0 }, t1{ 0.0 };

					if (g_solver == 0) {
						auto F = cp.m_K_inv * (-cp.m_restitution * (dN + g_use_vbias * cp.m_vbias) * contact.m_normalW - vrel);
						cp.m_vbias = 0.0;
						f = glm::dot(F, contact.m_normalW);
						auto Fn = f * contact.m_normalW;
						auto Ft = F - Fn;
						t0 = -glm::dot(Ft, contact.m_tangentW[0]);
						t1 = -glm::dot(Ft, contact.m_tangentW[1]);
					}
					
					if (g_solver == 1) {
						glmmat3 mc0 = matrixCross3(cp.m_r0W);
						glmmat3 mc1 = matrixCross3(cp.m_r1W);

						glmmat3 K = -mc1 * contact.m_body_inc.m_body->m_inertia_invW * mc1 - mc0 * contact.m_body_ref.m_body->m_inertia_invW * mc0;

						auto dV = -cp.m_restitution * dN * contact.m_normalW - vrel;
						auto kn = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + glm::dot(K * contact.m_normalW, contact.m_normalW);
						f = (glm::dot(dV, contact.m_normalW) + g_use_vbias * cp.m_vbias) / kn;
						cp.m_vbias = 0.0;

						auto kt0 = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + 
							glm::dot(K * contact.m_tangentW[0], contact.m_tangentW[0]);
						t0 = -glm::dot(dV, contact.m_tangentW[0]) / kt0;

						auto kt1 = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv +
							glm::dot(K * contact.m_tangentW[1], contact.m_tangentW[1]);
						t1 = -glm::dot(dV, contact.m_tangentW[1]) / kt1;
					}

					auto tmp = cp.m_f;
					cp.m_f = std::max(tmp + f, 0.0);
					f = cp.m_f - tmp;

					glmvec2 dt{t0, t1};
					auto tmpt = cp.m_t;
					cp.m_t += dt;
					auto len = glm::length(cp.m_t);
					if (len > fabs(cp.m_f * cp.m_friction)) {
						cp.m_t *= fabs(cp.m_f * cp.m_friction) / len;
						dt = cp.m_t - tmpt;
					}

					auto F = f * contact.m_normalW - dt.x * contact.m_tangentW[0] - dt.y * contact.m_tangentW[1];

					contact.m_body_ref.m_body->m_linear_velocityW  += -F * contact.m_body_ref.m_body->m_mass_inv;
					contact.m_body_ref.m_body->m_angular_velocityW +=      contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0W, -F);
					contact.m_body_inc.m_body->m_linear_velocityW  +=  F * contact.m_body_inc.m_body->m_mass_inv;
					contact.m_body_inc.m_body->m_angular_velocityW +=      contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1W, F);
				}
			}
			return res;
		}

		void calculateImpulses( Contact::ContactPoint::type_t contact_type , uint64_t loops, double max_time) {
			uint64_t num = loops;
			auto start = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			do {
				uint64_t res = 0;
				for (auto& contact : m_contacts) { 			//loop over all contacts
					auto nres = calculateContactPointImpules(contact.second, contact_type);
					res = std::max(nres, (uint64_t)res);
				}
				num = num + res - 1;
				elapsed = std::chrono::high_resolution_clock::now() - start;
			} while (num > 0 && (m_mode == SIMULATION_MODE_DEBUG || std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() < 1.0e6 * max_time));
		}


		//----------------------------------------------------------------------------------------------------

		struct EdgeQuery {
			real	m_separation;	//separation length (negative)
			Edge*	m_edge_ref;		//pointer to reference edge (body 0)
			Edge*	m_edge_inc;		//pointer to incident edge (body 1)
			glmvec3	m_normalL;		//normal of contact (cross product) in A's space
		};

		struct FaceQuery {
			real	m_separation;	//separation length (negative)
			Face*	m_face_ref;			//pointer to reference face
			Vertex* m_vertex_inc;		//pointer to incident vertex
		};

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

			FaceQuery fq0 = queryFaceDirections(contact);
			if(fq0.m_separation > 0) return;	//found a separating axis with face normal
			std::swap(contact.m_body_ref, contact.m_body_inc);	//body 0 is the reference body having the reference face
			FaceQuery fq1 = queryFaceDirections(contact);
			if(fq1.m_separation > 0) return;	//found a separating axis with face normal

			std::swap(contact.m_body_ref, contact.m_body_inc);	//prevent flip flopping
			EdgeQuery eq = queryEdgeDirections(contact);
			if (eq.m_separation > 0) return;	//found a separating axis with edge-edge normal

			if (fq0.m_separation >= eq.m_separation * 1.001 || fq1.m_separation >= eq.m_separation * 1.001) {	//max separation is a face-vertex contact
				if (fq0.m_separation >= fq1.m_separation) { 
					createFaceContact(contact, fq0);
				}
				else {
					std::swap(contact.m_body_ref, contact.m_body_inc);	//body 0 is the reference body having the reference face
					createFaceContact(contact, fq1);
				}
			}
			else {
				createEdgeContact(contact, eq);	//max separation is an edge-edge contact
			}
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
			FaceQuery result{-std::numeric_limits<real>::max(), nullptr, nullptr };

			for (auto& face : contact.m_body_ref.m_body->m_polytope->m_faces) {
				glmvec3 n = glm::normalize(-RTOIN(face.m_normalL));
				Vertex* vertB = contact.m_body_inc.m_body->support( n );
				glmvec3 pos = ITORP(vertB->m_positionL);
				glmvec3 diff = pos - face.m_face_vertex_ptrs[0]->m_positionL;
				real distance = glm::dot(face.m_normalL, diff);
				if (distance > c_collision_margin) 
					return { distance, &face, vertB }; //no overlap - distance is positive
				if (distance > result.m_separation) 
					result = { distance, &face, vertB };
			} 
			return result; //overlap - distance is negative
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
					auto edgeb_L = ITORV(edgeB.m_edgeL);
					glmvec3 n = glm::cross(edgeA.m_edgeL, ITORV(edgeB.m_edgeL));	//axis n is cross product of both edges
					real len = glm::length(n);
					//std::cout << "edgeA " << edgeA.m_id << " RL " << edgeA.m_edgeL << "\n";
					//std::cout << "edgeB " << edgeB.m_id << " IL " << edgeB.m_edgeL << " RL " << ITORV(edgeB.m_edgeL) << "\n";
					//std::cout << "n " << n << "\n";

					if (len > 0.0) {
						n = n / len;	//normalize the axis

						if (glm::dot(n, edgeA.m_first_vertexL.m_positionL) < 0) 
							n = -n;		//n must be oriented away from center of A								
						Vertex* vertA = contact.m_body_ref.m_body->support(n);				//support of A in normal direction
						Vertex* vertB = contact.m_body_inc.m_body->support(RTOIN(-n));		//support of B in negative normal direction
						auto vBR = ITORP(vertB->m_positionL);
						auto diff = vBR - vertA->m_positionL;
						real distance = glm::dot(n, diff);		//overlap distance along n

						//std::cout << "vertA " << vertA->m_id << " RL " << vertA->m_positionL << "\n";
						//std::cout << "vertB " << vertB->m_id << " IL " << vertB->m_positionL << " RL " << vBR << "\n";
						//std::cout << "n " << n << " Diff " << diff << "\n";
						//std::cout << "Distance " << distance << "\n";

						if (distance > c_collision_margin) {
							return { distance, &edgeA, &edgeB };							//no overlap - distance is positive
						}
						
						if (distance > result.m_separation) {
							result = { distance, &edgeA, &edgeB, n };	//remember max of negative distances
						}
					}
				}
			}
			return result; 
		}

		/// <summary>
		/// We have a vertex-face contact. Test if this actually a 
		/// face-face, face-edge or just a face-vertex contact, then call the right function to create the manifold.
		/// Vertex-face contacts are projected to the referece face.
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="fq"></param>
		void createFaceContact(Contact& contact, FaceQuery& fq) {
			glmvec3 An = glm::normalize( -RTOIN(fq.m_face_ref->m_normalL) ); //transform normal vector of ref face to inc body
			Face* inc_face = maxFaceAlignment(An, fq.m_vertex_inc->m_vertex_face_ptrs, [](real x) -> real { return x; });	//do we have a face - face contact?
			real min = clipFaceFace(contact, fq.m_face_ref, inc_face);

			if (fq.m_separation < 0) {
				real weight = 1.0 / (1.0 + contact.m_body_inc.m_body->mass() * contact.m_body_ref.m_body->m_mass_inv);
				auto pbias = -RTOWN(fq.m_face_ref->m_normalL) * (-min) * g_sim_frequency * (1.0 - weight);
				addBias(contact.m_body_ref.m_body->m_pbias, pbias);
				pbias = RTOWN(fq.m_face_ref->m_normalL) * (-min) * g_sim_frequency * weight;
				addBias(contact.m_body_inc.m_body->m_pbias, pbias);
			}
		}

		/// <summary>
		/// We found a face of B that is aligned with the ref face of A. Bring inc face vertices of B into 
		/// A's face tangent space, then clip B against A. Bring the result into world space.
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="face_ref"></param>
		/// <param name="face_inc"></param>
		real clipFaceFace(Contact& contact, Face* face_ref, Face* face_inc) {
			glmvec3 v = glm::normalize( face_ref->m_face_edge_ptrs.begin()->first->m_edgeL * face_ref->m_face_edge_ptrs.begin()->second ); //????
			glmvec3 p = {};

			std::vector<geometry::point2D> points;					
			for (auto* vertex : face_inc->m_face_vertex_ptrs) {			//add face points of B's face
				auto pT = ITORTP( vertex->m_positionL );				//ransform to A's tangent space
				points.emplace_back(pT.x, pT.z);						//add as 2D point
			}
			std::vector<geometry::point2D> newPolygon;
			geometry::SutherlandHodgman(points, face_ref->m_face_vertex2D_T, newPolygon); //clip B's face against A's face

			real min = 0.0;
			for (auto& p2D : newPolygon) {
				auto p = glmvec3{ p2D.x, 0.0, p2D.y }; //cannot put comma into macro 
				glmvec3 posRW = RTTOWP(p);
				glmvec3 posIT = WTOTIP(posRW);
				posIT.y = 0.0;
				glmvec3 posIW = ITTOWP( posIT );
				auto dist = glm::dot(posIW - posRW, RTOWN(face_ref->m_normalL));
				if ( dist < c_collision_margin) {
					min = std::min(min, dist);
					contact.addContactPoint(posRW, RTOWN(face_ref->m_normalL), dist);
				}
			}
			return min;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="eq"></param>
		void createEdgeContact(Contact& contact, EdgeQuery &eq) {

			Face* ref_face = maxFaceAlignment(eq.m_normalL, eq.m_edge_ref->m_edge_face_ptrs, fabs);	//face of A best aligned with the contact normal
			Face* inc_face = maxFaceAlignment(-RTOIN(eq.m_normalL), eq.m_edge_inc->m_edge_face_ptrs, fabs);	//face of B best aligned with the contact normal
		
			real dp_ref = fabs(glm::dot(eq.m_normalL, ref_face->m_normalL));
			real dp_inc = fabs(glm::dot(eq.m_normalL, inc_face->m_normalL));
			if (dp_inc > dp_ref) { 
				std::swap(contact.m_body_ref, contact.m_body_inc);
				std::swap(ref_face, inc_face);
				std::swap(eq.m_edge_ref, eq.m_edge_inc);
			}
			real sep = clipFaceFace(contact, ref_face, inc_face);

			if (eq.m_separation < 0) {
				real weight = 1.0 / (1.0 + contact.m_body_inc.m_body->mass() * contact.m_body_ref.m_body->m_mass_inv);
				auto pbias = -RTOWN(eq.m_normalL) * (-sep) * g_sim_frequency * (1.0 - weight);
				addBias(contact.m_body_ref.m_body->m_pbias, pbias);
				pbias = RTOWN(eq.m_normalL) * (-sep) * g_sim_frequency * weight;
				addBias(contact.m_body_inc.m_body->m_pbias, pbias);
			}
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
				if (nk_option_label(ctx, "Solver A", g_solver == 0)) g_solver = 0;
				if (nk_option_label(ctx, "Solver B", g_solver == 1)) g_solver = 1;

				str << "Sim Freq " << g_sim_frequency;
				nk_layout_row_dynamic(ctx, 30, 4);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-10")) {
					g_sim_frequency = std::max(10.0, g_sim_frequency - 10.0);
					g_sim_delta_time = 1.0 / g_sim_frequency;
				}
				if (nk_button_label(ctx, "+10")) {
					g_sim_frequency += 10;
					g_sim_delta_time = 1.0 / g_sim_frequency;
				}
				if (nk_button_label(ctx, "Next time slot")) {
					m_physics->m_current_time += g_sim_delta_time;
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

				str.str("");
				str << "Loops " << g_loops;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-5")) { g_loops = std::max(5, g_loops - 5); }
				if (nk_button_label(ctx, "+5")) { g_loops += 5; }

				str.str("");
				str << "Resting Fac " << g_resting_factor;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-0.2")) { g_resting_factor = std::max(0.2, g_resting_factor - 0.2); }
				if (nk_button_label(ctx, "+0.2")) { g_resting_factor += 0.2; }

				str.str("");
				str << "Damping " << g_damping;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-0.5")) { g_damping = std::max(0.0, g_damping - 0.5); }
				if (nk_button_label(ctx, "+0.5")) { g_damping += 0.5; }

				str.str("");
				str << "PBias Fac " << g_pbias_factor;
				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				if (nk_button_label(ctx, "-0.1")) { g_pbias_factor = glm::clamp(g_pbias_factor - 0.1, 0.0, 1.0); }
				if (nk_button_label(ctx, "+0.1")) { g_pbias_factor = glm::clamp(g_pbias_factor + 0.1, 0.0, 1.0); }

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Use Bias", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", g_use_vbias == 1))
					g_use_vbias = 1;
				if (nk_option_label(ctx, "No", g_use_vbias == 0))
					g_use_vbias = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Warmstart", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", g_use_warmstart == 1))
					g_use_warmstart = 1;
				if (nk_option_label(ctx, "No", g_use_warmstart == 0))
					g_use_warmstart = 0;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Deactivate", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", g_deactivate))
					g_deactivate = true;
				if (nk_option_label(ctx, "No", !g_deactivate))
					g_deactivate = false;

				nk_layout_row_dynamic(ctx, 30, 3);
				nk_label(ctx, "Clamp Pos", NK_TEXT_LEFT);
				if (nk_option_label(ctx, "Yes", g_clamp_position == 1))
					g_clamp_position = 1;
				if (nk_option_label(ctx, "No", g_clamp_position == 0))
					g_clamp_position = 0;

				nk_layout_row_dynamic(ctx, 30, 2);
				if (nk_button_label(ctx, "Create Bodies")) {
					m_physics->createRandomBodies(20);
				}
				if (nk_button_label(ctx, "Clear Bodies")) {
					m_physics->clear();
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

				/*nk_layout_row_dynamic(ctx, 30, 1);
				nk_label(ctx, "Current Body", NK_TEXT_LEFT);

				auto* sel = "";
				if(m_physics->m_body.get() ) sel = m_physics->m_body->m_name.c_str();
				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_combo_begin_label(ctx, sel, nk_vec2(nk_widget_width(ctx), 300))) {
					nk_layout_row_dynamic(ctx, 25, 1);
					for (auto& body : m_physics->m_bodies) {
						if (nk_combo_item_label(ctx, body.second->m_name.c_str(), NK_TEXT_LEFT)) {
							sel = body.second->m_name.c_str();
							m_physics->m_body = body.second;
						}
					}
					nk_combo_end(ctx);
				}

				nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, "Pos", NK_TEXT_LEFT);
				str.str("");
				if (m_physics->m_body) str << std::setprecision(5) << m_physics->m_body->m_positionW;
				nk_layout_row_push(ctx, 250);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				nk_layout_row_end(ctx);

				nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, "Orient", NK_TEXT_LEFT);
				str.str("");
				if (m_physics->m_body) str << std::setprecision(5) << m_physics->m_body->m_orientationLW;
				nk_layout_row_push(ctx, 350);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				nk_layout_row_end(ctx);

				nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, "Lin Vel", NK_TEXT_LEFT);
				str.str("");
				if(m_physics->m_body) str << std::setprecision(5) << m_physics->m_body->m_linear_velocityW;
				nk_layout_row_push(ctx, 250);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				nk_layout_row_end(ctx);

				nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
				nk_layout_row_push(ctx, 60);
				nk_label(ctx, "Ang Vel", NK_TEXT_LEFT);
				str.str("");
				if (m_physics->m_body) str << std::setprecision(5) << m_physics->m_body->m_angular_velocityW;
				nk_layout_row_push(ctx, 250);
				nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);
				nk_layout_row_end(ctx);
				*/

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
					real dt = g_sim_delta_time;
					m_physics->m_body->m_positionW += dt * glmvec3{ m_dx, m_dy, m_dz };
					m_physics->m_body->m_orientationLW =
						glm::rotate(glmquat{ 1,0,0,0 }, dt * m_da, glmvec3{ 1, 0, 0 }) *
						glm::rotate(glmquat{ 1,0,0,0 }, dt * m_db, glmvec3{ 0, 1, 0 }) *
						glm::rotate(glmquat{ 1,0,0,0 }, dt * m_dc, glmvec3{ 0, 0, 1 }) * m_physics->m_body->m_orientationLW;

					m_physics->m_body->updateMatrices();
					m_dx = m_dy = m_dz = m_da = m_db = m_dc = 0.0;
				}
			}
			nk_end(ctx);

			//--------------------------------------------------------------------------------------

			/*if (nk_begin(ctx, "Debug String", nk_rect(500, 50, 300, 500),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
			{
				static auto* sel_debug_string = "";

				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_button_label(ctx, "Clear")) { 
					m_physics->m_debug_string.clear(); 
					sel_debug_string = "";
				}

				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_combo_begin_label(ctx, sel_debug_string, nk_vec2(nk_widget_width(ctx), 300))) {
					nk_layout_row_dynamic(ctx, 25, 1);
					for (auto& deb : m_physics->m_debug_string) {
						if (nk_combo_item_label(ctx, deb.first.c_str(), NK_TEXT_LEFT)) {
							sel_debug_string = deb.first.c_str();
						}
					}
					nk_combo_end(ctx);
				}

				size_t i = 0;
				auto size = m_physics->m_debug_string[sel_debug_string].size();
				std::vector<const char*> list;
				for (auto& s : m_physics->m_debug_string[sel_debug_string]) { list.push_back(s.c_str()); }
				struct nk_list_view view;
				nk_layout_row_dynamic(ctx, 400, 1);
				if (nk_list_view_begin(ctx, &view, "Debug Info", NK_WINDOW_BORDER, 25, size)) {
					nk_layout_row_dynamic(ctx, 25, 1);
					int num = std::clamp(size - view.begin, (size_t)0, (size_t)view.count);
					for (int i = 0; i < num; ++i) {
						nk_label(ctx, list[view.begin + i], NK_TEXT_LEFT);
					}
					nk_list_view_end(&view);
				}
			}
			nk_end(ctx);
			*/

			//--------------------------------------------------------------------------------------


			/*if (nk_begin(ctx, "Debug Real", nk_rect(500, 50, 300, 300),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
			{
				static auto* sel_debug_real = "";
				static std::vector<std::string> values;

				//clear the selection
				nk_layout_row_dynamic(ctx, 25, 1);
				if (nk_button_label(ctx, "Clear")) {
					m_physics->m_debug_real.clear();
					values.clear();
					sel_debug_real = "";
				}

				//show all possible value arrays
				nk_layout_row_dynamic(ctx, 25, 1);
				std::string new_string = "";
				if (nk_combo_begin_label(ctx, sel_debug_real, nk_vec2(nk_widget_width(ctx), 300))) {
					nk_layout_row_dynamic(ctx, 25, 1);
					for (auto& deb : m_physics->m_debug_real) {
						if (nk_combo_item_label(ctx, deb.first.c_str(), NK_TEXT_LEFT)) {
							values.push_back(deb.first);
							sel_debug_real = deb.first.c_str();
						}
					}
					nk_combo_end(ctx);
				}

				//show the selected value arrays
				size_t i = 0;
				auto size = values.size();
				std::vector<const char*> list;
				for (auto& s : values) { list.push_back(s.c_str()); }
				struct nk_list_view view;
				nk_layout_row_dynamic(ctx, 100, 1);
				if (nk_list_view_begin(ctx, &view, "Value arrays", NK_WINDOW_BORDER, 25, size)) {
					nk_layout_row_dynamic(ctx, 25, 1);
					int num = std::clamp(size - view.begin, (size_t)0, (size_t)view.count);
					for (int i = 0; i < num; ++i) {
						nk_label(ctx, list[view.begin + i], NK_TEXT_LEFT);
					}
				}
				nk_list_view_end(&view);

				if (m_physics->m_debug_real.size() && values.size()>0) {
					auto slots = std::ranges::max( values 
						| std::views::transform([&](auto& s) -> std::vector<std::pair<uint64_t,real>>& { return m_physics->m_debug_real[s]; })
						| std::views::transform([](auto& n) { return n.size(); })
					);

					auto [minval,maxval] = std::ranges::minmax( values 
						| std::views::transform([&](auto& s) -> std::vector<std::pair<uint64_t, real>>& { return m_physics->m_debug_real[s]; })
						| std::views::join | std::views::values
					);

					static std::vector<nk_color> cols;
					static bool fill = true;
					if (fill) for (int i = 0; i<100; ++i) cols.push_back( nk_rgb(100 + 150 * rand() / RAND_MAX, 100 + 150 * rand() / RAND_MAX, 100 + 150 * rand() / RAND_MAX) );
					fill = false;

					nk_layout_row_dynamic(ctx, 30, 1);
					nk_label(ctx, std::to_string(maxval).c_str(), NK_TEXT_LEFT);

					nk_layout_row_dynamic(ctx, 200, 1);
					int j = 0;
					if (nk_chart_begin_colored(ctx, NK_CHART_LINES, cols[2 * j], cols[2 * j + 1], slots, minval, maxval)) {
						for (int i = 1; i < values.size(); ++i) {
							++j;
							nk_chart_add_slot_colored(ctx, NK_CHART_LINES, cols[2 * j], cols[2 * j + 1], slots, minval, maxval);
						}
	
						for (int j = 0; j < slots; ++j) {
							for (int i = 0; i < values.size(); ++i) {
								if(j < m_physics->m_debug_real[values[i]].size()) {
									nk_chart_push_slot(ctx, m_physics->m_debug_real[values[i]][j].second, i);
								}
							}
						} 
						nk_chart_end(ctx);
					}
					nk_layout_row_dynamic(ctx, 30, 1);
					nk_label(ctx, std::to_string(minval).c_str(), NK_TEXT_LEFT);
				}
			}
			nk_end(ctx);
			*/
		}


	public:
		VEEventListenerPhysics* m_physics;

		int m_zoom = 0; ///<example data

		///Constructor of class VEEventListenerPhysicsGUI
		VEEventListenerPhysicsGUI(std::string name, VEEventListenerPhysics* physics) : VEEventListener{ name }, m_physics{ physics } {};

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
		real t1 = glm::clamp(glm::dot(p2 - p1, n2) / glm::dot(d1, n2), 0.0, 1.0);
		real t2 = glm::clamp(glm::dot(p1 - p2, n1) / glm::dot(d2, n1), 0.0, 1.0);
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
	bool inside(point2D p, point2D p1, point2D p2) {
		return (p2.y - p1.y) * p.x + (p1.x - p2.x) * p.y + (p2.x * p1.y - p1.x * p2.y) >= 0;
	}

	// calculate intersection point
	point2D intersection(point2D cp1, point2D cp2, point2D s, point2D e) {
		point2D dc = { cp1.x - cp2.x, cp1.y - cp2.y };
		point2D dp = { s.x - e.x, s.y - e.y };

		real n1 = cp1.x * cp2.y - cp1.y * cp2.x;
		real n2 = s.x * e.y - s.y * e.x;
		real n3 = 1.0 / (dc.x * dp.y - dc.y * dp.x);

		return { (n1 * dp.x - n2 * dc.x) * n3, (n1 * dp.y - n2 * dc.y) * n3 };
	}

	// Sutherland-Hodgman clipping
	//https://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping#C.2B.2B
	template<typename T>
	void SutherlandHodgman(T& subjectPolygon, T& clipPolygon, T& newPolygon) {
		point2D cp1, cp2, s, e;
		std::vector<point2D> inputPolygon;
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



	int main()
	{
		// subject polygon
		std::vector<point2D> subjectPolygon {
		{50,150}, {200,50}, {350,150},
			{350,300},{250,300},{200,250},
			{150,350},{100,250},{100,200}
		};

		// clipping polygon
		std::vector<point2D> clipPolygon { {100,100}, {300,100}, {300,300}, {100,300} };

		// define the new clipped polygon (empty)
		std::vector<point2D> newPolygon;

		// apply clipping
		SutherlandHodgman(subjectPolygon, clipPolygon, newPolygon);

		// print clipped polygon points
		cout << "Clipped polygon points:" << endl;
		for (int i = 0; i < newPolygon.size(); i++)
			cout << "(" << newPolygon[i].x << ", " << newPolygon[i].y << ")" << endl;

		return 0;
	}

}


int main() {
	bool debug = true;

	MyVulkanEngine mve(veRendererType::VE_RENDERER_TYPE_FORWARD, debug);	//enable or disable debugging (=callback, validation layers)
	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	//geometry::main();

	return 0;
}
