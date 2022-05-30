/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"

//#define GLM_FORCE_PRECISION_MEDIUMP_INT
#define GLM_FORCE_PRECISION_HIGHP_FLOAT
#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"

using real = double;
using int_t = int64_t;
using uint_t = uint64_t;


template <typename T> 
inline void hash_combine(std::size_t& seed, T const& v) {
	seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

using intpair_t = std::pair<int_t, int_t>;

namespace std {
	template <>
	struct hash<intpair_t> {
		std::size_t operator()(const intpair_t& p) {
			size_t seed = std::hash<int_t>()(p.first);
			hash_combine(seed, p.second);
			return seed;
		}
	};
}

namespace ve {


	static std::default_random_engine e{ 12345 };					//Random numbers
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//Random numbers


	struct Plane {
		std::vector<uint_t> m_vertices;
		glm::vec3 m_normal;
	};

	struct Polytope {
		std::vector<glm::vec3> m_vertices;
		std::vector<Plane> m_planes;
		std::vector<std::pair<uint_t,uint_t>> m_edges;
		glm::vec3 m_scale{ 1,1,1 };

		Polytope(	std::vector<glm::vec3>&& v, std::vector<Plane>&& p,
					std::vector<std::pair<uint_t, uint_t>>&& e, glm::vec3&& scale) 
						: m_vertices(v), m_planes(p), m_edges(e), m_scale(scale) {};
	};

	Polytope g_cube(
		{ {-0.5,-0.5,-0.5}, {-0.5,-0.5,0.5}, {-0.5,0.5,0.5}, {-0.5,0.5,-0.5},{0.5,-0.5,-0.5}, {0.5,-0.5,0.5}, {0.5,0.5,0.5}, {0.5,0.5,-0.5} },
		{}, 
		{},
		{1,1,1});


	struct Collider {
		void* m_owner = nullptr;
		Polytope& m_polytope;
		glm::vec3 m_position{ 0, 0, 0 };
		glm::quat m_orientation{1, 0, 0, 0};
		glm::vec3 m_inertia{1,1,1};
		glm::vec3 m_linear_velocity{ 0,0,0 };
		glm::vec3 m_angular_velocity{0,0,0};

		std::function<void(Collider*, void*)>* m_on_move = nullptr;

		Collider(void* owner, Polytope& pol, glm::vec3 pos, glm::quat o, std::function<void(Collider*,void*)>* on_move) :
			m_owner(owner), m_polytope(pol), m_position(pos), m_orientation(o), m_on_move(on_move) {};
	};


	struct Contact {
		Collider& m_collider_A;
		Collider& m_collider_B;

	};


	//
	// Überprüfen, ob die Kamera die Kiste berührt
	//
	class EventListenerPhysics : public VEEventListener {
	protected:

		double m_last_time = 0.0;					//last time the sim was interpolated
		double m_last_slot = 0.0;					//last time the sim was calculated
		const double m_delta_slot = 1.0 / 60.0;		//sim frequency
		double m_next_slot = m_delta_slot;			//next time for simulation

		using collider_map = std::unordered_map<void*, std::shared_ptr<Collider>>;
		collider_map m_collider;					//main container of all colliders
		
		const double c_width = 5;					//grid cell width (m)
		std::unordered_map< intpair_t, collider_map > m_grid; //broadphase grid

		virtual void onFrameStarted(veEvent event) {
			//getSceneManagerPointer()->getSceneNode("The Cube Parent")->setPosition(glm::vec3(d(e), 1.0f, d(e)));
			//glm::vec3 positionCube   = getSceneManagerPointer()->getSceneNode("The Cube Parent")->getPosition();
			//glm::vec3 positionCamera = getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getPosition();
			//VESceneNode *eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
			//eParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));
			//getSceneManagerPointer()->deleteSceneNodeAndChildren("The Cube"+ std::to_string(cubeid));
			//VECHECKPOINTER(getSceneManagerPointer()->loadModel("The Cube"+ std::to_string(++cubeid)  , "media/models/test/crate0", "cube.obj", 0, eParent) );

			double current_time = m_last_time + event.dt;
			while (current_time > m_next_slot) {
				broadPhase();
				narrowPhase();
				update();
				m_last_slot = m_next_slot;
				m_next_slot += m_delta_slot;
			}
			predict();
			m_last_time = current_time;
		};


		void make_pairs(const intpair_t& cell, const intpair_t& neigh) {
			//auto& cel = m_grid.at(neigh);

			//for (auto& c : m_grid[cell]) {
				//for (auto& c : m_grid.at(neigh)) {

				//}
			//}
		}

		const std::array<intpair_t, 6> c_pairs{ { {0,0}, {1,1}, {1,0}, {-1,-1}, {0,-1}, {1,-1} } };
		void broadPhase() {
			for (auto& cell : m_grid) {
				for (auto& p : c_pairs) {
					intpair_t neigh{ cell.first.first + p.first, cell.first.second + p.second };
					make_pairs(cell.first, neigh);
				}
			}
		}

		void narrowPhase() {

		}

		void update() {

		}

		void predict() {

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
