
#include <iostream>
#include <utility>
#include <format>
#include "VHInclude2.h"
#include "VEInclude.h"
#include <random>

class MyGame : public vve::System {

        enum class State : int {
            STATE_RUNNING,
            STATE_DEAD
        };

        const float c_max_time = 35.0f;
        const int c_field_size = 50;
        const int c_number_cubes = 10;

        uint16_t currentNumberLights = 1;

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
            m_registry.Get<vve::Position&>(m_cameraNodeHandle)().x += 7.46f;
            m_registry.Get<vve::Position&>(m_cameraNodeHandle)().y -= 4.2f;
            m_registry.Get<vve::Position&>(m_cameraNodeHandle)().z += 1.46f;

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

            // TODO: Was 0.6f, revert back
            float intensity1 = 0.0f;
            auto lightHandle = m_registry.Insert(
                vve::Name{ "PointLight-1" },
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
            m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle), vve::ParentHandle{m_myParentHandle}, this });


            // -----------------  Spot Light 1 -----------------
            vvh::Color color3{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.1f, 0.1f, 0.9f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
            float intensity3 = 2.9f;
            auto lightHandle3 = m_registry.Insert(
                vve::Name{"SpotLight-1"},
                vve::SpotLight{vvh::LightParams{
        		    .color = glm::vec3(1.0f, 0.0f, 0.0f), 
                    .params = glm::vec4(3.0f, intensity3, 10.0, 0.01f), 
                    .attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
        	    }},
                vve::Position{ glm::vec3(7.0f, 1.5f, 2.0f) },
                //vve::Position{ glm::vec3(10.0f, -10.0f, 10.0f) },
                vve::Rotation{mat3_t{glm::rotate(glm::mat4(1.0f), -3.14152f / 5.0f, glm::vec3(1.0f,0.0f,0.0f)) }},
                vve::Scale{vec3_t{0.01f, 0.05f, 0.01f}},
                vve::LocalToParentMatrix{mat4_t{1.0f}},
                vve::LocalToWorldMatrix{mat4_t{1.0f}},
                color3,
        	    vve::MeshName{"assets/standard/sphere.obj/sphere"}
            );
            m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle3), vve::ParentHandle{}, this });

            return false;
        };
    
        bool OnUpdate( Message& message ) {
            auto& msg = message.template GetData<vve::System::MsgUpdate>();
            m_time_left -= static_cast<float>(msg.m_dt);
            auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
            pos().z = 1.5f;
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
            ImGui::Begin("Light-Settings", nullptr, 
                  ImGuiWindowFlags_NoDecoration
                | ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoMove);

            ImGui::Text("Select Light amount:");
            static const int options[] = { 1, 5, 10, 20, 40, 80 };
            for (size_t idx = 0; idx < std::size(options); ++idx) {
                uint16_t val = options[idx];
                char buf[16];
                sprintf(buf, "%d", val);
                if (ImGui::Button(buf, ImVec2(40, 30))) {
                    currentNumberLights = val;
                    manageLights(val);
                }
                if (idx + 1 < std::size(options))
                    ImGui::SameLine();
            }

            ImGui::Text("Current: %d", currentNumberLights);

            ImGui::End();

            return false;
        }

        void manageLights(const uint16_t& lightCount) {
            // deletes all lights first
            for (auto [handle, light] : m_registry.template GetView<vecs::Handle, vve::PointLight&>()) {
                m_engine.SendMsg(MsgObjectDestroy{ (vve::ObjectHandle)handle});
            }
            static constexpr vvh::Color sphereColor{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };

            for (uint16_t i = 0; i < lightCount; ++i) {
                // Random values but still close to main scene
                glm::vec3 pointLightPosition(randFloat(5.0f, 10.0f), randFloat(0.5f, 3.0), randFloat(1.75f, 2.5f));
                glm::vec3 pointLightColor(randFloat(0.0f, 1.0f), randFloat(0.0f, 1.0f), randFloat(0.0f, 1.0f));
                float intensity = randFloat(0.1f, 0.3f);

                auto lightHandle = m_registry.Insert(
                    vve::Name{ "PointLight-" + i },
                    vve::PointLight{ vvh::LightParams{
                        .color = pointLightColor,
                        .params = glm::vec4(1.0f, intensity, 10.0f, 0.01f),
                        .attenuation = glm::vec3(1.0f, 0.09f, 0.032f),
                    } },
                    vve::Position{ pointLightPosition },
                    vve::Rotation{ mat3_t{1.0f} },
                    vve::Scale{ vec3_t{0.01f, 0.01f, 0.01f} },
                    vve::LocalToParentMatrix{ mat4_t{1.0f} },
                    vve::LocalToWorldMatrix{ mat4_t{1.0f} },
                    sphereColor,
                    vve::MeshName{ "assets/standard/sphere.obj/sphere" }
                    );
                m_engine.SendMsg(MsgObjectCreate{ vve::ObjectHandle(lightHandle), vve::ParentHandle{}, this });
            }
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
        vecs::Handle m_myParentHandle{};
		float m_volume{MIX_MAX_VOLUME / 2.0};

        static std::mt19937& getRng() {
            static std::mt19937 engine{ std::random_device{}() };
            return engine;
        }

        float randFloat(float min, float max) {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(getRng());
        }
    };
    
    
    
    int main() {
        vve::Engine engine("My Engine", VK_MAKE_VERSION(1, 3, 0)) ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    