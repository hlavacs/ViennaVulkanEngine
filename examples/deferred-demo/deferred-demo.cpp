
#include <iostream>
#include <utility>
#include <format>
#include "VHInclude2.h"
#include "VEInclude.h"

class MyGame : public vve::System {

        enum class State : int {
            STATE_RUNNING,
            STATE_DEAD
        };

        const float c_max_time = 35.0f;
        const int c_field_size = 50;
        const int c_number_cubes = 10;

        int nextRandom() const {
            return rand() % (c_field_size) - c_field_size/2;
        }

    public:
        MyGame( vve::Engine& engine ) : vve::System("MyGame", engine ) {
    
            m_engine.RegisterCallbacks( { 
                {this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
                {this,  10000, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
                {this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
            } );
            m_engine.SendMsg(MsgSetVolume{ (int)m_volume });
        };
        
        ~MyGame() {};

        void GetCamera() {
            if(m_cameraHandle.IsValid() == false) { 
                auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin(); 
                m_cameraHandle = handle;
                m_cameraNodeHandle = parent;
            };
        }
    
        inline static std::string plane_obj  { "assets/test/plane/plane_t_n_s.obj" };
        inline static std::string plane_mesh { "assets/test/plane/plane_t_n_s.obj/plane" };
        inline static std::string plane_txt  { "assets/test/plane/grass.jpg" };

        inline static std::string cube_obj  { "assets/test/crate0/cube.obj" };
        inline static std::string cornell_obj  { "assets/test/cornell/CornellBox-Original.obj" };

        bool OnLoadLevel( Message message ) {
            auto& msg = message.template GetData<vve::System::MsgLoadLevel>();	
            std::cout << "Loading level: " << msg.m_level << std::endl;
            std::string level = std::string("Level: ") + msg.m_level;

            // ----------------- Load Plane -----------------

            m_engine.SendMsg( MsgSceneLoad{ vve::Filename{plane_obj}, aiProcess_FlipWindingOrder });

            auto m_handlePlane = m_registry.Insert( 
                            vve::Position{ {0.0f,0.0f,0.0f } }, 
                            vve::Rotation{ mat3_t { glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f)) }}, 
                            vve::Scale{vec3_t{1000.0f,1000.0f,1000.0f}}, 
                            vve::MeshName{plane_mesh},
                            vve::TextureName{plane_txt},
                            vve::UVScale{ { 1000.0f, 1000.0f } }
                        );

            m_engine.SendMsg(MsgObjectCreate{  vve::ObjectHandle(m_handlePlane), vve::ParentHandle{}, this });
    
            // ----------------- Load Cube -----------------

            m_handleCube = m_registry.Insert( 
                            vve::Position{ { nextRandom(), nextRandom(), 0.5f } }, 
                            vve::Rotation{mat3_t{1.0f}}, 
                            vve::Scale{vec3_t{1.0f}});

            m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(m_handleCube), vve::ParentHandle{}, vve::Filename{cube_obj}, aiProcess_FlipWindingOrder });

            GetCamera();
            m_registry.Get<vve::Rotation&>(m_cameraHandle)() = mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f/2.0f, vec3_t{1.0f, 0.0f, 0.0f}) };

            m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/dance.mp3"}, -1, 50 });
			m_engine.SendMsg(MsgSetVolume{ (int)m_volume });

            // ----------------- Load Cornell -----------------

            m_engine.SendMsg(MsgSceneLoad{ vve::Filename{cornell_obj}, aiProcess_PreTransformVertices });
            m_handleCornell = m_registry.Insert(
                vve::Position{ { 0.0f, 0.0f, -0.1f } },
                vve::Rotation{ mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f / 2.0f, vec3_t{1.0f, 0.0f, 0.0f}) } },
                vve::Scale{ vec3_t{1.0f} }
            );
            m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(m_handleCornell), vve::ParentHandle{}, vve::Filename{cornell_obj}, aiProcess_PreTransformVertices });
            // cornell camera position
            m_registry.Get<vve::Position&>(m_cameraHandle)().z += 0.46f;
            m_registry.Get<vve::Position&>(m_cameraHandle)().y -= 1.2f;

            // -----------------  Fireplace -----------------
            aiPostProcessSteps flags = static_cast<aiPostProcessSteps>(aiProcess_PreTransformVertices | aiProcess_ImproveCacheLocality);
            m_engine.SendMsg(MsgSceneLoad{ vve::Filename{"assets/test/Fireplace/Fireplace.gltf"}, flags });
            m_test = m_registry.Insert(
                vve::Position{ { 5.0f, 0.0f, 0.1f } },
                vve::Rotation{ mat3_t{glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))} },
                //vve::Rotation{ mat3_t{1.0f} },
                vve::Scale{ vec3_t{1.0f} }
            );
            m_engine.SendMsg(MsgSceneCreate{ vve::ObjectHandle(m_test), vve::ParentHandle{}, vve::Filename{"assets/test/Fireplace/Fireplace.gltf"}, flags });


            // -----------------  Point Light 1 -----------------
            m_engine.SendMsg(MsgSceneLoad{ vve::Filename{"assets/standard/sphere.obj"} });
            vvh::Color sphereColor{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

            float intensity1 = 0.8f;
            auto lightHandle = m_registry.Insert(
                vve::Name{ "Light1" },
                vve::PointLight{ vvh::LightParams{
                    .color = glm::vec3(1.0f, 1.0f, 1.0f), 
                    .params = glm::vec4(1.0f, intensity1, 10.0f, 0.01f), 
                    .attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
                } },
                vve::Position{ glm::vec3(7.0f, 1.5f, 2.0f) },
                vve::Rotation{ mat3_t{1.0f} },
                vve::Scale{ vec3_t{0.01f, 0.01f, 0.01f} },
                vve::LocalToParentMatrix{ mat4_t{1.0f} },
                vve::LocalToWorldMatrix{ mat4_t{1.0f} },
                sphereColor,
                vve::MeshName{ "assets/standard/sphere.obj/sphere" }
                );
            m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle), vve::ParentHandle{}, this });

            return false;
        };
    
        bool OnUpdate( Message& message ) {
            auto& msg = message.template GetData<vve::System::MsgUpdate>();
            m_time_left -= static_cast<float>(msg.m_dt);
            auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
            pos().z = 0.5f;
            if( m_state == State::STATE_RUNNING ) {
                if( m_time_left <= 0.0f ) { 
                    m_state = State::STATE_DEAD; 
                    m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/dance.mp3"}, 0 });
                    m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/gameover.wav"}, 1 });
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
                        m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/bell.wav"}, 1 });
                    } else {
                        m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/explosion.wav"}, 1 });
                    }
                }
            }

            return false;
        }
    
        bool OnRecordNextFrame(Message message) { 
            if( m_state == State::STATE_RUNNING ) {
                ImGui::Begin("Game State");
                char buffer[100];
                std::snprintf(buffer, 100, "Time Left: %.2f s", m_time_left);
                ImGui::TextUnformatted(buffer);
                std::snprintf(buffer, 100, "Cubes Left: %d", m_cubes_left);
                ImGui::TextUnformatted(buffer);
				if (ImGui::SliderFloat("Sound Volume", &m_volume, 0, MIX_MAX_VOLUME)) {
					m_engine.SendMsg(MsgSetVolume{ (int)m_volume });
				}
                ImGui::End();
            }

            if( m_state == State::STATE_DEAD ) {
                ImGui::Begin("Game State");
                ImGui::TextUnformatted("Game Over");
                if (ImGui::Button("Restart")) {
                    m_state = State::STATE_RUNNING;
                    m_time_left = c_max_time;
                    m_cubes_left = c_number_cubes;
                    m_engine.SendMsg(MsgPlaySound{ vve::Filename{"assets/sounds/dance.mp3"}, -1 });
                }
                ImGui::End();
            }
            return false;
        }

    private:
        State m_state = State::STATE_RUNNING;
        float m_time_left = c_max_time;
        int m_cubes_left = c_number_cubes;  
        vecs::Handle m_handlePlane{};
        vecs::Handle m_handleCube{};
        vecs::Handle m_handleCornell{};
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};
        vecs::Handle m_test{};
		float m_volume{MIX_MAX_VOLUME / 2.0};
    };
    
    
    
    int main() {
        vve::Engine engine("My Engine", VK_MAKE_VERSION(1, 3, 0)) ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    