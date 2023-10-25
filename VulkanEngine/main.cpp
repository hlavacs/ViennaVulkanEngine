/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "VHVideoEncoder.h"



namespace ve {


	uint32_t g_score = 0;				//derzeitiger Punktestand
	double g_time = 30.0;				//zeit die noch übrig ist
	bool g_gameLost = false;			//true... das Spiel wurde verloren
	bool g_restart = false;			//true...das Spiel soll neu gestartet werden
	int g_writeFrames = false;			//true..write frames to disk

	//
	//Zeichne das GUI
	//
	class EventListenerGUI : public VEEventListener {
	protected:
		
		virtual void onDrawOverlay(veEvent event) {
			VESubrender_Nuklear * pSubrender = (VESubrender_Nuklear*)getEnginePointer()->getRenderer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context * ctx = pSubrender->getContext();

			if (!g_gameLost) {
				if (nk_begin(ctx, "", nk_rect(0, 0, 200, 200), NK_WINDOW_BORDER )) {
					char outbuffer[100];
					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Score: %03d", g_score);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);

					nk_layout_row_dynamic(ctx, 45, 1);
					sprintf(outbuffer, "Time: %004.1lf", g_time);
					nk_label(ctx, outbuffer, NK_TEXT_LEFT);
				}
			}
			else {
				if (nk_begin(ctx, "", nk_rect(500, 500, 200, 200), NK_WINDOW_BORDER )) {
					nk_layout_row_dynamic(ctx, 45, 1);
					nk_label(ctx, "Game Over", NK_TEXT_LEFT);
					if (nk_button_label(ctx, "Restart")) {
						g_restart = true;
					}
				}

			};
			static double fps = 0.0;
			if (event.dt > 0)
				fps = 0.05 / event.dt + 0.95 * fps;
			std::stringstream str;
			str << std::setprecision(5);
			str << "FPS " << fps;
			nk_layout_row_dynamic(ctx, 30, 1);
			nk_label(ctx, str.str().c_str(), NK_TEXT_LEFT);

			nk_layout_row_dynamic(ctx, 45, 1);
			nk_checkbox_label(ctx, "Write Frames", &g_writeFrames);

			nk_end(ctx);
		}

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUI(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};


	static std::default_random_engine e{ 12345 };					//Für Zufallszahlen
	static std::uniform_real_distribution<> d{ -10.0f, 10.0f };		//Für Zufallszahlen

	//
	// Überprüfen, ob die Kamera die Kiste berührt
	//
	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {
			static uint32_t cubeid = 0;

			if (g_restart) {
				g_gameLost = false;
				g_restart = false;
				g_time = 30;
				g_score = 0;
				getSceneManagerPointer()->getSceneNode("The Cube Parent")->setPosition(glm::vec3(d(e), 1.0f, d(e)));
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/ophelia.wav", true);
				return;
			}
			if (g_gameLost) return;

			glm::vec3 positionCube   = getSceneManagerPointer()->getSceneNode("The Cube Parent")->getPosition();
			glm::vec3 positionCamera = getSceneManagerPointer()->getSceneNode("StandardCameraParent")->getPosition();

			float distance = glm::length(positionCube - positionCamera);
			if (distance < 1) {
				g_score++;
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/explosion.wav", false);
				if (g_score % 10 == 0) {
					g_time = 30;
					getEnginePointer()->m_irrklangEngine->play2D("media/sounds/bell.wav", false);
				}

				VESceneNode *eParent = getSceneManagerPointer()->getSceneNode("The Cube Parent");
				eParent->setPosition(glm::vec3(d(e), 1.0f, d(e)));

				getSceneManagerPointer()->deleteSceneNodeAndChildren("The Cube"+ std::to_string(cubeid));
				VECHECKPOINTER(getSceneManagerPointer()->loadModel("The Cube"+ std::to_string(++cubeid)  , "media/models/test/crate0", "cube.obj", 0, eParent) );
			}

			g_time -= event.dt;
			if (g_time <= 0) {
				g_gameLost = true;
				getEnginePointer()->m_irrklangEngine->removeAllSoundSources();
				getEnginePointer()->m_irrklangEngine->play2D("media/sounds/gameover.wav", false);
			}
		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	class EventListenerFrameWriter : public VEEventListener {
	private:
		vh::VHVideoEncoder videoEncoder;
		std::ofstream outfile;

	protected:
		void onFrameEnded(veEvent event) override
		{
			const uint32_t RECORD_FPS = 100;
			const double TIME_BETWEEN_WRITES = 1.0 / RECORD_FPS;
			static double timeSinceLastWrite = TIME_BETWEEN_WRITES;
			timeSinceLastWrite += event.dt;

			VkResult ret;
			const char* packetData;
			size_t packetSize;
			do {
				ret = videoEncoder.finishEncode(packetData, packetSize);
				if (ret != VK_SUCCESS && ret != VK_NOT_READY) {
					std::cout << "Error on VideoEncoder frame finish\n";
				}
				if (packetSize > 0) {
					if (!outfile.is_open()) {
						outfile.open("hwenc.264", std::ios::binary);
					}
					outfile.write(packetData, packetSize);
				}
			} while (packetSize > 0);

			if (!g_writeFrames || timeSinceLastWrite < TIME_BETWEEN_WRITES)
				return;
			timeSinceLastWrite = 0.0;

			// queue another frame for copy
			VkExtent2D extent = getWindowPointer()->getExtent();
			ret = videoEncoder.init(getEnginePointer()->getRenderer()->getPhysicalDevice(),
				getEnginePointer()->getRenderer()->getDevice(),
				getEnginePointer()->getRenderer()->getVmaAllocator(),
				getEnginePointer()->getRenderer()->getGraphicsQueueFamily(),
				getEnginePointer()->getRenderer()->getGraphicsQueue(),
				getEnginePointer()->getRenderer()->getCommandPool(),
				getEnginePointer()->getRenderer()->getEncodeQueueFamily(),
				getEnginePointer()->getRenderer()->getEncodeQueue(),
				getEnginePointer()->getRenderer()->getEncodeCommandPool(),
				getEnginePointer()->getRenderer()->getSwapChainImageViews(),
				extent.width, extent.height, RECORD_FPS);
			if (ret != VK_SUCCESS) {
				std::cout << "Error initializing VideoEncoder\n";
				g_writeFrames = false;
				return;
			}

			ret = videoEncoder.queueEncode(getEnginePointer()->getRenderer()->getImageIndex());
			if (ret != VK_SUCCESS) {
				std::cout << "Error using VideoEncoder\n";
			}
		}

	public:
		///Constructor of class EventListenerCollision
		EventListenerFrameWriter(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerFrameWriter() {};
	};

	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	public:

		MyVulkanEngine(veRendererType type = veRendererType::VE_RENDERER_TYPE_FORWARD, bool debug=false) : VEEngine(type, debug) {};
		~MyVulkanEngine() {};


		///Register an event listener to interact with the user
		
		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new EventListenerCollision("Collision"), { veEvent::VE_EVENT_FRAME_STARTED });
			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY});
			registerEventListener(new EventListenerFrameWriter("FrameWriter"), { veEvent::VE_EVENT_FRAME_ENDED });
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

			m_irrklangEngine->play2D("media/sounds/ophelia.wav", true);
		};
	};


}

using namespace ve;

int main() {
	bool debug = true;
#ifdef VULKAN_VIDEO_ENCODE
	debug = false; //validation layer not supported for beta extensions
#endif

	MyVulkanEngine mve(veRendererType::VE_RENDERER_TYPE_FORWARD, debug);	//enable or disable debugging (=callback, validation layers)

	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	return 0;
}
