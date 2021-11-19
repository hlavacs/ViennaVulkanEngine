/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"


namespace ve {


    uint32_t g_score = 0;                //derzeitiger Punktestand
    double g_time = 30.0;                //zeit die noch �brig ist
    bool g_gameLost = false;            //true... das Spiel wurde verloren
    bool g_restart = false;            //true...das Spiel soll neu gestartet werden

    //
    //Zeichne das GUI
    //
    class EventListenerGUI : public VEEventListener {
    protected:

        virtual void onDrawOverlay(veEvent event) {
            VESubrender_Nuklear *pSubrender = (VESubrender_Nuklear *) getEnginePointer()->getRenderer()->getOverlay();
            if (pSubrender == nullptr) return;

            struct nk_context *ctx = pSubrender->getContext();

            if (!g_gameLost) {
                if (nk_begin(ctx, "", nk_rect(0, 0, 200, 170), NK_WINDOW_BORDER)) {
                    char outbuffer[100];
                    nk_layout_row_dynamic(ctx, 45, 1);
                    sprintf(outbuffer, "Score: %03d", g_score);
                    nk_label(ctx, outbuffer, NK_TEXT_LEFT);

                    nk_layout_row_dynamic(ctx, 45, 1);
                    sprintf(outbuffer, "Time: %004.1lf", g_time);
                    nk_label(ctx, outbuffer, NK_TEXT_LEFT);
                }
            } else {
                if (nk_begin(ctx, "", nk_rect(500, 500, 200, 170), NK_WINDOW_BORDER)) {
                    nk_layout_row_dynamic(ctx, 45, 1);
                    nk_label(ctx, "Game Over", NK_TEXT_LEFT);
                    if (nk_button_label(ctx, "Restart")) {
                        g_restart = true;
                    }
                }

            };

            nk_end(ctx);
        }

    public:
        ///Constructor of class EventListenerGUI
        EventListenerGUI(std::string name) : VEEventListener(name) {};

        ///Destructor of class EventListenerGUI
        virtual ~EventListenerGUI() {};
    };


    static std::default_random_engine e{12345};                    //F�r Zufallszahlen
    static std::uniform_real_distribution<> d{-15.0f, 15.0f};        //F�r Zufallszahlen

    class EventListenerRotation : public VEEventListener {
    protected:
        VESceneNode *m_pNode;

        virtual void onFrameEnded(veEvent event) {
            float slow = 2.0;        //camera rotation speed
            float angle = slow * (float) event.dt;            //pitch angle
            glm::vec4 rot4dy =
                    m_pNode->getTransform() * glm::vec4(0.0, 1.0, 0.0, 1.0); //x axis from local to parent space!
            glm::vec3 rot3dy = glm::vec3(rot4dy.x, rot4dy.y, rot4dy.z);
            glm::mat4 rotatedy = glm::rotate(glm::mat4(1.0), angle, rot3dy);
            m_pNode->multiplyTransform(rotatedy);
        }


    public:
        ///Constructor of class EventListenerCollision
        EventListenerRotation(VESceneNode *node) : VEEventListener(node->getName() + " Rotation"), m_pNode(node) {};

        ///Destructor of class EventListenerCollision
        virtual ~EventListenerRotation() {};
    };


    ///user defined manager class, derived from VEEngine
    class MyVulkanEngine : public VEEngine {
    public:

        MyVulkanEngine(veRendererType type, bool debug = false) : VEEngine(type, debug) {};

        ~MyVulkanEngine() {};


        ///Register an event listener to interact with the user

        virtual void registerEventListeners() {
            VEEngine::registerEventListeners();

            //registerEventListener(new EventListenerGUI("GUI"), { veEvent::VE_EVENT_DRAW_OVERLAY});
            registerEventListener(new VEEventListenerNuklearDebug("DebugGUI"), {veEvent::VE_EVENT_DRAW_OVERLAY});

        };


        ///Load the first level into the game engine
        ///The engine uses Y-UP, Left-handed
        virtual void loadLevel(uint32_t numLevel = 1) {

            VEEngine::loadLevel(numLevel);            //create standard cameras and lights

            VESceneNode *pScene;
            VECHECKPOINTER(pScene = getSceneManagerPointer()->createSceneNode("Level 1", getRoot()));

            //create multiple ligt sources

            VELight *light1 = (VEDirectionalLight *) getSceneManagerPointer()->createLight("StandardDirLight",
                                                                                           VELight::VE_LIGHT_TYPE_DIRECTIONAL,
                                                                                           getRoot());     //new VEDirectionalLight("StandardDirLight");
            light1->lookAt(glm::vec3(0.0f, 20.0f, -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            light1->m_col_diffuse = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
            light1->m_col_specular = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);

            size_t lights_number = 1;
            for (int i = 0; i < lights_number; i++) {

                VELight *light = (VEPointLight *) getSceneManagerPointer()->createLight("StandardPointLight" + i,
                                                                                        VELight::VE_LIGHT_TYPE_POINT,
                                                                                        getSceneManagerPointer()->getCamera());
                light->multiplyTransform(glm::translate(glm::vec3(0.0f, 5.0f, 15.0f)));
                //light->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(d(e), 1.0f, d(e))));
                light->m_param[0] = 200.0f;

                //VELight *light = (VEDirectionalLight *)getSceneManagerPointer()->createLight("StandardDirLight" + i, VELight::VE_LIGHT_TYPE_DIRECTIONAL, getRoot());
                //light1->lookAt(glm::vec3(0.0f, d(e), -20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

                light->m_col_diffuse = glm::vec4(0.99f / lights_number, 0.99f / lights_number, 0.99f / lights_number,
                                                 1.0f);
                light->m_col_specular = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            }

            //scene models

            VESceneNode *sp1;
            VECHECKPOINTER(sp1 = getSceneManagerPointer()->createSkybox("The Sky", "media/models/test/sky/cloudy",
                                                                        {"bluecloud_ft.jpg", "bluecloud_bk.jpg",
                                                                         "bluecloud_up.jpg",
                                                                         "bluecloud_dn.jpg", "bluecloud_rt.jpg",
                                                                         "bluecloud_lf.jpg"}, pScene));


            VESceneNode *e4;
            VECHECKPOINTER(
                    e4 = getSceneManagerPointer()->loadModel("The Plane", "media/models/test", "plane_t_n_s.obj", 0,
                                                             pScene));
            e4->setTransform(glm::scale(glm::mat4(1.0f), glm::vec3(1000.0f, 1.0f, 1000.0f)));

            VEEntity *pE4;
            VECHECKPOINTER(pE4 = (VEEntity *) getSceneManagerPointer()->getSceneNode(
                    "The Plane/plane_t_n_s.obj/plane/Entity_0"));
            pE4->setParam(glm::vec4(1000.0f, 1000.0f, 0.0f, 0.0f));


            size_t cubes_number = 10;
            for (int i = 0; i < cubes_number; i++) {
                VESceneNode *e1, *eParent1;

                eParent1 = getSceneManagerPointer()->createSceneNode("The Cube Parent" + i, pScene, glm::mat4(1.0));
                VECHECKPOINTER(e1 = getSceneManagerPointer()->loadModel("The Cube" + i, "media/models/test/crate0",
                                                                        "cube.obj"));
                eParent1->multiplyTransform(glm::translate(glm::mat4(1.0f), glm::vec3(d(e), 1.0f, d(e))));
                eParent1->addChild(e1);

                registerEventListener(new EventListenerRotation(e1), {veEvent::VE_EVENT_FRAME_ENDED});
            }

            m_irrklangEngine->play2D("media/sounds/ophelia.wav", true);
        };
    };


}

using namespace ve;

int main() {
    MyVulkanEngine mve(VE_RENDERER_TYPE_FORWARD, true);   

    mve.initEngine();
    mve.loadLevel(1);
    mve.run();

    return 0;
}

