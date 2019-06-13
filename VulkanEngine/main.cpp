/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



namespace ve {

	///simple event listener for rotating objects
	class RotatorListener : public VEEventListener {
		VESceneNode *m_pObject = nullptr;
		float m_speed;
		glm::vec3 m_axis;
	public:
		///Constructor
		RotatorListener(std::string name, VESceneNode *pObject, float speed, glm::vec3 axis) :
			VEEventListener(name), m_pObject(pObject), m_speed(speed), m_axis(axis) {};

		void onFrameStarted(veEvent event) {
			glm::mat4 rot = glm::rotate( glm::mat4(1.0f), m_speed*(float)event.dt, m_axis );
			m_pObject->multiplyTransform(rot);
		}
	};


	///simple event listener for loading levels
	class LevelListener : public VEEventListener {
	public:
		///Constructor
		LevelListener(std::string name) : VEEventListener(name) {};

		virtual bool onKeyboard(veEvent event) {
			if (event.idata3 == GLFW_RELEASE) return false;

			if (event.idata1 == GLFW_KEY_1 && event.idata3 == GLFW_PRESS) {
				getSceneManagerPointer()->deleteScene();
				getEnginePointer()->loadLevel(1);
				return true;
			}

			if (event.idata1 == GLFW_KEY_2 && event.idata3 == GLFW_PRESS) {
				getSceneManagerPointer()->deleteScene();
				getEnginePointer()->loadLevel(2);
				return true;
			}

			if (event.idata1 == GLFW_KEY_3 && event.idata3 == GLFW_PRESS) {
				getSceneManagerPointer()->deleteScene();
				getEnginePointer()->loadLevel(3);
				return true;
			}
			return false;
		}
	};


	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	protected:

	public:
		/**
		* \brief Constructor of my engine
		* \param[in] debug Switch debuggin on or off
		*/
		MyVulkanEngine( bool debug=false) : VEEngine(debug) {};
		~MyVulkanEngine() {};

		///Register an event listener to interact with the user
		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new LevelListener("LevelListener"), { veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new VEEventListenerNuklearDebug("NuklearDebugListener"), { veEvent::VE_EVENT_DRAW_OVERLAY});
		};

		///create many cubes
		void createCubes(uint32_t n, VESceneNode *parent ) {

			float stride = 300.0f;
			static std::default_random_engine e{12345};
			static std::uniform_real_distribution<> d{ 1.0f, stride }; 

			VEMesh *pMesh;
			VECHECKPOINTER( pMesh = getSceneManagerPointer()->getMesh("models/test/crate0/cube.obj/cube") );

			VEMaterial *pMat;
			VECHECKPOINTER( pMat = getSceneManagerPointer()->getMaterial("models/test/crate0/cube.obj/cube") );

			for (uint32_t i = 0; i < n; i++) {		
				VEEntity *e2;
				VECHECKPOINTER( e2 = getSceneManagerPointer()->createEntity("The Cube" + std::to_string(i), pMesh, pMat, parent ) );

				e2->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3( d(e) - stride/2.0f, d(e)/2.0f, d(e) - stride/2.0f)));
				//e2->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 10.0f)));
				registerEventListener(	new RotatorListener("LightListener" + std::to_string(i), e2, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)), { veEvent::VE_EVENT_FRAME_STARTED } );
			}

		}

		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel( uint32_t numLevel=1) {

			VEEngine::loadLevel(numLevel );			//create standard cameras and lights

			VESceneNode *pScene;
			VECHECKPOINTER( pScene = getSceneManagerPointer()->createSceneNode("Level 1", getRoot()) );
	
			//scene models

			VESceneNode *sp1;
			VECHECKPOINTER( sp1 = getSceneManagerPointer()->createSkybox("The Sky", "models/test/sky/cloudy",
										{	"bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", 
											"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene)  );

			RotatorListener *pRot;
			VECHECKPOINTER( pRot = new RotatorListener("CubemapRotator", sp1, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)) );
			getEnginePointer()->registerEventListener(pRot);

			VESceneNode *e4;
			VECHECKPOINTER( e4 = getSceneManagerPointer()->loadModel("The Plane", "models/test", "plane_t_n_s.obj",0, pScene) );
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity *pE4;
			VECHECKPOINTER( pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0") );
			pE4->setParam( glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f) );

			VESceneNode *pointLight = getSceneManagerPointer()->getSceneNode("StandardPointLight");
			if (pointLight != nullptr) {
				VESceneNode *eL;
				VECHECKPOINTER(eL = getSceneManagerPointer()->loadModel("The Light", "models/test/sphere", "sphere.obj", 0, pointLight));
				eL->multiplyTransform(glm::scale(glm::vec3(0.02f, 0.02f, 0.02f)));

				VEEntity *pE;
				VECHECKPOINTER( pE = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Light/sphere.obj/default/Entity_0"));
				pE->m_castsShadow = false;
			}

			VESceneNode *e1;
			VECHECKPOINTER( e1 = getSceneManagerPointer()->loadModel("The Cube", "models/test/crate0", "cube.obj"));
			e1->multiplyTransform( glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 10.0f)));
			e1->multiplyTransform( glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 5.0f, 10.0f)));
			pScene->addChild(e1);

			createCubes(10, pScene);
			//VESceneNode *pSponza = m_pSceneManager->loadModel("Sponza", "models/sponza", "sponza.dae", aiProcess_FlipWindingOrder);
			//pSponza->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)));

		};
	};
}


using namespace ve;

int main() {

	bool debug = false;
#ifdef  _DEBUG
	debug = true;
#endif

	MyVulkanEngine mve(debug);	//enable or disable debugging (=callback, validation layers)

	mve.initEngine();
	mve.loadLevel();
	mve.run();

	return 0;
}

