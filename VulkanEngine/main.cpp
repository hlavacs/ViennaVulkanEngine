/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



namespace ve {


	uint32_t getScore();

	class EventListenerGUI : public VEEventListener {
	protected:
		virtual void onDrawOverlay(veEvent event) {
			VESubrenderFW_Nuklear * pSubrender = (VESubrenderFW_Nuklear*)getRendererPointer()->getOverlay();
			if (pSubrender == nullptr) return;

			struct nk_context * ctx = pSubrender->getContext();

			/* GUI */
			if (nk_begin(ctx, "Score", nk_rect(0, 0, 200, 100),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
			{
				char outbuffer[100];
				nk_layout_row_static(ctx, 30, 80, 1);
				sprintf(outbuffer, "%03d", getScore());
				nk_label(ctx, outbuffer, NK_TEXT_LEFT);
			}

			nk_end(ctx);
		};

	public:
		///Constructor of class EventListenerGUI
		EventListenerGUI(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerGUI
		virtual ~EventListenerGUI() {};
	};


	class EventListenerCollision : public VEEventListener {
	protected:
		virtual void onFrameStarted(veEvent event) {

		};

	public:
		///Constructor of class EventListenerCollision
		EventListenerCollision(std::string name) : VEEventListener(name) { };

		///Destructor of class EventListenerCollision
		virtual ~EventListenerCollision() {};
	};

	

	///user defined manager class, derived from VEEngine
	class MyVulkanEngine : public VEEngine {
	protected:

	public:
		uint32_t m_score = 0;

		/**
		* \brief Constructor of my engine
		* \param[in] debug Switch debuggin on or off
		*/
		MyVulkanEngine( bool debug=false) : VEEngine(debug) {};
		~MyVulkanEngine() {};

		///Register an event listener to interact with the user
		virtual void registerEventListeners() {
			VEEngine::registerEventListeners();

			registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY});
		};


		///create many cubes
		void createCubes(uint32_t n, VESceneNode *parent ) {

			float stride = 300.0f;
			static std::default_random_engine e{12345};
			static std::uniform_real_distribution<> d{ 1.0f, stride }; 

			VEMesh *pMesh;
			VECHECKPOINTER( pMesh = getSceneManagerPointer()->getMesh("media/models/test/crate0/cube.obj/cube") );

			VEMaterial *pMat;
			VECHECKPOINTER( pMat = getSceneManagerPointer()->getMaterial("media/models/test/crate0/cube.obj/cube") );

			for (uint32_t i = 0; i < n; i++) {		
				VESceneNode *pNode;
				VECHECKPOINTER( pNode = getSceneManagerPointer()->createSceneNode("The Node" + std::to_string(i), parent) );
				pNode->setTransform(glm::translate(glm::mat4(1.0f), glm::vec3( d(e) - stride/2.0f, d(e)/2.0f, d(e) - stride/2.0f)));

				VEEntity *e2;
				VECHECKPOINTER( e2 = getSceneManagerPointer()->createEntity("The Cube" + std::to_string(i), pMesh, pMat, pNode ) );
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
			VECHECKPOINTER( sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
										{	"bluecloud_ft.jpg", "bluecloud_bk.jpg", "bluecloud_up.jpg", 
											"bluecloud_dn.jpg", "bluecloud_rt.jpg", "bluecloud_lf.jpg" }, pScene)  );

			VESceneNode *e4;
			VECHECKPOINTER( e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj",0, pScene) );
			e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

			VEEntity *pE4;
			VECHECKPOINTER( pE4 = (VEEntity*)getSceneManagerPointer()->getSceneNode("The Plane/plane_t_n_s.obj/plane/Entity_0") );
			pE4->setParam( glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f) );

			std::vector<VEMesh*> meshes;
			std::vector<VEMaterial*> materials;
			getSceneManagerPointer()->loadAssets("media/models/test/crate0", "cube.obj", 0, meshes, materials);

		};
	};

	uint32_t getScore() {
		return ((MyVulkanEngine*)getEnginePointer())->m_score;
	}

}


using namespace ve;

int main() {

	bool debug = false;
#ifdef  _DEBUG
	debug = true;
#endif

	MyVulkanEngine mve(debug);	//enable or disable debugging (=callback, validation layers)

	mve.initEngine();
	mve.loadLevel(1);
	mve.run();

	return 0;
}

