/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <ranges>
#include <string>
#include<iostream>
#include <cmath>

#include "VEInclude.h"

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
#define glmvec3 glm::dvec3
#define glmmat3 glm::dmat3
#define glmvec4 glm::dvec4
#define glmmat4 glm::dmat4
#define glmquat glm::dquat
const double c_eps = 1.0e-5;
#else
using real = float;
using int_t = int32_t;
using uint_t = uint32_t;
#define glmvec3 glm::vec3
#define glmvec4 glm::vec4
#define glmmat3 glm::mat3
#define glmmat4 glm::mat4
#define glmquat glm::quat
const double c_eps = 1.0e-5;
#endif

constexpr real operator "" _real(long double val) { return (real)val; };


const double c_small = 0.01;
const real c_margin_factor = 1.01;
const real c_gravity = -9.81;
const double c_sim_delta_time = 1.0 / 60.0;
const real c_resting = 4.5 * c_gravity * c_sim_delta_time; //gravity is negative!

template <typename T> 
inline void hash_combine(std::size_t& seed, T const& v) {
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

using intpair_t = std::pair<int_t, int_t>;
using voidppair_t = std::pair<void*, void*>;

namespace geometry {
	std::pair<glmvec3, glmvec3> closestPointsLineLine(glmvec3 p1, glmvec3 p2, glmvec3 p3, glmvec3 p4);
}

namespace clip {
	struct point2D {
		real x, y;
	};

	template<typename T>
	void SutherlandHodgman(T& subjectPolygon, T& clipPolygon, T& newPolygon);
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
			size_t seed = std::hash<void*>()(std::min(p.first,p.second));
			hash_combine(seed, std::max(p.first, p.second));
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
	struct hash<clip::point2D> {
		std::size_t operator()(const clip::point2D& p) const {
			size_t seed = std::hash<real>()(p.x);
			hash_combine(seed, p.y);
			return seed;
		}
	};
	template<>
	struct less<clip::point2D> {
		bool operator()(const clip::point2D& l, const clip::point2D& r) const {
			return (std::hash<clip::point2D>()(l) < std::hash<clip::point2D>()(r));
		}
	};

	ostream& operator<<(ostream& os, const glmvec3& v) {
		os << "(" << v.x << ',' << v.y << ',' << v.z << ")";
		return os;
	}
	ostream& operator<<(ostream& os, const glmmat3& m) {
		os << "(" << m[0][0] << ',' << m[0][1] << ',' << m[0][2] << ")\n";
		os << "(" << m[1][0] << ',' << m[1][1] << ',' << m[1][2] << ")\n";
		os << "(" << m[2][0] << ',' << m[2][1] << ',' << m[2][2] << ")\n";
		return os;
	}
}


#define ITORP(X) glmvec3{contact.m_body_inc.m_to_other * glmvec4{X, 1.0}}
#define ITORV(X) glmmat3{contact.m_body_inc.m_to_other}*(X)
#define ITORN(X) contact.m_body_inc.m_to_other_it*(X)

#define ITOWP(X) glmvec3{contact.m_body_inc.m_body->m_model * glmvec4{X, 1.0}}

#define RTOIP(X) glmvec3{contact.m_body_ref.m_to_other * glmvec4{X, 1.0}}
#define RTOIN(X) contact.m_body_ref.m_to_other_it*(X)

#define RTOWP(X) glmvec3{contact.m_body_ref.m_body->m_model * glmvec4{X,1.0}}
#define RTOWN(X) contact.m_body_ref.m_body->m_model_it*(X)



namespace ve {

	static std::default_random_engine rnd_gen{ 12345 };					//Random numbers
	static std::uniform_real_distribution<> rnd_unif{ 0.0f, 1.0f };		//Random numbers

	class EventListenerPhysics : public VEEventListener {
	public:

		struct Force {
			glmvec3 m_positionL{0,0,0};		//position in local space
			glmvec3 m_forceL{0,0,0};		//force vector in local space
			glmvec3 m_forceW{0,0,0};		//force vector in world space 
			glmvec3 m_accelW{0,c_gravity,0};	//acceleration in world space 
		};

		struct Face;
		struct Edge;

		struct Vertex {
			uint_t			m_id;
			glmvec3			m_positionL;		//vertex position in local space
			std::set<Face*>	m_vertex_face_ptrs;	//pointers to faces this vertex belongs to
			std::set<Edge*>	m_vertex_edge_ptrs;	//pointers to edges this vertex belongs to
		};

		struct Edge {
			uint_t			m_id;
			Vertex&			m_first_vertexL;	//first edge vertex
			Vertex&			m_second_vertexL;	//second edge vertex
			glmvec3			m_edgeL{};			//edge vector in local space 
			std::set<Face*>	m_edge_face_ptrs{};		//pointers to the two faces this edge belongs to
		};

		struct signed_edge_t {
			uint32_t m_edge_idx;
			real	 m_factor{1.0};
		};

		using SignedEdge = std::pair<Edge*,real>;

		struct Face {
			uint_t						m_id;
			std::vector<Vertex*>		m_face_vertex_ptrs{};	//pointers to the vertices of this face in correct orientation
			std::vector<clip::point2D>	m_face_vertex2D_T{};	//vertex 2D coordinates in tangent space
			std::vector<SignedEdge>		m_face_edge_ptrs{};		//pointers to the edges of this face and orientation factors
			glmvec3						m_normalL{};			//normal vector in local space
			glmmat4						m_LtoT;					//local to face tangent space
			glmmat4						m_TtoL;					//face tangent to local space
		};

		template<typename T>
		Face* maxFaceAlignment(const glmvec3 dirL, const T& faces, real (*fct)(real) ) {
			assert(faces.size() > 0);
			real max_face_alignment{ -std::numeric_limits<real>::max() };
			Face* maxface = nullptr;
			for (Face* const face : faces) {
				if (real face_alignment = fct(glm::dot(dirL, face->m_normalL)); face_alignment > max_face_alignment) {
					max_face_alignment = face_alignment;
					maxface = face;
				}
			}
			return maxface;
		}

		template<typename T>
		Edge* minEdgeAlignment(const glmvec3 dirL, const T& signed_edges) {
			assert(signed_edges.size() > 0);
			real min_abs_edge_alignment{ std::numeric_limits<real>::max() };
			Edge* minedge = nullptr;
			for (Edge* const edge : signed_edges) {
				if (real abs_edge_alignment = fabs(glm::dot(dirL, edge->m_edgeL)); abs_edge_alignment < min_abs_edge_alignment) {
					min_abs_edge_alignment = abs_edge_alignment;
					minedge = edge;
				}
			}
			return minedge;
		}


		//struct AABB {
		//	std::array<glmvec3, 2> m_verticesL;
		//};

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

					//for (auto& vert : m_vertices) {		//Min and max distance along the face normals in local space
					//	real dp = glm::dot(m_faces.back().m_normalL, vert.m_positionL);
					//}
				}
			};
		};

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
			[&](real mass, glmvec3& s) {
				return mass * glmmat3{ {s.y * s.y + s.z * s.z,0,0}, {0,s.x * s.x + s.z * s.z,0}, {0,0,s.x * s.x + s.y * s.y} } / 12.0;
			}
		};

		class Body;
		using body_callback = std::function<void(double, std::shared_ptr<Body>)>;

		class Body {
		public:
			std::string	m_name;
			void*		m_owner = nullptr;				//pointer to owner of this body, must be unique
			Polytope*	m_polytope = nullptr;			//geometric shape
			glmvec3		m_scale{ 1,1,1 };				//scale factor in local space
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmquat		m_orientationLW{ {0, 0, 0} };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };	//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space
			body_callback* m_on_move = nullptr;			//called if the body moves
			real		m_mass_inv{ 0.0 };				//1 over mass
			real		m_restitution{ 0.0 };			//coefficient of restitution eps
			real		m_friction{ 1.0 };				//coefficient of friction mu

			std::unordered_map<uint64_t, Force> m_forces;//forces acting on this body

			glmmat3		m_inertiaL{1.0};				//computed when the body is created
			glmmat3		m_inertia_invL{ 1.0 };			//computed when the body is created

			//computed when the body moves
			glmvec3		m_vbias{0.0};					//one time summand to lin velocity for removing ground penetration
			glmmat4		m_model{ glmmat4{1} };			//model matrix at time slots
			glmmat4		m_model_inv{ glmmat4{1} };		//model inverse matrix at time slots
			glmmat3		m_model_it;						//orientation inverse transpose for bringing normal vector to world
			glmmat3		m_inertiaW{ glmmat4{1} };		//inertia tensor in world frame
			glmmat3		m_inertia_invW{ glmmat4{1} };	//inverse inertia tensor in world frame
			int_t		m_grid_x{ 0 };
			int_t		m_grid_z{ 0 };

			Body() { inertiaTensorL(); updateMatrices(); };

			Body(std::string name, void* owner, Polytope* polytope, glmvec3 scale, glmvec3 positionW, glmquat orientationLW = glmvec3{0,0,0}, 
				body_callback* on_move = nullptr, glmvec3 linear_velocityW = glmvec3{ 0,0,0 }, glmvec3 angular_velocityW = glmvec3{0,0,0}, 
				real mass_inv = 0.0, real restitution = 0.0_real, real friction = 0.5_real ) :
					m_name{name}, m_owner {owner}, m_polytope{ polytope }, m_scale{ scale }, m_positionW{ positionW }, m_orientationLW{ orientationLW },
					m_on_move{on_move}, m_linear_velocityW{linear_velocityW}, m_angular_velocityW{ angular_velocityW }, 
					m_mass_inv{ mass_inv }, m_restitution{ restitution }, m_friction{ friction } { 
				m_scale *= c_margin_factor;
				inertiaTensorL();
				updateMatrices(); 
			};

			bool stepPosition(double dt, glmvec3& pos, glmquat& quat) {
				bool active = false;
				if (glm::length(m_linear_velocityW) > c_eps) {
					pos = m_positionW + (m_linear_velocityW + m_vbias) * (real)dt;
					m_vbias = glmvec3{ 0,0,0 };
					active = true;
				}

				auto avW = glmmat3{ m_model_inv } * m_angular_velocityW;
				real len = glm::length(avW);
				if (abs(len) > c_eps) {
					quat = glm::rotate(m_orientationLW, len * (real)dt, avW / len);
					active = true;
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
				return true;
			}

			glmvec3 totalVelocityW(glmvec3 positionW) {
				return m_linear_velocityW + glm::cross(m_angular_velocityW, positionW - m_positionW);
			}

			real boundingSphereRadius() { 
				return std::max( m_scale.x, std::max(m_scale.y, m_scale.z)) * m_polytope->m_bounding_sphere_radius;
			}

			real mass() {
				return m_mass_inv <= c_eps ? 1.0/(c_eps*c_eps) : 1.0 / m_mass_inv;
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

		struct Contact {

			struct BodyPtr {
				std::shared_ptr<Body> m_body;	//pointer to body
				glmmat4 m_to_other;				//transform to other body
				glmmat3 m_to_other_it;			//inverse transpose of transform to other body, for normal vectors
			};

			struct ContactPoint {
				enum type_t {
					unknown,
					any,
					colliding,
					resting,
					separating
				};

				glmvec3 m_positionW{0.0};
				glmvec3 m_normalW{0.0};
				type_t	m_type{type_t::unknown};
				real	m_restitution;
				real	m_friction;
				glmvec3 m_r0;
				glmvec3 m_r1;
				glmmat3	m_K;		//mass matrix
				glmmat3	m_K_inv;	//inverse mass matrix
				glmvec3 m_F{ 0,0,0 };
				real	m_f{ 0.0 };	//accumulates force impulses along normal during a loop run
				real	m_t{ 0.0 };	//accumulates force impulses along normal during a loop run

			};

			uint64_t	m_last_loop{ std::numeric_limits<uint64_t>::max() }; //number of last loop this contact was valid
			BodyPtr		m_body_ref;	//reference body, we will use its local space mostly
			BodyPtr		m_body_inc; //incident body, we will transfer its points/vectors to the ref space
			glmvec3		m_separating_axisW{0.0};			//Axis that separates the two bodies in world space
			std::vector<ContactPoint> m_contact_points{};	//Contact points in contact manifold in world space
			bool		m_all_resting{true};				//true if all contact points are resting
			real		m_dampen_coeff{ 0.99 };				//coefficient for dampening unwanted rotation for face/face contact

			void addContactPoint(glmvec3 positionW, glmvec3 normalW) {
				if (m_contact_points.size() == 0) m_all_resting = true;
				auto r0 = positionW - m_body_ref.m_body->m_positionW;
				auto r1 = positionW - m_body_inc.m_body->m_positionW;
				auto vrel = m_body_inc.m_body->totalVelocityW(positionW) - m_body_ref.m_body->totalVelocityW(positionW);
				auto d = glm::dot(vrel, normalW);

				ContactPoint::type_t type;
				if (d > 0) { 
					type = Contact::ContactPoint::type_t::separating;
					m_all_resting = false;
					m_dampen_coeff = 0.99;
				}
				else if (d > c_resting) { 
					type = Contact::ContactPoint::type_t::resting; 
				}
				else { 
					type = Contact::ContactPoint::type_t::colliding;
					m_all_resting = false;
					m_dampen_coeff = 0.99;
				}

				auto restitution = std::max(m_body_ref.m_body->m_restitution, m_body_inc.m_body->m_restitution);
				restitution = (type == Contact::ContactPoint::type_t::colliding ? restitution : 0.0);
				auto friction = (m_body_ref.m_body->m_friction + m_body_inc.m_body->m_friction) / 2.0;

				auto mc0 = matrixCross3(r0);
				auto mc1 = matrixCross3(r1);

				auto K = glmmat3{ 1.0 } * m_body_inc.m_body->m_mass_inv - mc1 * m_body_inc.m_body->m_inertia_invW * mc1 +
						 glmmat3{ 1.0 } * m_body_ref.m_body->m_mass_inv - mc0 * m_body_ref.m_body->m_inertia_invW * mc0;

				//auto K = -mc1 * m_body_inc.m_body->m_inertia_invW * mc1 - mc0 * m_body_ref.m_body->m_inertia_invW * mc0;
				
				auto K_inv = glm::inverse(K);

				m_contact_points.emplace_back(positionW, normalW, type, restitution, friction, r0, r1, K, K_inv);
			}
		};

	protected:
		bool			m_debug = false;
		bool			m_run = true;

		uint64_t		m_loop{ 0L };
		double			m_last_time{ 0.0 };				//last time the sim was interpolated
		double			m_last_slot{ 0.0 };				//last time the sim was calculated
		double			m_next_slot{ c_sim_delta_time };//next time for simulation

		using body_map = std::unordered_map<void*, std::shared_ptr<Body>>;
		body_map		m_bodies;						//main container of all bodies
		
		const double	c_width{5};						//grid cell width (m)
		std::unordered_map< intpair_t, body_map > m_grid;	//broadphase grid

		Body			m_ground{ "Ground", nullptr, &g_cube , { 1000, 1000, 1000 } , { 0, -500.0_real, 0 }, {1,0,0,0} };
		body_map		m_global_cell{ { nullptr, std::make_shared<Body>(m_ground) } };	//cell containing the ground

		std::unordered_map<voidppair_t, Contact> m_contacts;	//possible contacts resulting from broadphase

		std::function<void(double, std::shared_ptr<Body>)> onMove = [&](double dt, std::shared_ptr<Body> body) {
			VESceneNode* cube = static_cast<VESceneNode*>(body->m_owner);
			glmvec3 pos = body->m_positionW;
			glmquat orient = body->m_orientationLW;
			body->stepPosition(dt, pos, orient);
			cube->setTransform(Body::computeModel(pos, orient, body->m_scale));

		};

		std::shared_ptr<Body> m_body; // the body we can move with the keyboard

		void addBody( std::shared_ptr<Body> pbody ) {
			m_bodies.insert( { pbody->m_owner, pbody } );
			pbody->m_grid_x = static_cast<int_t>(pbody->m_positionW.x / c_width);
			pbody->m_grid_z = static_cast<int_t>(pbody->m_positionW.z / c_width);
			m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].insert({ pbody->m_owner, pbody });
		}
		
		void moveBodyInGrid(std::shared_ptr<Body> pbody) {
			int_t x = static_cast<int_t>(pbody->m_positionW.x / c_width);
			int_t z = static_cast<int_t>(pbody->m_positionW.z / c_width);
			//if(m_debug) std::cout << x << " " << z << "\n";
			if (x != pbody->m_grid_x || z != pbody->m_grid_z) {
				m_grid[intpair_t{ pbody->m_grid_x, pbody->m_grid_z }].erase(pbody->m_owner);
				pbody->m_grid_x = x;
				pbody->m_grid_z = z;
				m_grid[intpair_t{ x, z }].insert({ pbody->m_owner, pbody });
			}
		}

		real m_dx = 0.0;
		real m_dy = 0.0;
		real m_dz = 0.0;
		real m_da = 0.0;
		real m_db = 0.0;
		real m_dc = 0.0;

		void onFrameEnded(veEvent event) {
			if (m_body) {
				m_body->m_positionW += event.dt * glmvec3{ m_dx, m_dy, m_dz };
				m_body->m_orientationLW = m_body->m_orientationLW * 
					glm::rotate(glmquat{ {0,0,0} }, event.dt * m_da, glmvec3{1,0,0}) *
					glm::rotate(glmquat{ {0,0,0} }, event.dt * m_db, glmvec3{ 0,1,0 }) *
					glm::rotate(glmquat{ {0,0,0} }, event.dt * m_dc, glmvec3{ 0,0,1 });

				m_body->updateMatrices();
			}
		}

		bool onKeyboard(veEvent event) {
			static int64_t cubeid = 0;

			if (event.idata1 == GLFW_KEY_Z && event.idata3 == GLFW_PRESS) {
				m_debug = true;
				return true;
			}

			real vel = 0.5;
			if (event.idata1 == GLFW_KEY_I && event.idata3 == GLFW_PRESS) { m_dy = vel; }
			if (event.idata1 == GLFW_KEY_I && event.idata3 == GLFW_RELEASE) { m_dy = 0.0; }
			if (event.idata1 == GLFW_KEY_Y && event.idata3 == GLFW_PRESS) { m_dy = -vel; }
			if (event.idata1 == GLFW_KEY_Y && event.idata3 == GLFW_RELEASE) { m_dy = 0.0; }

			if (event.idata1 == GLFW_KEY_U && event.idata3 == GLFW_PRESS) { m_dz = vel; }
			if (event.idata1 == GLFW_KEY_U && event.idata3 == GLFW_RELEASE) { m_dz = 0.0; }
			if (event.idata1 == GLFW_KEY_J && event.idata3 == GLFW_PRESS) { m_dz = -vel; }
			if (event.idata1 == GLFW_KEY_J && event.idata3 == GLFW_RELEASE) { m_dz = 0.0; }

			if (event.idata1 == GLFW_KEY_H && event.idata3 == GLFW_PRESS) { m_dx = -vel; }
			if (event.idata1 == GLFW_KEY_H && event.idata3 == GLFW_RELEASE) { m_dx = 0.0; }
			if (event.idata1 == GLFW_KEY_K && event.idata3 == GLFW_PRESS) { m_dx = vel; }
			if (event.idata1 == GLFW_KEY_K && event.idata3 == GLFW_RELEASE) { m_dx = 0.0; }

			real rot = 0.5;
			if (event.idata1 == GLFW_KEY_SEMICOLON && event.idata3 == GLFW_PRESS) { m_da = -rot; }
			if (event.idata1 == GLFW_KEY_SEMICOLON && event.idata3 == GLFW_RELEASE) { m_da = 0.0; }
			if (event.idata1 == GLFW_KEY_APOSTROPHE && event.idata3 == GLFW_PRESS) { m_da = rot; }
			if (event.idata1 == GLFW_KEY_APOSTROPHE && event.idata3 == GLFW_RELEASE) { m_da = 0.0; }

			if (event.idata1 == GLFW_KEY_COMMA && event.idata3 == GLFW_PRESS) { m_db = -rot; }
			if (event.idata1 == GLFW_KEY_COMMA && event.idata3 == GLFW_RELEASE) { m_db = 0.0; }
			if (event.idata1 == GLFW_KEY_PERIOD && event.idata3 == GLFW_PRESS) { m_db = rot; }
			if (event.idata1 == GLFW_KEY_PERIOD && event.idata3 == GLFW_RELEASE) { m_db = 0.0; }

			if (event.idata1 == GLFW_KEY_SLASH && event.idata3 == GLFW_PRESS) { m_dc = -rot; }
			if (event.idata1 == GLFW_KEY_SLASH && event.idata3 == GLFW_RELEASE) { m_dc = 0.0; }
			if (event.idata1 == GLFW_KEY_RIGHT_SHIFT && event.idata3 == GLFW_PRESS) { m_dc = rot; }
			if (event.idata1 == GLFW_KEY_RIGHT_SHIFT && event.idata3 == GLFW_RELEASE) { m_dc = 0.0; }


			if (event.idata1 == GLFW_KEY_B && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };

				glmvec3 dir{ getSceneManagerPointer()->getSceneNode("StandardCamera")->getWorldTransform()[2] };
				glmvec3 vel = 30.0 * rnd_unif(rnd_gen) * dir / glm::length(dir);
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				real angle = rnd_unif(rnd_gen) * 10 * 3 * M_PI / 180.0;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 10, rnd_unif(rnd_gen) * 10, rnd_unif(rnd_gen) * 10 };
				VESceneNode* cube;
				VECHECKPOINTER(cube = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(++cubeid), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body{ "", cube, &g_cube, scale, positionCamera + 2.0 * dir, glm::rotate(angle, glm::normalize(orient)), &onMove, vel, vrot, 1.0 / 100.0, 0.2, 1 };
				body.m_forces.insert({ 0ul, Force{} });
				addBody(std::make_shared<Body>(body));
			}
								
			if (event.idata1 == GLFW_KEY_SPACE && event.idata3 == GLFW_PRESS) {
				glmvec3 positionCamera{ getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getWorldTransform()[3] };
					
				VESceneNode* cube0;
				VECHECKPOINTER(cube0 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(++cubeid), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				Body body0{ "Below", cube0, &g_cube, glmvec3{1.0}, glmvec3{positionCamera.x,positionCamera.y,positionCamera.z + 4}, glmquat{0,0,0,1}, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, 0.2, 1};
				body0.m_forces.insert({ 0ul, Force{} });
				addBody(std::make_shared<Body>(body0));
				
				/*VESceneNode* cube1;
				VECHECKPOINTER(cube1 = getSceneManagerPointer()->loadModel("The Cube" + std::to_string(++cubeid), "media/models/test/crate0", "cube.obj", 0, getRoot()));
				//glmquat orient{ glm::rotate(45.0*2.0*M_PI/360.0, glmvec3{1,0,0}) };
				//glmquat orient2{ glm::rotate(45.0 * 2.0 * M_PI / 360.0, glmvec3{0,0,-1}) };
				glmquat orient{ }, orient2{ };
				Body body1{ "Above", cube1, &g_cube, glmvec3{1.0}, positionCamera + glmvec3{0,0.5,4}, orient2*orient, &onMove, glmvec3{0.0}, glmvec3{0.0}, 1.0 / 100.0, 0.2, 1.0};
				body1.m_forces.insert({ 0ul, Force{} });
				addBody(m_body = std::make_shared<Body>(body1));
				*/
			}
			return false;
		};

		void onFrameStarted(veEvent event) {
			double current_time = m_last_time + event.dt;
			auto last_loop = m_loop;
			while (current_time > m_next_slot) {	//compute position/vel at time slots
				++m_loop;
				broadPhase();
				narrowPhase();

				//std::cout << "COLLINDING + RESTING \n";
				calculateImpulses(Contact::ContactPoint::type_t::any, 1000);
				if (m_run) {
					for (auto& c : m_bodies) {
						auto& body = c.second;
						body->stepVelocity(c_sim_delta_time);
					}
				}
				//std::cout << "RESTING \n";
				calculateImpulses(Contact::ContactPoint::type_t::resting, 1000);

				if (m_run) {
					for (auto& c : m_bodies) {
						auto& body = c.second;
						if (body->stepPosition(c_sim_delta_time, body->m_positionW, body->m_orientationLW)) {
							body->updateMatrices();
						}
					}
				}
				m_last_slot = m_next_slot;
				m_next_slot += c_sim_delta_time;
			}
			if (m_loop > last_loop) {
				m_debug = false;
				for (auto& c : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, not stored anywhere
					moveBodyInGrid(c.second);
				}
			}
			for (auto& c : m_bodies) {	//predict pos/vel at slot + delta, this is only a prediction for rendering, not stored anywhere
				if (m_run && c.second->m_on_move != nullptr) {
					(*c.second->m_on_move)(current_time - m_last_slot, c.second); //predict new pos/orient
				}
			}
			m_last_time = current_time;
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
			//if(m_debug) std::cout << m_contacts.size() << "\n";
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
			for (auto it = std::begin(m_contacts); it != std::end(m_contacts); ) {
				auto& contact = it->second;
				contact.m_contact_points.clear();
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
		/// Test if a body collides with the ground.
		/// </summary>
		/// <param name="contact">The contact information between ground and the body.</param>
		void groundTest(Contact& contact) {
			if (contact.m_body_inc.m_body->m_positionW.y > contact.m_body_inc.m_body->boundingSphereRadius()) return;
			real min_depth{ std::numeric_limits<real>::max()};
			for (auto& vL : contact.m_body_inc.m_body->m_polytope->m_vertices) {
				auto vW = ITOWP(vL.m_positionL);
				if (vW.y < 0) {
					min_depth = std::min(min_depth,vW.y);
					contact.addContactPoint(vW, glmvec3{ 0,1,0 });
				}
			}
			// if(m_debug) std::cout << std::endl;
			if (min_depth<0.0) { contact.m_body_inc.m_body->m_vbias += glmvec3{ 0, -min_depth*60.0, 0 }; }
		}

		void dampen( auto &contact, auto &n) {
			if (contact.m_all_resting) {	//if all contacts are resting (face/face) -> dampen rotation that is not along the normal axis
				auto ori0_n = glm::dot(contact.m_body_ref.m_body->m_angular_velocityW, n) * n;
				auto ori0_t = contact.m_body_ref.m_body->m_angular_velocityW - ori0_n;
				if (glm::length(ori0_t) < 10 * c_small) contact.m_body_ref.m_body->m_angular_velocityW = ori0_n + contact.m_dampen_coeff * ori0_t;

				auto ori1_n = glm::dot(contact.m_body_inc.m_body->m_angular_velocityW, n) * n;
				auto ori1_t = contact.m_body_inc.m_body->m_angular_velocityW - ori1_n;
				if (glm::length(ori1_t) < 10 * c_small) contact.m_body_inc.m_body->m_angular_velocityW = ori1_n + contact.m_dampen_coeff * ori1_t;
				contact.m_dampen_coeff = std::max(contact.m_dampen_coeff * 0.95, 0.2);
			}
		}

		uint64_t calculateContactPointImpules(Contact &contact, Contact::ContactPoint::type_t contact_type) {
			uint64_t res = 0;
			for (auto& cp : contact.m_contact_points) {
				if (Contact::ContactPoint::type_t::any == contact_type || cp.m_type == contact_type) {
					auto vref = contact.m_body_ref.m_body->totalVelocityW(cp.m_positionW);
					auto vinc = contact.m_body_inc.m_body->totalVelocityW(cp.m_positionW);
					auto vrel = vinc - vref;
					auto dN = glm::dot(vrel, cp.m_normalW);
					auto vT = vrel - dN * cp.m_normalW;
					auto dT = glm::length(vT);
					glmvec3 F{ 0,0,0 }, Fn{ 0,0,0 }, Ft{ 0,0,0 }, T{ 0,0,0 };
					real f{ 0.0 }, t{ 0.0 };

					/*
					F = cp.m_K_inv * (-cp.m_restitution * dN * cp.m_normalW - vrel); // / (real)contact.m_contact_points.size();
					f = glm::dot(F, cp.m_normalW);
					Fn = f * cp.m_normalW;
					Ft = F - Fn;
					t = glm::length(Ft);
					
					if (t != 0.0) {
						T = -Ft / t;
						if (fabs(t) > fabs(cp.m_friction * f)) {	//dynamic friction?
							f = -(1 + cp.m_restitution) * dN / glm::dot(cp.m_normalW, cp.m_K * (cp.m_normalW - cp.m_friction * T));
							t = f * cp.m_friction;
						}
					}
					*/
					
					glmmat3 mc0 = matrixCross3(cp.m_r0);
					glmmat3 mc1 = matrixCross3(cp.m_r1);

					glmmat3 K = - mc1 * contact.m_body_inc.m_body->m_inertia_invW * mc1 - mc0 * contact.m_body_ref.m_body->m_inertia_invW * mc0;

					auto dV = -cp.m_restitution * dN * cp.m_normalW - vrel;
					auto kn = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + glm::dot( K * cp.m_normalW, cp.m_normalW );
					f = glm::dot(dV, cp.m_normalW) / kn;

					if (dT != 0.0) T = vT / dT;
					auto kt = contact.m_body_ref.m_body->m_mass_inv + contact.m_body_inc.m_body->m_mass_inv + glm::dot( K * T, T);
					t = -glm::dot(dV, T) / kt;
					

					auto tmp = cp.m_f;
					cp.m_f = std::max(tmp + f, 0.0);
					f = cp.m_f - tmp;

					tmp = cp.m_t;
					cp.m_t = std::min( std::max(-cp.m_f*cp.m_friction, tmp + t), cp.m_f * cp.m_friction);
					t = cp.m_t - tmp;

					F = f * cp.m_normalW - t * T;

					cp.m_F = F / (real)contact.m_contact_points.size();

					//contact.m_body_ref.m_body->m_linear_velocityW  += -F * contact.m_body_ref.m_body->m_mass_inv;
					//contact.m_body_ref.m_body->m_angular_velocityW +=      contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0, -F);
					//contact.m_body_inc.m_body->m_linear_velocityW  +=  F * contact.m_body_inc.m_body->m_mass_inv;
					//contact.m_body_inc.m_body->m_angular_velocityW +=      contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1, F);
					//dampen(contact, cp.m_normalW);
				}
			}
			return res;
		}

		void calculateImpulses( Contact::ContactPoint::type_t contact_type = Contact::ContactPoint::type_t::colliding, uint64_t loops = 20, double max_time = 1.0 / 60.0) {
			if (contact_type == Contact::ContactPoint::type_t::any) {
				calculateImpulses(Contact::ContactPoint::type_t::colliding, 1, max_time / 2.0);
				calculateImpulses(Contact::ContactPoint::type_t::resting, loops, max_time);
				return;
			}

			uint64_t num = loops;
			auto start = std::chrono::high_resolution_clock::now();
			auto elapsed = std::chrono::high_resolution_clock::now() - start;
			do {
				//std::cout << "Loop " << num << "\n";
				uint64_t res = 0;
				for (auto& contact : m_contacts) { 			//loop over all contacts
					auto nres = calculateContactPointImpules(contact.second, contact_type);
					res = std::max(nres, (uint64_t)res);
					applyImpulsesToContact(contact.second);
				}
				//applyImpulses();
				num = num + res - 1;
				elapsed = std::chrono::high_resolution_clock::now() - start;
			} while (num > 0 && std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() < 1, 000, 000.0 * max_time);

			//if(m_debug) std::cout << std::endl;
		}

		void applyImpulsesToContact( Contact &contact ) {
			glmvec3 lin0{ 0,0,0 }, ori0{ 0,0,0 }, lin1{ 0,0,0 }, ori1{ 0,0,0 }, n{ 0,0,0 };
			for (auto& cp : contact.m_contact_points) {
				if (cp.m_type == Contact::ContactPoint::colliding || cp.m_type == Contact::ContactPoint::resting) {
					auto F = cp.m_F;

					lin0 += -F * contact.m_body_ref.m_body->m_mass_inv;
					ori0 +=      contact.m_body_ref.m_body->m_inertia_invW * glm::cross(cp.m_r0, -F);
					lin1 +=  F * contact.m_body_inc.m_body->m_mass_inv;
					ori1 +=      contact.m_body_inc.m_body->m_inertia_invW * glm::cross(cp.m_r1, F);
					n = cp.m_normalW;
				}
				cp.m_F = glmvec3{ 0,0,0 };
			}

			contact.m_body_ref.m_body->m_linear_velocityW += lin0;
			contact.m_body_ref.m_body->m_angular_velocityW += ori0;
			contact.m_body_inc.m_body->m_linear_velocityW += lin1;
			contact.m_body_inc.m_body->m_angular_velocityW += ori1;

			dampen(contact, n);
		}

		void applyImpulses() {
			for (auto it = std::begin(m_contacts); it != std::end(m_contacts); ++it) { 			//loop over all contacts
				applyImpulsesToContact(it->second);
			}
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
		void SAT(Contact& contact) {
			//if(m_debug) std::cout << contact.m_body_ref.m_body->m_owner << " " << contact.m_body_inc.m_body->m_owner << "\n";

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

			//m_run = false;
			if (fq0.m_separation >= eq.m_separation || fq1.m_separation >= eq.m_separation) {	//max separation is a face-vertex contact
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

			//for (auto& cp : contact.m_contact_points) {
			//	std::cout << "Pos= " << cp.m_positionW << " n=" << cp.m_normalW << "\n";
			//}
			//std::cout << "\n";
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
				//if(m_debug) std::cout << distance << " " << n << " " << diff << "\n";

				//real distance = glm::dot(face.m_normalL, ITORP(vertB->m_positionL) - face.m_face_vertex_ptrs[0]->m_positionL);
				//if(m_debug) std::cout << distance << "\n";
				if (distance > 0) 
					return { distance, &face, vertB }; //no overlap - distance is positive
				if (distance > result.m_separation) 
					result = { distance, &face, vertB };
			} 
			//std::cout << "Distance= " << result.m_separation << " Face Normal= "<< result.m_face_ref->m_normalL << "\n";
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
					glmvec3 n = glm::cross(edgeA.m_edgeL, ITORV(edgeB.m_edgeL));	//axis n is cross product of both edges
					real len = glm::length(n);
					if (len > 0.0) {
						n = n / len;

						if (glm::dot(n, edgeA.m_first_vertexL.m_positionL) < 0) n = -n;		//n must be oriented away from center of A								
						Vertex* vertA = contact.m_body_inc.m_body->support(n);				//support of A in normal direction
						Vertex* vertB = contact.m_body_inc.m_body->support(-RTOIN(n));		//support of B in negative normal direction
						real distance = glm::dot(n, ITORP(vertB->m_positionL) - vertA->m_positionL);//overlap distance along n

						if (distance > 0) {
							return { distance, &edgeA, &edgeB };							//no overlap - distance is positive
						}
						
						real distance2 = glm::dot(n, ITORP( edgeB.m_first_vertexL.m_positionL ) - edgeA.m_first_vertexL.m_positionL);

						if (distance2 < 0 && distance > result.m_separation) {
							result = { distance, &edgeA, &edgeB, n };	//remember max of negative distances
						}
					}
				}
			}
			return result; 
		}

		/// <summary>
		/// We have a vertex-face contact. Test if this actually a 
		/// face-face, face-edge or face-vertex contact, then call the right function to create the manifold.
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="fq"></param>
		void createFaceContact(Contact& contact, FaceQuery& fq) {
			glmvec3 An = glm::normalize( -RTOIN(fq.m_face_ref->m_normalL) ); //transform normal vector of ref face to inc body
			Face* inc_face = maxFaceAlignment(An, fq.m_vertex_inc->m_vertex_face_ptrs, [](real x) -> real { return x; });	//do we have a face - face contact?
			if (glm::dot(An, inc_face->m_normalL) > 1.0 - c_small) { 
				clipFaceFace(contact, fq.m_face_ref, inc_face); 
			} else {
				Edge* inc_edge = minEdgeAlignment(An, fq.m_vertex_inc->m_vertex_edge_ptrs); //do we have an edge - face contact?
				if (fabs(glm::dot(An, inc_edge->m_edgeL)) < c_small) { 
					clipEdgeFace(contact, fq.m_face_ref, inc_edge); 
				} else { 
					contact.addContactPoint(ITOWP(fq.m_vertex_inc->m_positionL), RTOWN(fq.m_face_ref->m_normalL));  //we have only a vertex - face contact}
				}
			}
			if (fq.m_separation < 0) {
				contact.m_body_ref.m_body->m_vbias += -RTOWN(fq.m_face_ref->m_normalL) * (-fq.m_separation) * 30.0;
				contact.m_body_inc.m_body->m_vbias += RTOWN(fq.m_face_ref->m_normalL) * (-fq.m_separation) * 30.0;
			}
		}

		/// <summary>
		/// We found a face of B that is aligned with the ref face of A. Bring inc face vertices of B into 
		/// A's face tangent space, then clip B against A. Bring the result into world space.
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="face_ref"></param>
		/// <param name="face_inc"></param>
		void clipFaceFace(Contact& contact, Face* face_ref, Face* face_inc) {
			glmvec3 v = glm::normalize( face_ref->m_face_edge_ptrs.begin()->first->m_edgeL * face_ref->m_face_edge_ptrs.begin()->second ); //????
			glmvec3 p = {};
			glmmat4 ItoRT =  face_ref->m_LtoT * contact.m_body_inc.m_to_other; //transform from B to A's face tangent space

			std::vector<clip::point2D> points;					
			for (auto* vertex : face_inc->m_face_vertex_ptrs) {			//add face points of B's face
				glmvec4 pT = ItoRT * glmvec4{ vertex->m_positionL, 1.0 };//ransform to A's tangent space
				points.emplace_back(pT.x, pT.z);						//add as 2D point
			}
			std::vector<clip::point2D> newPolygon;
			clip::SutherlandHodgman(points, face_ref->m_face_vertex2D_T, newPolygon); //clip B's face against A's face

			for (auto& p2D : newPolygon) { 
				glmvec3 posW = glmvec3{ contact.m_body_ref.m_body->m_model * face_ref->m_TtoL * glmvec4{ p2D.x, 0.0, p2D.y, 1.0 } };
				contact.addContactPoint(posW, RTOWN(face_ref->m_normalL));
			}
		}

		/// <summary>
		/// We have a face-edge contact. Bring the edge vertices into A's face tangent space, then
		/// clip them against A's ref face. Bring the resulting points back to world space.
		/// </summary>
		/// <param name="contact"></param>
		/// <param name="face_ref"></param>
		/// <param name="edge_inc"></param>
		void clipEdgeFace(Contact& contact, Face* face_ref, Edge* edge_inc) {
			glmmat4 ItoRT = face_ref->m_LtoT * contact.m_body_inc.m_to_other;	//transform from B to A's face tangent space

			std::vector<clip::point2D> points;
			glmvec4 pT = ItoRT * glmvec4{ edge_inc->m_first_vertexL.m_positionL, 1.0 };	//bring edge vertices to A's face tangent space
			points.emplace_back(pT.x, pT.z);											//add as 2D point (projection)
			pT = ItoRT * glmvec4{ edge_inc->m_second_vertexL.m_positionL, 1.0 };
			points.emplace_back(pT.x, pT.z);

			std::vector<clip::point2D> newPolygon;
			clip::SutherlandHodgman( points, face_ref->m_face_vertex2D_T, newPolygon);	//clip B's face against A's face
			std::set<clip::point2D> newPolygon2;
			for (auto& p : newPolygon) { newPolygon2.insert(p); }

			for (auto & p2D : newPolygon2 ) {	//result is exactly 2 or 3 points (if clip), we only need 2 points 
				glmvec4 posW{ contact.m_body_ref.m_body->m_model * face_ref->m_TtoL * glmvec4{ p2D.x, 0.0, p2D.y, 1.0 } };
				contact.addContactPoint(posW, RTOWN(face_ref->m_normalL));
			}
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

			if (glm::dot(ref_face->m_normalL, ITORN(inc_face->m_normalL)) > 1.0 - c_small) { //face - face
				clipFaceFace(contact, ref_face, inc_face); 
			}
			else {
				if (fabs(glm::dot(ref_face->m_normalL, ITORV(eq.m_edge_inc->m_edgeL))) < c_small) { 
					clipEdgeFace(contact, ref_face, eq.m_edge_inc); //face - edge
				}
				else { //we have only an edge - edge contact}
					auto B1 = ITORP(eq.m_edge_inc->m_first_vertexL.m_positionL);
					auto B2 = ITORP(eq.m_edge_inc->m_second_vertexL.m_positionL);

					auto posL = geometry::closestPointsLineLine( 
						eq.m_edge_ref->m_first_vertexL.m_positionL, eq.m_edge_ref->m_second_vertexL.m_positionL,
						ITORP(eq.m_edge_inc->m_first_vertexL.m_positionL), ITORP(eq.m_edge_inc->m_second_vertexL.m_positionL)
					);

					auto mid = (posL.first + posL.second) / 2.0;
					auto mid_W = RTOWP(mid);
					contact.addContactPoint(mid_W, RTOWN(eq.m_normalL));
					//m_debug = true;
				} 
			}
			if (eq.m_separation < 0) {
				contact.m_body_ref.m_body->m_vbias += -RTOWN(eq.m_normalL) * (-eq.m_separation) * 30.0;
				contact.m_body_inc.m_body->m_vbias += RTOWN(eq.m_normalL) * (-eq.m_separation) * 30.0;
			}
		}


	public:
		///Constructor of class EventListenerCollision
		EventListenerPhysics(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerPhysics() {};
	};

	

	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(veRendererType type = veRendererType::VE_RENDERER_TYPE_FORWARD, bool debug=false) : VEEngine(type, debug) {};
		~MyVulkanEngine() {};

		EventListenerPhysics *m_physics;


		///Register an event listener to interact with the user
		
		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(m_physics = new EventListenerPhysics("Physics"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(m_physics, { veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(m_physics, { veEvent::VE_EVENT_FRAME_ENDED });
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
}



//https://rosettacode.org/wiki/Sutherland-Hodgman_polygon_clipping
//rewritten for std::vector
namespace clip {

	using namespace std;


	// check if a point is on the RIGHT side of an edge
	bool inside(point2D p, point2D p1, point2D p2) {
		return (p2.y - p1.y) * p.x + (p1.x - p2.x) * p.y + (p2.x * p1.y - p1.x * p2.y) > 0;
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

	//clip::main();

	return 0;
}
