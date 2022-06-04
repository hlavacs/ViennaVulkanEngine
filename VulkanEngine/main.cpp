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

const double c_eps = 1.0e-10;
const real c_margin = 1.0;
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

		struct Edge {
			std::pair<uint_t, uint_t>	m_vertices{};		//indices of the vertices making this edge
			glmvec3						m_edgeL{};			//edge vector in local space 
			std::vector<int_t>			m_face_indices{};	//indices of the faces this edge belongs to
		};

		struct Face {
			std::vector<std::pair<uint_t, real>> m_edge_indices{};	//edge indices and orientation factors
			glmvec3 m_normalL{};									//normal vector in local space
			real m_min{ std::numeric_limits<real>::max() };			//max in anti direction of face normal
			real m_max{ std::numeric_limits<real>::min() };			//max in direction of face normal
		};

		//struct AABB {
		//	std::array<glmvec3, 2> m_verticesL;
		//};

		class Collider {};

		class Polytope : public Collider {
			const uint_t c_high_bit = 0x01ull << (((uint_t)sizeof(uint_t) * 8ull) - 1ull); 

		protected:
			std::vector<glmvec3>	m_verticesL{};		//positions of vertices in local space
			std::vector<Edge>		m_edges{};			//list of edges
			std::vector<Face>		m_faces{};			//list of faces

		public:
			auto getVerticesL() const -> const std::vector<glmvec3>& { return m_verticesL; };
			auto getVertexL(uint_t vi) const -> glmvec3 { return getVerticesL()[vi]; }

			auto getEdges() const -> const std::vector<Edge>& { return m_edges; };
			auto getEdge(uint_t ei) const -> glmvec3 { return getEdges()[ei].m_edgeL; }

			auto getFaces() const -> const std::vector<Face>& { return m_faces; };
			auto getFace(uint_t fi) const -> const Face& { return getFaces()[fi]; };
			auto getFaceEdge(uint_t fi, uint_t ei) -> glmvec3 { m_edges[getFace(fi).m_edge_indices[ei].first].m_edgeL * getFace(fi).m_edge_indices[ei].second; }

			auto maxFaceAlignment( glmvec3 dirL, glmmat4 BtoA ) -> std::pair<uint_t,real> {
				real max_abs_face_alignment{ std::numeric_limits<real>::min() };
				uint_t maxfi = 0;
				for (uint_t fi = 0;  auto & face : getFaces()) {
					if (real abs_face_alignment = abs(glm::dot(dirL, face.m_normalL)) > max_abs_face_alignment) {
						max_abs_face_alignment = abs_face_alignment;
						maxfi = fi;
					}
					++fi;
				}
				return { maxfi, abs(glm::dot(BtoA * glmvec4{ dirL, 0.0 }, BtoA * glmvec4{ getFaces()[maxfi].m_normalL, 1.0 } )) };
			}

			Polytope(const std::vector<glmvec3>&& vertices, const std::vector<std::pair<uint_t, uint_t>>&& edgeindices, const std::vector < std::vector<std::pair<uint_t,real>> >&& face_edge_indices)
				: Collider{}, m_verticesL{ vertices }, m_edges{}, m_faces{} {

				for (auto& edgepair : edgeindices) {	//compute edges from indices
					m_edges.emplace_back( edgepair, m_verticesL[edgepair.second] - m_verticesL[edgepair.first] );
				}

				for (auto& fi : face_edge_indices) {	//compute faces from edge indices belonging to this face
					Face face{ fi };					//new face

					if (face_edge_indices.size() >= 2) {	//compute face normal
						face.m_normalL = glm::cross(m_edges[fi[0].first].m_edgeL * fi[0].second, m_edges[fi[1].first].m_edgeL * fi[1].second);
					}

					for (auto& vert : m_verticesL) {				//AABB along the face normals in local space
						real dp = glm::dot(face.m_normalL, vert);
						face.m_min = std::min(face.m_min, dp);
						face.m_max = std::max(face.m_max, dp);
					}

					for (auto& edge : fi ) {	//record that this face belongs to a specific edge
						m_edges[edge.first].m_face_indices.push_back( m_faces.size() );
					}

					m_faces.push_back(face);	//add new face to face vector
				}
			};
		};

		Polytope g_cube{
			{ {-0.5,-0.5,-0.5}, {-0.5,-0.5,0.5}, {-0.5,0.5,0.5}, {-0.5,0.5,-0.5},{0.5,-0.5,-0.5}, {0.5,-0.5,0.5}, {0.5,0.5,0.5}, {0.5,0.5,-0.5} },
			{},
			{}
		};

		struct Body {
			void*		m_owner = nullptr;				//pointer to owner of this body
			Polytope&	m_polytope;						//geometric shape
			real		m_inv_mass{ 1 };				//1 over mass
			glmvec3		m_inertia{ 1,1,1 };				//inertia tensor diagonal

			glmvec3		m_scaleL{ 1,1,1 };				//scale factor in local space
			glmvec3		m_positionW{ 0, 0, 0 };			//current position at time slot in world space
			glmquat		m_orientationLW{ 1, 0, 0, 0 };	//current orientation at time slot Local -> World
			glmvec3		m_linear_velocityW{ 0,0,0 };		//linear velocity at time slot in world space
			glmvec3		m_angular_velocityW{ 0,0,0 };	//angular velocity at time slot in world space

			std::unordered_map<uint64_t, Force> m_forces;//forces acting on this body

			glmmat4		m_model;						//model matrix at time slots
			glmmat4		m_model_inv;					//model inverse matrix at time slots

			std::function<void(double, std::shared_ptr<Body>)>* m_on_move = nullptr; //called if the body moves

			bool stepPosition(double dt, glmvec3& pos) {
				if (abs(glm::dot(m_linear_velocityW, m_linear_velocityW)) < c_eps * c_eps) return false;
				pos = m_positionW + m_linear_velocityW * (real)dt;
				return true;
			};

			bool stepOrientation(double dt, glmquat quat) {
				real len = glm::length(m_angular_velocityW);
				if (abs(len) < c_eps) return false;
				quat = glm::rotate(m_orientationLW, len * (real)dt, m_angular_velocityW * 1.0 / len);
				return true;
			};

			static glmmat4 computeModel( glmvec3 pos, glmquat orient, glmvec3 scale ) { 
				return glm::translate(glmmat4{ 1.0 }, pos) * glm::mat4_cast(orient) * glm::scale(glmmat4{ 1.0 }, scale);
			};

			std::pair<int_t, real> support(glmvec3 dirL, glmmat4 BtoA = glmmat4{ 1.0 }) {
				std::pair<int_t, real> ret{ -1, std::numeric_limits<real>::min() };
				for (int_t i = 0; auto & vert : m_polytope.getVerticesL()) {
					real dp = glm::dot(dirL, vert);
					if (dp > ret.second) { ret.first = i;  ret.second = dp; }
					++i;
				}
				if (ret.first >= 0) ret.second = glm::dot(BtoA * glmvec4{ dirL, 0.0 }, BtoA * glmvec4{ m_polytope.getVertexL(ret.first), 1.0});
				return ret;
			};
		};

		struct Contact {
			struct ContactPoint {
				glmvec3 m_positionW;
				glmvec3 m_normalW;
			};

			std::array<std::shared_ptr<Body>, 2> m_bodies{};
			uint64_t					m_last_loop{ std::numeric_limits<uint64_t>::max() };
			real						m_separation{ 0.0 };
			bool						m_face_vertex{ true };	//true...face-vertex  false...edge-edge
			std::array<int_t, 2>		m_indices{};			//indices of faces or edges involved into the contact
			std::vector<ContactPoint>	m_contact_points{};
		};

	protected:

		uint64_t m_loop{ 0L };
		double m_last_time{ 0.0 };					//last time the sim was interpolated
		double m_last_slot{ 0.0 };					//last time the sim was calculated
		const double m_delta_slot{ 1.0 / 60.0 };	//sim frequency
		double m_next_slot{ m_delta_slot };			//next time for simulation

		using body_map = std::unordered_map<void*, std::shared_ptr<Body>>;
		body_map m_bodies;									//main container of all bodies
		
		const double c_width{5};									//grid cell width (m)
		std::unordered_map< intpair_t, body_map > m_grid;		//broadphase grid

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

				for (auto& c : m_bodies) {
					if( c.second->stepPosition(m_delta_slot, c.second->m_positionW) || 
						c.second->stepOrientation(m_delta_slot, c.second->m_orientationLW)) {					
						c.second->m_model = Body::computeModel(c.second->m_positionW, c.second->m_orientationLW, c.second->m_scaleL);
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
		void makePairs(const intpair_t& cell, const intpair_t&& neigh) {
			if( !m_grid.count(cell) || !m_grid.count(neigh) ) return;
			for (auto& coll : m_grid.at(cell)) {
				for (auto& neigh : m_grid.at(neigh)) {
					if (coll.second->m_owner != neigh.second->m_owner) {
						auto it = m_contacts.find({ coll.second->m_owner, neigh.second->m_owner });
						if (it != m_contacts.end()) it->second.m_last_loop = m_loop;
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
				for (auto& p : c_pairs) {	//create pairs of neighborig cells and make body pairs.
					makePairs(cell.first, { cell.first.first + p.first, cell.first.second + p.second });
				} 
			}
		}

		/// <summary>
		/// For each pair coming from the broadphase, test whether the bodies touch each other. If so then
		/// compute the contact manifold.
		/// </summary>
		void narrowPhase() {
			for( auto it = std::begin(m_contacts); it!=std::end(m_contacts); ) {
				if (it->second.m_last_loop == m_loop ) {
					SAT(it->second);
					++it;
				}
			}
		}



		/// <summary>
		/// Perform SAT test for two bodies. If they overlap then compute the contact manifold.
		/// </summary>
		/// <param name="contact"></param>
		void SAT(Contact& contact) {
			glmmat4 AtoB = contact.m_bodies[1]->m_model_inv * contact.m_bodies[0]->m_model; //transform to bring space A to space B
			glmmat4 BtoA = contact.m_bodies[0]->m_model_inv * contact.m_bodies[1]->m_model; //transform to bring space B to space A

			std::array<real, 3> separations;
			if ((separations[0] = queryFaceDirections(contact, 0, 1, AtoB, BtoA ))	> c_2margin) return;
			if ((separations[1] = queryFaceDirections(contact, 1, 0, BtoA, AtoB))	> c_2margin) return;
			if ((separations[2] = queryEdgeDirections(contact, AtoB, BtoA))			> c_2margin) return;

			if (separations[0] > separations[2] && separations[1] > separations[2]) createFaceContact();
			else createEdgeContact();


		}

		real queryFaceDirections(Contact& contact, int_t a, int_t b, glmmat4& AtoB, glmmat4& BtoA) {
			for (auto& face : contact.m_bodies[a]->m_polytope.getFaces()) {
				auto [vi_minB, minB] = contact.m_bodies[b]->support(glmmat3{ AtoB } * face.m_normalL * -1.0, BtoA);
				auto [vi_maxB, maxB] = contact.m_bodies[b]->support(glmmat3{ AtoB } * face.m_normalL, BtoA);
				if (face.m_max < minB) return minB - face.m_max;
				if (maxB < face.m_min) return face.m_min - maxB;
				if (face.m_max < maxB) return minB - face.m_max;
				return face.m_min - maxB;
			}

			return 0.0;
		}

		/// <summary>
		/// Loop over all edge pairs. Create the cross product vector L. 
		/// Choose a reference and incident body.
		/// Reference body is the one having the face that is most aligned with L (abs value). The other is the
		/// incident body. Return overlap.
		/// </summary>
		/// <param name="contact">The contact pair.</param>
		/// <param name="AtoB">Transform from object space A to B.</param>
		/// <param name="BtoA">Transform from object space B to A.</param>
		/// <returns></returns>
		real queryEdgeDirections(Contact& contact, glmmat4& AtoB, glmmat4& BtoA) {
			for (auto& edgeA : contact.m_bodies[0]->m_polytope.getEdges()) {
				for (auto& edgeB : contact.m_bodies[1]->m_polytope.getEdges()) {
					auto L = glm::cross(edgeA.m_edgeL, glmmat3{ BtoA } * edgeB.m_edgeL);
					uint_t a = 0, b = 1; auto myAtoB = AtoB, myBtoA = BtoA;

					auto [fi1, max_alignment1] = contact.m_bodies[0]->m_polytope.maxFaceAlignment( contact.m_bodies[0]->m_model_inv * glmvec4{ L, 0.0 }, contact.m_bodies[0]->m_model);
					auto [fi2, max_alignment2] = contact.m_bodies[1]->m_polytope.maxFaceAlignment( contact.m_bodies[1]->m_model_inv * glmvec4{ L, 0.0 }, contact.m_bodies[1]->m_model);

					if (max_alignment2 > max_alignment2) {
						a = 1; b = 0; AtoB = BtoA, myBtoA = AtoB;
					}
										
					auto [vminA, minA] = contact.m_bodies[a]->support( L * -1.0, glmmat4{1.0});
					auto [vmaxA, maxA] = contact.m_bodies[a]->support( L, glmmat4{1.0});

					auto [vminB, minB] = contact.m_bodies[b]->support(glmmat3{ myAtoB } * L * -1.0, myBtoA);
					auto [vmaxB, maxB] = contact.m_bodies[b]->support(glmmat3{ myAtoB } * L, myBtoA);
				}
			}

			return 0.0;

		}

		void createFaceContact() {

		}

		void createEdgeContact() {

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
