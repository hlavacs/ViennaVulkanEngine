
#include <iostream>
#include <utility>
#include "VHInclude.h"
#include "VEInclude.h"

class MyGame : public vve::System {

        enum class State : int {
            STATE_RUNNING,
            STATE_DEAD
        };

    public:
        MyGame( vve::Engine& engine ) : vve::System("MyGame", engine ) {
    
            m_engine.RegisterCallback( { 
                {this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
                {this,      0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
                {this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
            } );
        };
        
        ~MyGame() {};
    
        inline static std::string plane_obj  { "assets\\test\\plane\\plane_t_n_s.obj" };
        inline static std::string plane_mesh { "assets\\test\\plane\\plane_t_n_s.obj\\plane" };
        inline static std::string plane_txt  { "assets\\test\\plane\\grass.jpg" };

        inline static std::string cube_obj  { "assets\\test\\cube1.obj" };
        inline static std::string cube_mesh { "assets\\test\\cube1.obj\\cube" };
        inline static std::string cube_txt  { "assets\\test\\cube1.png" };

        bool OnLoadLevel( Message message ) {
            auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
            std::cout << "Loading level: " << msg.m_level << std::endl;
            std::string level = std::string("Level: ") + msg.m_level;

            // ----------------- Load Plane -----------------

            m_engine.SendMessage( MsgSceneLoad{ this, nullptr, vve::Name{plane_obj} });

            auto m_handlePlane = m_registry.Insert( 
                            vve::Position{ {0.0f,0.0f,0.0f } }, 
                            vve::Rotation{ mat3_t { glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f)) }}, 
                            vve::Scale{vec3_t{1000.0f,1000.0f,1000.0f}}, 
                            vve::MeshName{plane_mesh},
                            vve::TextureName{plane_txt},
                            vve::UVScale{ { 1000.0f, 1000.0f } }
                        );

            m_engine.SendMessage(MsgObjectCreate{ this, nullptr, vve::ObjectHandle(m_handlePlane), vve::ParentHandle{} });
    
            // ----------------- Load Cube -----------------

            //m_engine.SendMessage( MsgSceneLoad{ this, nullptr, vve::Name{"assets\\test\\cube1.obj"} });

            m_handleCube = m_registry.Insert( 
                            vve::Position{ { x, y, 0.5f } }, 
                            vve::Rotation{mat3_t{1.0f}}, 
                            vve::Scale{vec3_t{1.0f}});

            m_engine.SendMessage(MsgSceneCreate{ this, nullptr, vve::ObjectHandle(m_handleCube), vve::ParentHandle{}, vve::Name{cube_obj} });

            return false;
        };
    
        bool OnUpdate( Message message ) {
            auto msg = message.template GetData<vve::System::MsgUpdate>();
            return false;
        }
    
        bool OnRecordNextFrame(Message message) {           
            if( m_engine.GetWindow("VVE Window")->GetIsMinimized()) {return false;}

            if( m_state == State::STATE_RUNNING ) {
                ImGui::Begin("Time Left"); 
                ImGui::TextUnformatted("Time Left: ");
                ImGui::End();
            }
            
            return false;
        }

    private:
        State m_state = State::STATE_RUNNING;
        float m_time_left = 30.0f;
        vecs::Handle m_handlePlane{};
        vecs::Handle m_handleCube{};
        float x = 0.0f, y = 0.0f;
    };
    
    
    
    int main() {
    
        vve::Engine engine("My Engine") ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    