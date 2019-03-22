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
		VEEntity *m_pEntity = nullptr;
		float m_speed;
		glm::vec3 m_axis;
	public:
		///Constructor
		RotatorListener(std::string name, VEEntity *pEntity, float speed, glm::vec3 axis) :
			VEEventListener(name), m_pEntity(pEntity), m_speed(speed), m_axis(axis) {};

		void onFrameStarted(veEvent event) {
			glm::mat4 rot = glm::rotate( glm::mat4(1.0f), m_speed*(float)event.dt, m_axis );
			m_pEntity->multiplyTransform(rot);
		}
	};



	///simple event listener for managing light movement
	class LightListener : public VEEventListener {
	public:
		///Constructor
		LightListener( std::string name) : VEEventListener(name) {};

		bool onKeyboard(veEvent event) {
			if ( event.idata3 == GLFW_RELEASE) return false;

			VELight *pLight = getSceneManagerPointer()->getLights()[0];		//first light

			float speed = 10.0f * (float)event.dt;

			switch (event.idata1) {
			case GLFW_KEY_Y:		//Z key on German keyboard!
				pLight->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * glm::vec3(0.0f, -1.0f, 0.0f)));
				break;
			case GLFW_KEY_I:
				pLight->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * glm::vec3(0.0f, 1.0f, 0.0f)));
				break;
			case GLFW_KEY_U:
				pLight->multiplyTransform( glm::translate(glm::mat4(1.0f), speed * glm::vec3(0.0f, 0.0f, 1.0f)));
				break;
			case GLFW_KEY_J:
				pLight->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * glm::vec3(0.0f, 0.0f, -1.0f)));
				break;
			case GLFW_KEY_H:
				pLight->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * glm::vec3(-1.0f, 0.0f, 0.0f)));
				break;
			case GLFW_KEY_K:
				pLight->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * glm::vec3(1.0f, 0.0f, 0.0f)));
				break;
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
			registerEventListener( new LightListener("LightListener"));
		};

		///Load the first level into the game engine
		//The engine uses Y-UP, Left-handed
		void loadLevel() {

			//VEEntity *cubemap = getSceneManagerPointer()->createCubemap("The Cubemap", "models/textures", "grasscube1024.dds");
			
			//  ft bk up dn rt lf
			VEEntity *cubemap = getSceneManagerPointer()->createCubemap("The Cubemap", "models/textures/cloudy", 
			{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", "bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" });
			//{ "graycloud_ft.jpg", "graycloud_bk.jpg", "graycloud_up.jpg", "graycloud_dn.jpg", "graycloud_rt.jpg", "graycloud_lf.jpg" });
			cubemap->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -700.0f, 0.0f)));
			RotatorListener *pRot = new RotatorListener("CubemapRotator", cubemap, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
			getEnginePointer()->registerEventListener(pRot);
			
			VEEntity *eSLight = getSceneManagerPointer()->getEntity("StandardLight");
			VEEntity *eL = m_pSceneManager->loadModel("The Light", "models/test/sphere", "sphere.obj", 0 , eSLight);
			eL->multiplyTransform(glm::scale(glm::vec3(0.02f,0.02f,0.02f)));

			VEEntity *e4 = m_pSceneManager->loadModel("The Plane", "models/test", "plane_t_n_s.obj");
			glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f));
			e4->setTransform(scale);

			VEEntity *e1 = m_pSceneManager->loadModel("The Cube",  "models/test/crate0", "cube.obj");
			e1->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

			VEEntity *e1b = m_pSceneManager->loadModel("The Cube b", "models/test/crate0", "cube.obj");
			e1b->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 10.0f)));

			VEEntity *e2 = m_pSceneManager->loadModel("The Cube2", "models/test/crate1", "cube.obj");
			e2->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 10.0f, 10.0f)));
			VEEntity *e3 = m_pSceneManager->loadModel("The Cube3", "models/test/crate2", "cube.obj");
			e3->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 10.0f, 10.0f)));
			
			VEEntity *e6 = m_pSceneManager->loadModel("The Cube6", "models/test", "cube1.obj");
			e6->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 1.0f)));

			VEEntity *e7 = m_pSceneManager->loadModel("The Cube6 b", "models/test", "cube1.obj");
			e7->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 5.0f)));

			//VEEntity *hand = m_pSceneManager->loadModel("The Hand", "models/hand", "hand.fbx", aiProcess_FlipUVs | aiProcess_FlipWindingOrder );
			//hand->multiplyTransform( glm::rotate( glm::mat4(1.0f), (float)-M_PI/2.0f, glm::vec3(0.0f, 1.0f, 0.0f) ) );
			//hand->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 5.0f)));
			//getSceneManagerPointer()->deleteEntityAndSubentities("The Hand/RootNode/Cube.005");

			std::default_random_engine generator;
			std::uniform_real_distribution<float> distribution(-50.0, 50.0);

			for (uint32_t i = 0; i < 20; i++) {
				std::string name = "The Cube " + std::to_string(i);

				VEEntity *p = new VEEntity( name + "-parent");
				m_pSceneManager->addEntity(p);
				p->setTransform( 
					glm::translate(glm::mat4(1.0f), glm::vec3(2.0f*distribution(generator), 55.0f + distribution(generator), 2.0f*distribution(generator))));

				VEEntity *e = m_pSceneManager->createEntity( name, m_pSceneManager->getMesh("models/test/crate0/cube.obj/cube"), m_pSceneManager->getMaterial("models/test/crate0/cube.obj/cube"), glm::mat4(1.0f), p );

				RotatorListener *pRot = new RotatorListener(name, e, 1.1f, glm::vec3(0.0f, 1.0f, 0.0f));
				getEnginePointer()->registerEventListener(pRot);
			}
			
		};
	};
}


using namespace ve;

int main() {

	MyVulkanEngine mve(true);	//enable or disable debugging (=callback, valication layers)

	try {
		mve.initEngine();
		mve.loadLevel();
		mve.run();
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << " (press return)" << std::endl;
		char dummy = getchar();
		return EXIT_FAILURE;
	}

	return 0;
}

