
#include <iostream>
#include <utility>
#include <format>
#include "VHInclude.h"
#include "VEInclude.h"

class MyGame : public vve::System {

        enum class State : int {
            STATE_RUNNING,
            STATE_DEAD
        };

        const float c_max_time = 35.0f;
        const int c_field_size = 50;
        const int c_number_cubes = 10;

        int nextRandom() {
            return rand() % (c_field_size) - c_field_size/2;
        }

    public:
        MyGame( vve::Engine& engine ) : vve::System("MyGame", engine ) {
    
            m_engine.RegisterCallbacks( { 
                {this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
                {this,  10000, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
                {this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
            } );
            m_engine.SetVolume(m_volume);
        };
        
        ~MyGame() {};

        void GetCamera() {
            if(m_cameraHandle.IsValid() == false) { 
                auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin(); 
                m_cameraHandle = handle;
                m_cameraNodeHandle = parent;
            };
        }

        //inline static std::string test_scene_gltf{ "assets/shader_room_gltf/test_scene.gltf" };
        inline static std::string test_scene_gltf{ "assets/sponza/sponza.gltf" };
        inline static std::string test_scene_penetrationTest_gltf{ "assets/light_penetration/light_penetration.gltf" };


        bool OnLoadLevel( Message message ) {
            auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
            std::cout << "Loading level: " << msg.m_level << std::endl;
            std::string level = std::string("Level: ") + msg.m_level;

            // ----------------- Load Plane -----------------
            /*
			m_engine.LoadScene( vve::Filename{plane_obj}, aiProcess_FlipWindingOrder);

            m_engine.CreateObject(vve::Name{},
                vve::ParentHandle{},
                vve::MeshName{ plane_mesh },
                vve::MaterialName{ plane_material },
                vve::TextureName{ plane_txt },
                vve::Position{ vec3_t{0.0f, 0.0f, 0.0f} },
                vve::Rotation{ mat4_t{glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f))} },
                vve::Scale{ vec3_t{1000.0f, 1000.0f, 1000.0f} },
                vve::UVScale{ vec2_t{1000.0f, 1000.0f} });

            // ----------------- Load Cube -----------------

			m_handleCube = m_engine.CreateScene(vve::Name{}, vve::ParentHandle{}, vve::Filename{cube_obj}, aiProcess_FlipWindingOrder, 
												vve::Position{{nextRandom(), nextRandom(), 0.5f}}, vve::Rotation{mat3_t{1.0f}}, vve::Scale{vec3_t{1.0f}});

            m_engine.CreateVRTSphereLight(vve::Name{}, vve::ParentHandle{}, vec3_t{ 5000.0f }, 4.0f, vve::Position{ {0.0f,0.0f,20.0f} });
            */

            //m_engine.PlaySound(vve::Filename{ "assets/sounds/dance.mp3" }, -1, 50);
            m_engine.SetVolume(m_volume);


            //The bounding box for the morton code generation is curreently hardcoded, changing the scene requires changing the morton code! 
            m_engine.CreateScene(vve::Name{}, vve::ParentHandle{}, vve::Filename{ test_scene_gltf }, aiProcess_FlipWindingOrder, vve::Position{ {0.0,0.0,0.0} }, vve::Rotation{ mat3_t{1.0f} }, vve::Scale{ vec3_t{1.0f} });
            //m_engine.CreateScene(vve::Name{}, vve::ParentHandle{}, vve::Filename{ test_scene_penetrationTest_gltf }, aiProcess_FlipWindingOrder, vve::Position{ {0.0,0.0,0.0} }, vve::Rotation{ mat3_t{1.0f} }, vve::Scale{ vec3_t{1.0f} });

            
            m_engine.CreateVRTSphereLight(vve::Name{}, vve::ParentHandle{}, vec3_t{ 2000.0f }, 0.5f, vve::Position{ {7.44097f,-0.608485f,4.85042f} });
            m_engine.CreateVRTSphereLight(vve::Name{}, vve::ParentHandle{}, vec3_t{ 2000.0f }, 0.5f, vve::Position{ {-5.81945f,0.797917f,2.57471f} });

            //m_engine.CreateVRTSphereLight(vve::Name{}, vve::ParentHandle{}, vec3_t{ 2000.0f }, 0.001f, vve::Position{ {4.50462,3.30285 ,1.16441} });

            std::mt19937 rng(2);

            std::uniform_real_distribution<float> distX(-11.0f, 9.8f);
            std::uniform_real_distribution<float> distY(-1.15f, 4.95f);
            std::uniform_real_distribution<float> distZ(0.12f, 6.65f);

            
            /*
            for (int i = 0; i < 200; i++) {
                vec3_t pos{
                    distX(rng),
                    distY(rng),
                    distZ(rng)
                };

                m_engine.CreateVRTSphereLight(vve::Name{}, vve::ParentHandle{}, vec3_t{ 5.0f }, 0.1f, vve::Position{ pos });
            }
            */

            GetCamera();
            m_registry.Get<vve::Rotation&>(m_cameraHandle)() = mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f/2.0f, vec3_t{1.0f, 0.0f, 0.0f}) };
            auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
            pos() = vec3_t(0.0f, 0.0f, 0.5f);


            auto view = m_registry.GetView<vecs::Handle, vvh::VRTSettings&>();
            auto iterBegin = view.begin();
            auto iterEnd = view.end();
            if (!(iterBegin != iterEnd)) {
                m_renderSettingsHandle = m_registry.Insert(vvh::VRTSettings{});
                m_renderSettings = m_registry.Get<vvh::VRTSettings&>(m_renderSettingsHandle);
            }
            else {
                auto [handleV, stateV] = *iterBegin;
                m_renderSettingsHandle = handleV;
                m_renderSettings = stateV;
            }

            return false;
            
        };
    
        bool OnUpdate( Message& message ) {
            /*
            auto msg = message.template GetData<vve::System::MsgUpdate>();
            m_time_left -= static_cast<float>(msg.m_dt);
            auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
            pos().z = 0.5f;
            if( m_state == State::STATE_RUNNING ) {
                if( m_time_left <= 0.0f ) { 
                    m_state = State::STATE_DEAD; 

                    m_engine.PlaySound( vve::Filename{"assets/sounds/dance.mp3"}, 0, 50 );
                    m_engine.PlaySound( vve::Filename{"assets/sounds/gameover.wav"}, 1, 50 );
                    return false;
                }
                auto posCube = m_registry.Get<vve::Position&>(m_handleCube);
                float distance = glm::length( vec2_t{pos().x, pos().y} - vec2_t{posCube().x, posCube().y} );
                if( distance < 1.5f) {
                    m_cubes_left--;
                    posCube().x = static_cast<float>(nextRandom());
                    posCube().y = static_cast<float>(nextRandom());
                    if( m_cubes_left == 0 ) {
                        m_time_left += 20;
                        m_cubes_left = c_number_cubes;
                        m_engine.PlaySound( vve::Filename{"assets/sounds/bell.wav"}, 1 );
                    } else {
                        m_engine.PlaySound( vve::Filename{"assets/sounds/explosion.wav"}, 1 );
                    }
                }
            }
            */
            return false;
        }
    
        bool OnRecordNextFrame(Message message) { 

            if (ImGui::BeginCombo("RenderMethode", renderMethodeOptions[current_render_methode].c_str()))
            {
                for (int i = 0; i < renderMethodeOptions.size(); ++i)
                {
                    bool selected = (current_render_methode == i);

                    if (ImGui::Selectable(renderMethodeOptions[i].c_str(), selected))
                        current_render_methode = i;
                    // Do something immediately
                    switch (current_render_methode)
                    {
                    case 0:
                        m_renderSettings().methode = vvh::RenderMethode::BACKWARD;
                        break;
                    case 1:
                        m_renderSettings().methode = vvh::RenderMethode::FORWARD;
                        break;
                    case 2:
                        m_renderSettings().methode = vvh::RenderMethode::RESTIRGI;
                        break;
                    case 3:
                        m_renderSettings().methode = vvh::RenderMethode::RESTIRLVC;
                        break;
                    case 4:
                        m_renderSettings().methode = vvh::RenderMethode::RESTIRLVCCOMBINED;
                        break;
                    case 5:
                        m_renderSettings().methode = vvh::RenderMethode::RESTIRIR;
                        break;
                    case 6:
                        m_renderSettings().methode = vvh::RenderMethode::IRTESTING;
                        break;
                    case 7:
                        m_renderSettings().methode = vvh::RenderMethode::IR;
                        break;
                    case 8:
                        m_renderSettings().methode = vvh::RenderMethode::RESTIRIRNOREPLACMENT;
                        break;
                    }



                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (ImGui::BeginCombo("Illumination Domain", illuminationDomainOptions[current_illumination_domain].c_str()))
            {
                for (int i = 0; i < illuminationDomainOptions.size(); ++i)
                {
                    bool selected = (current_illumination_domain == i);

                    if (ImGui::Selectable(illuminationDomainOptions[i].c_str(), selected))
                        current_illumination_domain = i;
                    // Do something immediately
                    switch (current_illumination_domain)
                    {
                    case 0:
                        m_renderSettings().domain = vvh::IlluminationDomain::COMBINED;
                        break;
                    case 1:
                        m_renderSettings().domain = vvh::IlluminationDomain::DIRECT;
                        break;
                    case 2:
                        m_renderSettings().domain = vvh::IlluminationDomain::INDIRECT;
                        break;
                    }

                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            
            
            return false;
        }

    private:
        State m_state = State::STATE_RUNNING;
        float m_time_left = c_max_time;
        int m_cubes_left = c_number_cubes;  
        vecs::Handle m_handlePlane{};
        vecs::Handle m_handleCube{};
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};
		float m_volume{MIX_MAX_VOLUME / 2.0};

        std::vector<std::string> renderMethodeOptions = {
            "Backward",
            "Forward",
            "RestirGI",
            "RestirLVC",
            "RestirLVC Combined",
            "Restir IR",
            "IR Testing",
            "IR",
            "Restir IR No Replacment"
        };

        std::vector<std::string> illuminationDomainOptions = {
            "Direct and Indirect Illumination",
            "Direct Illumination Only",
            "Indirect Illumination Only"
        };

        int current_render_methode = 0;

        int current_illumination_domain = 0;

        vecs::Ref<vvh::VRTSettings> m_renderSettings{};
        vecs::Handle m_renderSettingsHandle{};

    };
    
    
    
    int main() {
        vve::Engine engine("My Engine", vve::RendererType::RENDERER_TYPE_RAYTRACING) ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    