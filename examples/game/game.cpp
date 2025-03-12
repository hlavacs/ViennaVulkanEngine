
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
    
            m_engine.RegisterCallback( { 
                {this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
                {this,  10000, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
                {this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
            } );
        };
        
        ~MyGame() {};

        void GetCamera() {
            if(m_cameraHandle.IsValid() == false) { 
                auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, vve::Camera&, vve::ParentHandle>().begin(); 
                m_cameraHandle = handle;
                m_cameraNodeHandle = parent;
            };
        }
    
        inline static std::string plane_obj  { "assets\\test\\plane\\plane_t_n_s.obj" };
        inline static std::string plane_mesh { "assets\\test\\plane\\plane_t_n_s.obj\\plane" };
        inline static std::string plane_txt  { "assets\\test\\plane\\grass.jpg" };

        inline static std::string cube_obj  { "assets\\test\\crate0\\cube.obj" };

        bool OnLoadLevel( Message message ) {
            auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
            std::cout << "Loading level: " << msg.m_level << std::endl;
            std::string level = std::string("Level: ") + msg.m_level;

            // ----------------- Load Plane -----------------

            m_engine.SendMessage( MsgSceneLoad{ vve::Filename{plane_obj}, aiProcess_FlipWindingOrder });

            auto m_handlePlane = m_registry.Insert( 
                            vve::Position{ {0.0f,0.0f,0.0f } }, 
                            vve::Rotation{ mat3_t { glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f)) }}, 
                            vve::Scale{vec3_t{1000.0f,1000.0f,1000.0f}}, 
                            vve::MeshName{plane_mesh},
                            vve::TextureName{plane_txt},
                            vve::UVScale{ { 1000.0f, 1000.0f } }
                        );

            m_engine.SendMessage(MsgObjectCreate{  vve::ObjectHandle(m_handlePlane), vve::ParentHandle{} });
    
            // ----------------- Load Cube -----------------

            m_handleCube = m_registry.Insert( 
                            vve::Position{ { nextRandom(), nextRandom(), 0.5f } }, 
                            vve::Rotation{mat3_t{1.0f}}, 
                            vve::Scale{vec3_t{1.0f}});

            m_engine.SendMessage(MsgSceneCreate{ vve::ObjectHandle(m_handleCube), vve::ParentHandle{}, vve::Filename{cube_obj}, aiProcess_FlipWindingOrder });

            GetCamera();
            m_registry.Get<vve::Rotation&>(m_cameraHandle)() = mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f/2.0f, vec3_t{1.0f, 0.0f, 0.0f}) };

            m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\ophelia.wav"}, 2 });

            return false;
        };
    
        bool OnUpdate( Message& message ) {
            auto msg = message.template GetData<vve::System::MsgUpdate>();
            m_time_left -= msg.m_dt;
            auto pos = m_registry.Get<vve::Position&>(m_cameraNodeHandle);
            pos().z = 0.5f;
            if( m_state == State::STATE_RUNNING ) {
                if( m_time_left <= 0.0f ) { 
                    m_state = State::STATE_DEAD; 
                    m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\ophelia.wav"}, 0 });
                    m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\gameover.wav"}, 1 });
                    return false;
                }
                auto posCube = m_registry.Get<vve::Position&>(m_handleCube);
                float distance = glm::length( vec2_t{pos().x, pos().y} - vec2_t{posCube().x, posCube().y} );
                if( distance < 1.5f) {
                    m_cubes_left--;
                    posCube().x = nextRandom();
                    posCube().y = nextRandom();
                    if( m_cubes_left == 0 ) {
                        m_time_left += 20;
                        m_cubes_left = c_number_cubes;
                        m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\bell.wav"}, 1 });
                    } else {
                        m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\explosion.wav"}, 1 });
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
                ImGui::End();
            }

            if( m_state == State::STATE_DEAD ) {
                ImGui::Begin("Game State");
                ImGui::TextUnformatted("Game Over");
                if (ImGui::Button("Restart")) {
                    m_state = State::STATE_RUNNING;
                    m_time_left = c_max_time;
                    m_cubes_left = c_number_cubes;
                    m_engine.SendMessage(MsgPlaySound{ vve::Filename{"assets\\sounds\\ophelia.wav"}, 2 });
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
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};
    };
    
    
    
    int main() {
    
        vve::Engine engine("My Engine") ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    