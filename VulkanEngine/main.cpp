#include "VEInclude.h"

namespace ve {

	uint32_t g_score = 0;
	uint8_t g_state = 0; // 0 - searching, 1 - picked up

	static const glm::vec3 g_goalPosition = glm::vec3(-5.0f, 1.3f, 5.0f);
	static const glm::vec3 g_carryPosition = glm::vec3(0.0f, 0.0f, 2.0f);
	static const glm::vec3 g_farPosition = glm::vec3(0.0f, 0.0f, 100.0f);

	static std::default_random_engine e{ 12345 };
	static std::uniform_real_distribution<> d{ -4.0f, 4.0f };



	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {

			VEEntity* goalParent;
			VECHECKPOINTER(goalParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Goal Parent"));
			VEEntity* caseParent;
			VECHECKPOINTER(caseParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Case Parent"));
			VEEntity* carryParent;
			VECHECKPOINTER(carryParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Carry Parent"));
			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
			float distance;
			switch (g_state)
			{
			case 0: //searching
				distance = glm::distance(caseParent->getPosition(), characterParent->getPosition());
				if (distance < 1.5f) {
					caseParent->setPosition(g_farPosition);
					carryParent->setPosition(g_carryPosition);
					goalParent->setPosition(g_goalPosition);
					g_state = 1;
				}
				break;
			case 1: //picked up
				distance = glm::distance(goalParent->getPosition(), characterParent->getPosition());
				if (distance < 3.0f) {
					caseParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));
					carryParent->setPosition(g_farPosition);
					// goalParent->setPosition(g_farPosition);
					g_state = 0;
					g_score++;
				}
				break;

			default:
				break;
			}
		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	class EventListenerControls : public VEEventListener {
	protected:
		virtual bool onKeyboard(veEvent event) {
			if (event.idata1 == GLFW_KEY_ESCAPE) {				//ESC pressed - end the engine
				getEnginePointer()->end();
				return true;
			}

			if (event.idata3 == GLFW_RELEASE) return false;

			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));


			glm::vec4 rot4Character = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			glm::vec4 dir4 = glm::normalize(characterParent->getTransform() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
			glm::vec3 dir = glm::vec3(dir4.x, dir4.y, dir4.z);

			float rotSpeed = 2.0f * event.dt;
			float speed = 5.0f * event.dt;


			VECamera* pCamera = getSceneManagerPointer()->getCamera();
			VESceneNode* pParent = pCamera->getParent();

			switch (event.idata1) {
			case GLFW_KEY_A:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), -rotSpeed, glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
				break;
			case GLFW_KEY_D:
				characterParent->setTransform(glm::rotate(characterParent->getTransform(), rotSpeed, glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
				break;
			case GLFW_KEY_W:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), speed * dir));
				break;
			case GLFW_KEY_S:
				characterParent->multiplyTransform(glm::translate(glm::mat4(1.0f), -speed * dir));
				break;

			default:
				return false;
			};
			
			return true;
		};

	public:
		EventListenerControls(std::string name) : VEEventListener(name) { };
		virtual ~EventListenerControls() {};
	};

	class EventListenerGUI : public VEEventListener {
	protected:

		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear* pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context* ctx = pSubrender->getContext();

			if (nk_begin(ctx, "", nk_rect(600, 0, 200, 170), NK_WINDOW_BORDER)) {
				char outbuffer[100];
				nk_layout_row_dynamic(ctx, 45, 1);
				sprintf(outbuffer, "Score: %03d", g_score);
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);

				nk_layout_row_dynamic(ctx, 45, 1);
				switch (g_state)
				{
				case 0:
					sprintf(outbuffer, "Find the box!");
					break;
				case 1:
					sprintf(outbuffer, "Bring it back!");
					break;
				default:
					break;
				}
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);
			}

			nk_end(ctx);
		}

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUI(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};


	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(bool debug = false) : VEEngine(debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user

		virtual void registerEventListeners() { 
			std::cout << "loading listeners" << std::endl;

			registerEventListener(new EventListenerCollision("Collision"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(new EventListenerControls("Controls"), { veEvent::VE_EVENT_KEYBOARD });
			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY });
		};

		void loadCameraAndLights() {
			VEEntity* characterParent;
			VECHECKPOINTER(characterParent = (VEEntity*)getSceneManagerPointer()->getSceneNode("Character Parent"));
			VESceneNode* cameraParent = getSceneManagerPointer()->createSceneNode("StandardCameraParent", getRoot(),
				glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, -10.0f)));

			//camera can only do yaw (parent y-axis) and pitch (local x-axis) rotations
			VkExtent2D extent = getWindowPointer()->getExtent();
			VECameraProjective* camera = (VECameraProjective*)getSceneManagerPointer()->createCamera("StandardCamera", VECamera::VE_CAMERA_TYPE_PROJECTIVE, cameraParent);
			camera->m_nearPlane = 0.1f;
			camera->m_farPlane = 500.1f;
			camera->m_aspectRatio = extent.width / (float)extent.height;
			camera->m_fov = 45.0f;
			camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f));
			getSceneManagerPointer()->setCamera(camera);

			VELight* light4 = (VESpotLight*)getSceneManagerPointer()->createLight("StandardAmbientLight", VELight::VE_LIGHT_TYPE_AMBIENT, camera);
			light4->m_col_ambient = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);

			//use one light source
			VELight* light1 = (VEDirectionalLight*)getSceneManagerPointer()->createLight("StandardDirLight", VELight::VE_LIGHT_TYPE_DIRECTIONAL, getRoot());     //new VEDirectionalLight("StandardDirLight");
			light1->lookAt(glm::vec3(0.0f, 20.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			light1->m_col_diffuse = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
			light1->m_col_specular = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
		}

		void loadScene() {


			VESceneNode* pScene;
			VECHECKPOINTER(pScene = getSceneManagerPointer()->createSceneNode("My Scene", getRoot()));

			//scene models

			VESceneNode* sp1;
			VECHECKPOINTER(sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
				{ "bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg",
					"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene));

			VESceneNode* e4;
			VECHECKPOINTER(e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj", 0, pScene));
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity* pE4;
			VECHECKPOINTER(pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0"));
			pE4->setParam(glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f));

			VESceneNode* e1, * eParent;
			eParent = getSceneManagerPointer()->createSceneNode("Character Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("Character", "media/models/free/", "al.obj", aiProcess_FlipWindingOrder));
			eParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.5f,0.5f,0.5f)));
			eParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.2f, 0.0f)));
			glm::vec4 rot4Character = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			eParent->setTransform(glm::rotate(eParent->getTransform(), glm::pi<float>(), glm::vec3(rot4Character.x, rot4Character.y, rot4Character.z)));
			
			VESceneNode* caseParent, * caseObj;
			caseParent = getSceneManagerPointer()->createSceneNode("Case Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(caseObj = getSceneManagerPointer()->loadModel("Case", "media/models/test/crate0/", "cube.obj"));
			caseParent->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 1.0f, 2.0f)));
			caseParent->addChild(caseObj);

			VESceneNode* carryParent, * carryObj;
			carryParent = getSceneManagerPointer()->createSceneNode("Carry Parent", eParent, glm::mat4(1.0));
			VECHECKPOINTER(carryObj = getSceneManagerPointer()->loadModel("Carry", "media/models/test/crate0/", "cube.obj"));
			carryParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f)));
			carryParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_farPosition));
			carryParent->addChild(carryObj);

			VESceneNode* goalParent, * goalObj;
			goalParent = getSceneManagerPointer()->createSceneNode("Goal Parent", pScene, glm::mat4(1.0));
			VECHECKPOINTER(goalObj = getSceneManagerPointer()->loadModel("Goal", "media/models/free/", "skyscraper.obj", aiProcess_FlipWindingOrder);
			goalParent->multiplyTransform(glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f)));
			goalParent->multiplyTransform(glm::translate(glm::mat4(1.0f), g_goalPosition));
			goalParent->setTransform(glm::rotate(goalParent->getTransform(), -0.25f*glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));
			goalParent->addChild(goalObj);

			VESceneNode* cameraParent = getSceneManagerPointer()->getSceneNode("StandardCameraParent");
			eParent->addChild(e1);
		}
		 
		///Load the first level into the game engine
		///The engine uses Y-UP, Left-handed
		virtual void loadLevel(uint32_t numLevel = 1) {

			loadScene();
			loadCameraAndLights();
			registerEventListeners();

		};
	};


}

using namespace ve;

int main() {

	bool debug = true;

	MyVulkanEngine mve(debug);	//enable or disable debugging (=callback, validation layers)

	mve.initEngine();
	mve.loadLevel(1);
	mve.run();
	std::cout << "Game finished!" << std::endl << "Score: " << g_score << std::endl;

	return 0;
}
