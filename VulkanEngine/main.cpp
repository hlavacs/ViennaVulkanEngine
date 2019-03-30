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
		VEMovableObject *m_pObject = nullptr;
		float m_speed;
		glm::vec3 m_axis;
	public:
		///Constructor
		RotatorListener(std::string name, VEMovableObject *pObject, float speed, glm::vec3 axis) :
			VEEventListener(name), m_pObject(pObject), m_speed(speed), m_axis(axis) {};

		void onFrameStarted(veEvent event) {
			glm::mat4 rot = glm::rotate( glm::mat4(1.0f), m_speed*(float)event.dt, m_axis );
			m_pObject->multiplyTransform(rot);
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

			VEMovableObject *e4 = m_pSceneManager->loadModel("The Plane", "models/test", "plane_t_n_s.obj");
			e4->setParam( glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f) );
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			//VEEntity *cubemap = getSceneManagerPointer()->createCubemap("The Cubemap", "models/test/sky", "grasscube1024.dds");
			
			//  ft bk up dn rt lf
			//VEEntity *cubemap = getSceneManagerPointer()->createCubemap("The Cubemap", "models/test/sky/cloudy", 
			//{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", "bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" });
			//{ "graycloud_ft.jpg", "graycloud_bk.jpg", "graycloud_up.jpg", "graycloud_dn.jpg", "graycloud_rt.jpg", "graycloud_lf.jpg" });
			//cubemap->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -700.0f, 0.0f)));
			//RotatorListener *pRot = new RotatorListener("CubemapRotator", cubemap, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
			//getEnginePointer()->registerEventListener(pRot);

			VEMovableObject *sp1 = m_pSceneManager->createSkybox("The Sky", "models/test/sky/cloudy",
			{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", "bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" });
			RotatorListener *pRot = new RotatorListener("CubemapRotator", sp1, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
			getEnginePointer()->registerEventListener(pRot);


			VELight *eSLight = (VELight*)getSceneManagerPointer()->getMovableObject("StandardLight");
			VEMovableObject *eL = m_pSceneManager->loadModel("The Light", "models/test/sphere", "sphere.obj", 0 , eSLight);
			eL->multiplyTransform(glm::scale(glm::vec3(0.02f,0.02f,0.02f)));

			VEMovableObject *e1 = m_pSceneManager->loadModel("The Cube",  "models/test/crate0", "cube.obj");
			e1->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 1.0f, 1.0f)));
			e1->multiplyTransform( glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 10.0f)));

			VEMovableObject *e1b = m_pSceneManager->loadModel("The Cube b", "models/test/crate0", "cube.obj");
			e1b->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 10.0f)));

			VEMovableObject *e2 = m_pSceneManager->loadModel("The Cube2", "models/test/crate1", "cube.obj");
			e2->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 10.0f, 10.0f)));
			VEMovableObject *e3 = m_pSceneManager->loadModel("The Cube3", "models/test/crate2", "cube.obj");
			e3->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 10.0f, 10.0f)));
			
			VEMovableObject *e6 = m_pSceneManager->loadModel("The Cube6", "models/test", "cube1.obj");
			e6->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 1.0f)));

			VEMovableObject *e7 = m_pSceneManager->loadModel("The Cube6 b", "models/test", "cube1.obj");
			e7->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f)) * glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 10.0f, 5.0f)));

			std::default_random_engine generator;
			std::uniform_real_distribution<float> distribution(-10.0, 10.0);

			for (uint32_t i = 0; i < 20; i++) {
				std::string name = "The Cube " + std::to_string(i);

				VEMovableObject *pMO = m_pSceneManager->createMovableObject( name + "-parent");
				pMO->setTransform( 
					glm::translate(glm::mat4(1.0f), glm::vec3(2.0f*distribution(generator), 55.0f + distribution(generator), 2.0f*distribution(generator))));

				VEEntity *e = m_pSceneManager->createEntity( name, m_pSceneManager->getMesh("models/test/crate0/cube.obj/cube"), m_pSceneManager->getMaterial("models/test/crate0/cube.obj/cube"), glm::mat4(1.0f), pMO );

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

