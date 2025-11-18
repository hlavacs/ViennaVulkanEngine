#define _USE_MATH_DEFINES // for C++
#include <cmath>
#include <iostream>
#include <utility>
#include <format>
#include "VHInclude.h"
#include "VEInclude.h"

#include "VPE.hpp"


class MyGame : public vve::System {

	std::default_random_engine rnd_gen{ 12345 };					//Random numbers
	std::uniform_real_distribution<> rnd_unif{ 0.0f, 1.0f };		//Random numbers

    public:
        MyGame( vve::Engine& engine ) : vve::System("MyGame", engine ) {
            m_static_registry = &m_registry;

            m_engine.RegisterCallbacks( { 
                {this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
                {this,  10000, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
			    {this,      0, "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
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

        inline static vecs::Registry* m_static_registry{};
        vpe::VPEWorld::callback_move onMove = [&](double dt, std::shared_ptr<vpe::VPEWorld::Body> body) {
            auto pos = body->m_positionW;													// New position of the scene node
	        auto orient = body->m_orientationLW;											// New orientation of the scende node
	        body->stepPosition(dt, pos, orient, false);										// Extrapolate
            auto model = vpe::VPEWorld::Body::computeModel(pos, orient, body->m_scale);
	        vecs::Handle node = vecs::Handle(reinterpret_cast<size_t>(body->m_owner));		// Owner is a handle to a scene node
            m_registry.Put(node, vve::Position(vpe::fromPhysics(pos)), vve::Rotation(vpe::fromPhysics(toMat3(orient))));
        };

        inline static vpe::VPEWorld::callback_erase onErase = [](std::shared_ptr<vpe::VPEWorld::Body> body) {
	        auto node = vecs::Handle(reinterpret_cast<size_t>(body->m_owner));					// Owner is a pointer to a scene node
	        //getSceneManagerPointer()->deleteSceneNodeAndChildren(((VESceneNode*)body->m_owner)->getName());
            return;
        };
    
        inline static std::string plane_obj  { "assets/test/plane/plane_t_n_s.obj" };
        inline static std::string plane_mesh { "assets/test/plane/plane_t_n_s.obj/plane" };
        inline static std::string plane_txt  { "assets/test/plane/grass.jpg" };

        inline static std::string cube_obj  { "assets/test/crate0/cube.obj" };

        bool OnLoadLevel( Message message ) {
            auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
            std::cout << "Loading level: " << msg.m_level << std::endl;
            std::string level = std::string("Level: ") + msg.m_level;

            // ----------------- Load Plane -----------------

			m_engine.LoadScene( vve::Filename{plane_obj}, aiProcess_FlipWindingOrder);

			m_engine.CreateObject(	vve::Name{},
                                    vve::ParentHandle{}, 
                                    vve::MeshName{plane_mesh}, 
									vve::TextureName{plane_txt}, 
									vve::Position{vec3_t{0.0f, 0.0f, 0.0f}}, 
									vve::Rotation{mat4_t{glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f))}}, 
									vve::Scale{vec3_t{1000.0f, 1000.0f, 1000.0f}}, 
									vve::UVScale{vec2_t{1000.0f, 1000.0f}});

            // ----------------- Load Cube -----------------

			//m_handleCube = m_engine.CreateScene(vve::Name{}, 
            //                            vve::ParentHandle{}, 
            //                            vve::Filename{cube_obj}, aiProcess_FlipWindingOrder, 
			//							  vve::Position{{nextRandom(), nextRandom(), 0.5f}}, 
            //                            vve::Rotation{mat3_t{1.0f}}, 
            //                            vve::Scale{vec3_t{1.0f}});

            GetCamera();
            m_registry.Get<vve::Rotation&>(m_cameraHandle)() = mat3_t{ glm::rotate(mat4_t{1.0f}, 3.14152f/2.0f, vec3_t{1.0f, 0.0f, 0.0f}) };

			//m_engine.PlaySound(vve::Filename{"assets/sounds/dance.mp3"}, -1, 50);
			m_engine.SetVolume(m_volume);
            return false;
        };
    
        bool OnUpdate( Message& message ) {
            auto msg = message.template GetData<vve::System::MsgUpdate>();
            auto dt = msg.m_dt;
            m_physics.tick(dt);
            return false;
        }

        bool OnKeyDown(Message message) {
			auto msg = message.template GetData<MsgKeyDown>();
			auto key = msg.m_key;

            if( key == SDL_SCANCODE_B  ) { 
                static uint64_t body_id{0};
                auto [pn, rn, sn, LtoPn] = m_registry.template Get<vve::Position&, vve::Rotation&, vve::Scale&, vve::LocalToParentMatrix>(m_cameraNodeHandle);
		        auto [pc, rc, sc, LtoPc] = m_registry.template Get<vve::Position&, vve::Rotation&, vve::Scale&, vve::LocalToParentMatrix>(m_cameraHandle);	
				
                glmvec3 dir{vec3_t{ LtoPn() * LtoPc() * vec4_t{0.0f, 0.0f, -1.0f, 0.0f} }};
                glmvec3 vel = (30.0_real + 5.0_real * (real)rnd_unif(rnd_gen)) * dir / glm::length(dir);
				glmvec3 scale{ 1,1,1 }; // = rnd_unif(rnd_gen) * 10;
				float angle = (real)rnd_unif(rnd_gen) * 10 * 3 * (real)M_PI / 180.0_real;
				glmvec3 orient{ rnd_unif(rnd_gen), rnd_unif(rnd_gen), rnd_unif(rnd_gen) };
				glmvec3 vrot{ rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5, rnd_unif(rnd_gen) * 5 };

                vecs::Handle handleCube = m_engine.CreateScene(vve::Name{}, 
                                        vve::ParentHandle{}, 
                                        vve::Filename{cube_obj}, aiProcess_FlipWindingOrder, 
										vve::Position{{0.0f, 0.0f, 0.0f}}, 
                                        vve::Rotation{mat3_t{1.0f}}, 
                                        vve::Scale{vec3_t{1.0f}});
                
                auto body = std::make_shared<vpe::VPEWorld::Body>(
                    &m_physics,
                    "Body" + std::to_string(m_physics.m_bodies.size()),
                    reinterpret_cast<void*>(handleCube.GetValue()), 
                    & m_physics.g_cube, 
                    scale, 
                    vpe::toPhysics(pn()), //glmmat3{C} * pn(), //to go from render to physics, positions and vectors must be multiplied by C
                    vpe::toPhysics(glm::rotate(glm::mat4{1.0f}, angle, glm::normalize(orient))), //glmmat4{CTrans} * glm::rotate(glm::mat4{1.0f}, angle, glm::normalize(orient)) * glmmat4{C}, //rotations R transform to CTrans * R * C
                    vpe::toPhysics(vel), //direction is same as vector
                    vpe::toPhysics(vrot),  //is a vector
                    1.0_real / 100.0_real, 
                    m_physics.m_restitution, 
                    m_physics.m_friction);
				
                body->setForce( 0ul, vpe::VPEWorld::Force{ {0, m_physics.c_gravity, 0} } );
				body->m_on_move = onMove;
				body->m_on_erase = onErase;

                onMove(0.0, body);
				m_physics.addBody(body);
            }

		    return false;
        }
    
        bool OnRecordNextFrame(Message message) { 

            ImGui::Begin("Game State");
            char buffer[100];
            //std::snprintf(buffer, 100, "Time Left: %.2f s", m_time_left);
            //ImGui::TextUnformatted(buffer);
            //std::snprintf(buffer, 100, "Cubes Left: %d", m_cubes_left);
            //ImGui::TextUnformatted(buffer);
        	if (ImGui::SliderFloat("Sound Volume", &m_volume, 0, MIX_MAX_VOLUME)) {
		    	m_engine.SetVolume(m_volume);
			}
            ImGui::End();
            return false;
        }

    private:
    	vpe::VPEWorld m_physics;

        vecs::Handle m_handlePlane{};
        vecs::Handle m_handleCube{};
		vecs::Handle m_cameraHandle{};
		vecs::Handle m_cameraNodeHandle{};
		float m_volume{MIX_MAX_VOLUME / 2.0};
    };
    
    
    
    int main() {
        vve::Engine engine("My Engine", vve::RendererType::RENDERER_TYPE_FORWARD) ;
        MyGame mygui{engine};  
        engine.Run();
    
        return 0;
    }
    
    