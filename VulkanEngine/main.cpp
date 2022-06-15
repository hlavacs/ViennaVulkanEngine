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

#include "VEInclude.h"

#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"

#if 1
using real = double;
using int_t = int64_t;
using uint_t = uint64_t;
#define glmvec3 glm::dvec3
#define glmmat3 glm::dmat3
#define glmvec4 glm::dvec4
#define glmmat4 glm::dmat4
#define glmquat glm::dquat
#else
using real = float;
using int_t = int32_t;
using uint_t = uint32_t;
#define glmvec3 glm::vec3
#define glmvec4 glm::vec4
#define glmmat3 glm::mat3
#define glmmat4 glm::mat4
#define glmquat glm::quat
#endif

constexpr real operator "" _real(long double val) { return (real)val; };

const double c_eps = 1.0e-10;
const real c_margin = 1.0_real;
const real c_2margin = 2.0*c_margin;

template <typename T> 
inline void hash_combine(std::size_t& seed, T const& v) {
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

using intpair_t = std::pair<int_t, int_t>;
using voidppair_t = std::pair<void*, void*>;

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

}

namespace ve {

	static std::default_random_engine e{ 12345 };					//Random numbers
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//Random numbers

	class EventListenerPhysics : public VEEventListener {
	public:

		struct Force {
			glmvec3 m_positionL{0.0};		//position in local space
			glmvec3 m_forceL{0.0};			//force vector in local space
			glmvec3 m_forceW{ 0.0 };		//force vector in world space attached at mass center
		};

		struct Face;

		struct Vertex {
			glmvec3			m_positionL;	//vertex position in local space
			std::set<Face*>	m_face_ptrs;	//pointers to faces this vertex belongs to
		};

		struct Edge {
			Vertex&			m_first_vertexL;	//first edge vertex
			Vertex&			m_second_vertexL;	//second edge vertex
			glmvec3			m_edgeL{};			//edge vector in local space 
			std::set<Face*>	m_face_ptrs{};		//pointers to the two faces this edge belongs to
		};

		struct Face {
			std::set<std::pair<Edge*, real>> m_edge_ptrs{};	//pointers to the edges of this face and orientation factors
			glmvec3 m_normalL{};							//normal vector in local space
		};

		//struct AABB {
		//	std::array<glmvec3, 2> m_verticesL;
		//};

		struct Collider {};

		struct Polytope : public Collider {
			std::vector<Vertex>	m_vertices{};		//positions of vertices in local space
			std::vector<Edge>	m_edges{};			//list of edges
			std::vector<Face>	m_faces{};			//list of faces

			/// <summary>
			/// Constructor for Polytope. Take in a list of vector positions and indices and create the polygon data struct.
			/// </summary>
			/// <param name="vertices">Vertex positions in local space.</param>
			/// <param name="edgeindices">One pair of vector indices for each edge. </param>
			/// <param name="face_edge_indices">For each face a list of pairs edge index - orientation factor (1.0 or -1.0)</param>
			Polytope(const std::vector<glmvec3>& vertices, const std::vector<std::pair<uint_t, uint_t>>&& edgeindices, const std::vector < std::vector<std::pair<uint_t,real>> >&& face_edge_indices)
				: Collider{}, m_vertices{}, m_edges{}, m_faces{} {

				std::ranges::for_each(vertices, [&](const glmvec3& v) { m_vertices.emplace_back(v); });

				for (auto& edgepair : edgeindices) {	//compute edges from indices
					m_edges.emplace_back( m_vertices[edgepair.first], m_vertices[edgepair.second], m_vertices[edgepair.second].m_positionL - m_vertices[edgepair.first].m_positionL);
				}

				for (auto& face_indices : face_edge_indices) {	//compute faces from edge indices belonging to this face
					auto& face = m_faces.emplace_back();	//add new face to face vector

					for (auto& edge : face_indices) {		//add references to the edges belonging to this face
						face.m_edge_ptrs.insert( std::make_pair( &m_edges.at(edge.first), edge.second) );	//add new face to face vector
					}

					if (face_indices.size() >= 2) {			//compute face normal
						face.m_normalL = glm::cross( m_edges[face_indices[0].first].m_edgeL * face_indices[0].second, m_edges[face_indices[1].first].m_edgeL * face_indices[1].second);
						
						for (auto& vert : m_vertices) {		//Min and max distance along the face normals in local space
							real dp = glm::dot(m_faces.back().m_normalL, vert.m_positionL);
						}
					}

					for (auto& edge : face_indices) {		//record that this face belongs to a specific edge
						m_edges[edge.first].m_face_ptrs.insert(&face);	//we touch each face only once, no need to check if face already in list
						m_edges[edge.first].m_first_vertexL.m_face_ptrs.insert(&face);	//sets cannot hold duplicates
						m_edges[edge.first].m_second_vertexL.m_face_ptrs.insert(&face);
					}
				}
			};
		};

		Polytope g_cube{
			{ {-0.5,-0.5,-0.5}, {-0.5,-0.5,0.5}, {-0.5,0.5,0.5}, {-0.5,0.5,-0.5},{0.5,-0.5,-0.5}, {0.5,-0.5,0.5}, {0.5,0.5,0.5}, {0.5,0.5,-0.5} },
			{},
			{}
		};

		struct Support {
			Vertex* m_vertexB;		//vertex in local space of B
			glmvec3 m_positionA;	//vertex position in local space of A
			real	m_distanceA;	//distance in space A
		};

		struct Body {
			void*		m_owner = nullptr;				//pointer to owner of this body
			Polytope*	m_polytope = nullptr;			//geometric shape
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmvec3		m_scale{ 1,1,1 };				//scale factor in local space

			real		m_inv_mass{ 0 };				//1 over mass
			glmvec3		m_inertia{ 1,1,1 };				//inertia tensor diagonal
			real		m_restitution{ 0.0_real };			//coefficient of restitution eps
			real		m_friction{ 0.5_real };				//coefficient of friction mu

			glmquat		m_orientationLW{ 1, 0, 0, 0 };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };	//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space

			std::unordered_map<uint64_t, Force> m_forces;//forces acting on this body

			glmmat4		m_model;						//model matrix at time slots
			glmmat4		m_model_inv;					//model inverse matrix at time slots

			std::function<void(double, std::shared_ptr<Body>)>* m_on_move = nullptr; //called if the body moves

			bool stepPosition(double dt, glmvec3& pos, glmquat quat) {
				if (abs(glm::dot(m_linear_velocityW, m_linear_velocityW)) < c_eps * c_eps) return false;
				pos = m_positionW + m_linear_velocityW * (real)dt;

				real len = glm::length(m_angular_velocityW);
				if (abs(len) < c_eps) return false;
				quat = glm::rotate(m_orientationLW, len * (real)dt, m_angular_velocityW * 1.0 / len);

				return true;
			};

			bool stepVelocity(double dt) {
				return true;
			}

			real boundingSphereRadius() { return std::max( m_scale.x, std::max(m_scale.y, m_scale.z)); }

			static glmmat4 computeModel( glmvec3 pos, glmquat orient, glmvec3 scale ) { 
				return glm::translate(glmmat4{ 1.0 }, pos) * glm::mat4_cast(orient) * glm::scale(glmmat4{ 1.0 }, scale);
			};

			auto maxFaceAlignment(glmvec3 dirL, glmmat4 BtoA) -> std::pair<uint_t, real> {
				real max_abs_face_alignment{ std::numeric_limits<real>::min() };
				uint_t maxfi = 0;
				for (uint_t fi = 0; auto & face : m_polytope->m_faces) {
					if (real abs_face_alignment = abs(glm::dot(dirL, face.m_normalL)) > max_abs_face_alignment) {
						max_abs_face_alignment = abs_face_alignment;
						maxfi = fi;
					}
					++fi;
				}
				return { maxfi, abs(glm::dot(BtoA * glmvec4{ dirL, 0.0 }, BtoA * glmvec4{ m_polytope->m_faces[maxfi].m_normalL, 1.0 })) };
			}

			/// <summary>
			/// Support mapping function of polytope.
			/// </summary>
			/// <param name="dirL">Search directory in local space.</param>
			/// <param name="BtoA">Transform for the result.</param>
			/// <returns>Pointer to vertex, poisition of vertex in local space of A and max distance into the direction, transformed by BtoA.</returns>
			auto support(glmvec3 dirL, glmmat4 BtoA = glmmat4{ 1.0 }) -> Support {
				Support ret{ nullptr, {}, std::numeric_limits<real>::min() };
				if (m_polytope->m_vertices.size() == 0) return ret;

				for ( auto & vert : m_polytope->m_vertices ) {
					real dp = glm::dot(dirL, vert.m_positionL);
					if (dp > ret.m_distanceA) { ret.m_vertexB = &vert;  ret.m_distanceA = dp; }
				}
				ret.m_positionA = glmvec3{ BtoA * glmvec4{ ret.m_vertexB->m_positionL, 1.0 } };
				return { ret.m_vertexB, ret.m_positionA, glm::dot(glmmat3{BtoA} * dirL, ret.m_positionA) };
			};
		};

		struct Contact {
			struct ContactPoint {
				glmvec3 m_positionL{};
				glmvec3 m_normalL{};
			};

			std::array<std::shared_ptr<Body>, 2> m_bodies{};	//pointer to the two bodies involved into this contact
			uint64_t					m_last_loop{ std::numeric_limits<uint64_t>::max() }; //number of last loop this contact was valid
			real						m_separation_distance{ 0.0 };			//distance between the objects (if negative then they overlap)
			glmvec3						m_separating_axisL{0.0};				//Axis that separates the two bodies in object space A
			Face*						m_reference_face;		//pointers to the colling reference face
			Face*						m_incident_face;		//pointers to the colling incident face
			std::vector<ContactPoint>	m_contact_points{};		//up to 4 contact points in contact manifold
		};

	protected:

		uint64_t		m_loop{ 0L };
		double			m_last_time{ 0.0 };					//last time the sim was interpolated
		double			m_last_slot{ 0.0 };					//last time the sim was calculated
		const double	m_delta_slot{ 1.0 / 60.0 };	//sim frequency
		double			m_next_slot{ m_delta_slot };			//next time for simulation

		using body_map = std::unordered_map<void*, std::shared_ptr<Body>>;
		body_map		m_bodies;									//main container of all bodies
		
		const double	c_width{5};								//grid cell width (m)
		std::unordered_map< intpair_t, body_map > m_grid;		//broadphase grid

		Body			m_ground{nullptr, &g_cube, { 0, -0.5_real, 0 } , { 1000, 1, 1000 } };
		body_map		m_global_cell;							//cell containing the ground

		std::unordered_map<voidppair_t, Contact> m_contacts;	//possible contacts resulting from broadphase


		virtual void onFrameStarted(veEvent event) {
			//getSceneManagerPointer()->getSceneNode("The Cube Parent")->setPosition(glmvec3(d(e), 1.0f, d(e)));
			//glmvec3 positionCube   = getSceneManagerPointer()->getSceneNode("The Cube Parent")->getPosition();
			//glmvec3 positionCamera = getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getPosition();
			//VESceneNode *eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
			//eParent->setPosition(glmvec3(d(e), 1.0f, d(e)));
			//getSceneManagerPointer()->deleteSceneNodeAndChildren("The Cube"+ std::to_string(cubeid));
			//VECHECKPOINTER(getSceneManagerPointer()->loadModel("The Cube"+ std::to_string(++cubeid)  , "media/models/test/crate0", "cube.obj", 0, eParent) );

			double current_time = m_last_time + event.dt;
			while (current_time > m_next_slot) {
				++m_loop;
				broadPhase();
				narrowPhase();
				applyImpulses();

				for (auto& c : m_bodies) {
					c.second->stepVelocity(m_delta_slot);
				}

				for (auto& c : m_bodies) {
					if( c.second->stepPosition(m_delta_slot, c.second->m_positionW, c.second->m_orientationLW )) {					
						c.second->m_model = Body::computeModel(c.second->m_positionW, c.second->m_orientationLW, c.second->m_scale);
						c.second->m_model_inv = glm::inverse(c.second->m_model);
					}
				}

				m_last_slot = m_next_slot;
				m_next_slot += m_delta_slot;
			}
			for (auto& c : m_bodies)
				if(c.second->m_on_move != nullptr) 
					(*c.second->m_on_move)(current_time - m_last_slot, c.second); //predict new pos/orient

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
						if (it != m_contacts.end()) it->second.m_last_loop = m_loop;				//update loop count
						else m_contacts.insert({ { coll.second->m_owner, neigh.second->m_owner }, {{coll.second, neigh.second}, m_loop} });
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
				//for (auto& pi : c_pairs) {	//create pairs of neighborig cells and make body pairs.
				//	intpair_t ni = { cell.first.first + pi.first, cell.first.second + pi.second };
				//	if ( m_grid.count(ni) > 0) makeBodyPairs(cell.second, m_grid.at(ni)); 
				//} 
			}
		}


		/// <summary>
		/// For each pair coming from the broadphase, test whether the bodies touch each other. If so then
		/// compute the contact manifold.
		/// </summary>
		void narrowPhase() {
			for (auto it = std::begin(m_contacts); it != std::end(m_contacts); ) {
				if (it->second.m_last_loop == m_loop ) {	//is contact still possible?
					if (it->second.m_bodies[0].get() == &m_ground) groundTest(it->second);


					//SAT(it->second);						//yes - test it
					++it;
				}
				else { m_contacts.erase(it); }				//no - erase from container
			}
		}


		void groundTest(Contact& contact) {
			if (contact.m_bodies[1]->m_positionW.y > contact.m_bodies[1]->boundingSphereRadius()) return;
			contact.m_contact_points.clear();
			int_t cnt = 0;
			for (auto& vL : contact.m_bodies[1]->m_polytope->m_vertices) {
				auto vW = contact.m_bodies[1]->m_model * glmvec4{ vL.m_positionL, 1 };
				if (vW.y < 0) {
					contact.m_contact_points.emplace_back(glmvec3{ vW }, glmvec3{ 0,1,0 });
					if (++cnt > 3) return;
				}
			}
		}


		struct EdgeQuery {
			uint_t	m_reference;
			real	m_separation;
			Edge* m_edge_ref;
			Edge* m_edge_inc;
		};

		struct FaceQuery {
			uint_t	m_reference;
			real	m_separation;
			Face* m_face;
			Vertex* m_vertex;
		};

		/// <summary>
		/// Perform SAT test for two bodies. If they overlap then compute the contact manifold.
		/// </summary>
		/// <param name="contact">The contact between the two bodies.</param>
		void SAT(Contact& contact) {
			glmmat4 AtoB = contact.m_bodies[1]->m_model_inv * contact.m_bodies[0]->m_model; //transform to bring space A to space B
			glmmat4 BtoA = contact.m_bodies[0]->m_model_inv * contact.m_bodies[1]->m_model; //transform to bring space B to space A

			FaceQuery fq0, fq1;
			EdgeQuery eq;
			if ((fq0 = queryFaceDirections(contact, 0, AtoB, BtoA)).m_separation	> 0) return;
			if ((fq1 = queryFaceDirections(contact, 1, BtoA, AtoB)).m_separation	> 0) return;
			if ((eq = queryEdgeDirections(contact, AtoB, BtoA)).m_separation		> 0) return;

			if (fq0.m_separation > eq.m_separation && fq1.m_separation > eq.m_separation) {
				fq0.m_separation > fq1.m_separation ? createFaceContact( contact, fq0 ) : createFaceContact(contact, fq1);
			}
			else createEdgeContact( contact, eq );

			createManifold(contact);
		}

		/// <summary>
		/// Loop through all faces of body a. Find min and max of body b along the direction of a's face normal.
		/// Return negative number if they overlap. Return positive number if they do not overlap.
		/// </summary>
		/// <param name="contact">The pair contact struct.</param>
		/// <param name="a">Index of reference body A.</param>
		/// <param name="b">Index of incident body B.</param>
		/// <param name="AtoB">Transform to get from object space A to object space B.</param>
		/// <param name="BtoA"></param>
		/// <returns>Negative: overlap of bodies along this axis. Positive: distance between the bodies.</returns>
		FaceQuery queryFaceDirections(Contact& contact, uint_t a, glmmat4& AtoB, glmmat4& BtoA) {
			uint_t b = 1 - a;
			Vertex* maxVertexB{nullptr};
			Face* maxFace{ nullptr };
			real max_distance{std::numeric_limits<real>::min()};

			glmmat3 AtoBit = glm::transpose(glm::inverse(glmmat3{AtoB}));	//transform for a normal vector

			for (auto& face : contact.m_bodies[a]->m_polytope->m_faces) {			
				Support s = contact.m_bodies[b]->support(AtoBit * face.m_normalL * -1.0, BtoA);
				real distance = glm::dot( face.m_normalL, s.m_positionA - face.m_edge_ptrs.begin()->first->m_first_vertexL.m_positionL);
				if (distance > 0) return { a, distance, &face, s.m_vertexB }; //no overlap - distance is positive

				if (distance > max_distance) {
					max_distance = distance;
					maxFace = &face;
					maxVertexB = s.m_vertexB;
				}
			}
			return { a, max_distance, maxFace, maxVertexB }; //overlap - distance is negative
		}

		/// <summary>
		/// Loop over all edge pairs, where one edge is from nody 0, and one is from body 1. 
		/// Create the cross product vector L of an edge pair.
		/// Choose a reference body A and incident body B.
		/// Reference body A is the one having the face that is most aligned with L (abs value). The other is the
		/// incident body B.
		/// Choose L such that it points away from A. Then compute overlap between A and B.
		/// Return a negative number if there is overlap. Return a positive number if there is no overlap.
		/// </summary>
		/// <param name="contact">The contact pair.</param>
		/// <param name="AtoB">Transform from object space A to B.</param>
		/// <param name="BtoA">Transform from object space B to A.</param>
		/// <returns>Negative: overlap of bodies along this axis. Positive: distance between the bodies.</returns>
		EdgeQuery queryEdgeDirections(Contact& contact, glmmat4& AtoB, glmmat4& BtoA) {
			uint_t max_ref{0};
			real max_distance{std::numeric_limits<real>::min()};
			Edge* edge0{nullptr}, * edge1{nullptr};

			glmmat3 AtoBit = glm::transpose(glm::inverse(glmmat3{ AtoB }));	//transform for a normal vector

			for (auto& edgeA : contact.m_bodies[0]->m_polytope->m_edges) {
				for (auto& edgeB : contact.m_bodies[1]->m_polytope->m_edges) {
					glmvec3 n = glm::cross(edgeA.m_edgeL, glmmat3{ BtoA } * edgeB.m_edgeL);	//axis L is cross product of both edges
					if (glm::dot( n, edgeA.m_first_vertexL.m_positionL) < 0) n = -n;			//L must be oriented away from center of A								
					Support s = contact.m_bodies[1]->support(glmmat3{ AtoBit } * n * -1.0, BtoA);

					real distance = glm::dot( n, s.m_positionA - edgeA.m_first_vertexL.m_positionL);
					if (distance > 0) return { 0, distance, &edgeA, &edgeB }; //no overlap - distance is positive

					if (distance > max_distance) {
						edge0 = &edgeA;
						edge1 = &edgeB;
						max_distance = distance;
					}

					//auto [fi1, max_alignment1] = contact.m_bodies[0]->maxFaceAlignment( contact.m_bodies[0]->m_model_inv * glmvec4{ L, 0.0 }, contact.m_bodies[0]->m_model);
					//auto [fi2, max_alignment2] = contact.m_bodies[1]->maxFaceAlignment( contact.m_bodies[1]->m_model_inv * glmvec4{ L, 0.0 }, contact.m_bodies[1]->m_model);

				}
			}

			return {max_ref, max_distance, edge0, edge1};

		}


		void createFaceContact(Contact& contact, FaceQuery& fq) {
			contact.m_contact_points.clear();
			contact.m_contact_points.emplace_back( fq.m_vertex->m_positionL, fq.m_face->m_normalL );

			Face* max_face;
			real max_alignment{std::numeric_limits<real>::min()};
			for (auto face : fq.m_vertex->m_face_ptrs) {
				real alignment = -glm::dot(fq.m_face->m_normalL, face->m_normalL);
				if (alignment > max_alignment) {
					max_face = face;
					max_alignment = alignment;
				}
			}
			contact.m_reference_face = fq.m_face;
			contact.m_incident_face = max_face;
		}


		void createEdgeContact(Contact& contact, EdgeQuery &eq) {
			contact.m_contact_points.clear();

			//find closest points between the two edges

			//add contact at mid point between the points, pointint along the axis´n

			//find face on A belonging to edge0 most aligned with n
			//find face on B belonging to edg1 most aligned with n
			//add faces to the contact

		}


		void createManifold( Contact& contact ) {

			//project inc face onto reference face

			//run Sutherland Hodgman Polygon Clipping Algorithm http://www.pracspedia.com/CG/sutherlandhodgman.html
			//Find edge points on both faces
			//if distance < 2 margin then add them to the manifold

		}


		void applyImpulses() {
			//loop over all contacts

			//create mass matrix

			//
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
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity *pE4;
			VECHECKPOINTER( pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0") );
			pE4->setParam( glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f) );

			VESceneNode *e1,*eParent;
			eParent = getSceneManagerPointer()->createSceneNode("The Cube Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube0", "media/models/test/crate0", "cube.obj"));
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 1.0f, 10.0f)));
			eParent->addChild(e1);
		};
	};


}

using namespace ve;

int main() {
	bool debug = true;

	MyVulkanEngine mve(veRendererType::VE_RENDERER_TYPE_FORWARD, debug);	//enable or disable debugging (=callback, validation layers)

	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	return 0;
}


/// <summary>
/// </summary>
/// <param name=""</param>
void closestPoints( glmvec3& a, glmvec3& a1, glmvec3& b, glmvec3& b1 ) {
	glmvec3 va = a1 - a;
	glmvec3 vb = b1 - b;
	glmvec3 c = b - a;
	glmvec3 vah = va / glm::dot(va, va);
	glmvec3 d = d  - glm::dot(va, c) * vah;
	glmvec3 v = vb - glm::dot(va, vb) * vah;;
	real s = -glm::dot(d, v) / glm::dot(v,v);
	real t = glm::dot( ( (b + s * vb) - a ), vah );

}



