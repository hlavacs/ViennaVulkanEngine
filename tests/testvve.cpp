
#include <iostream>
#include <utility>
#include <stack>
#include <time.h>

#include "VHInclude.h"
#include "VEInclude.h"

//#include "L2DFileDialog.h"


class MyGUI : public vve::System {

public:
    MyGUI( vve::Engine& engine ) : vve::System("MyGUI", engine ) {

		m_engine.RegisterCallback( { 
			{this,      0, "LOAD_LEVEL", [this](Message& message){ return OnLoadLevel(message);} },
			{this,      0, "UPDATE", [this](Message& message){ return OnUpdate(message);} },
			{this, -10000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this,      0, "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyRepeat(message);} },
			{this,      0, "SDL_KEY_UP", [this](Message& message){ return OnKeyUp(message);} }
		} );
    };
    
    ~MyGUI() {};

	bool OnLoadLevel( Message message ) {
		auto msg = message.template GetData<vve::System::MsgLoadLevel>();	
		std::cout << "Loading level: " << msg.m_level << std::endl;
		std::string level = std::string("Level: ") + msg.m_level;

		m_engine.SendMessage( MsgSceneLoad{ vve::Filename{"assets\\test\\plane\\plane_t_n_s.obj"}, aiProcess_FlipWindingOrder });
		m_planeHandle = m_registry.Insert( 
						vve::Position{ {0.0f,0.0f,0.0f } }, 
						vve::Rotation{ mat3_t { glm::rotate(glm::mat4(1.0f), 3.14152f / 2.0f, glm::vec3(1.0f,0.0f,0.0f)) }}, 
						vve::Scale{vec3_t{1000.0f,1000.0f,1000.0f}}, 
						vve::MeshName{"assets\\test\\plane\\plane_t_n_s.obj\\plane"},
						vve::TextureName{"assets\\test\\plane\\grass.jpg"},
						vve::UVScale{ { 1000.0f, 1000.0f } }
					);
		m_engine.SendMessage(MsgObjectCreate{  vve::ObjectHandle(m_planeHandle), vve::ParentHandle{} });

		return false;
	};

	bool OnUpdate( Message message ) {
		auto msg = message.template GetData<vve::System::MsgUpdate>();
		return false;
	}

    bool OnRecordNextFrame(Message message) {
      
        static bool show_demo_window = false;
        static bool show_another_window = false;

		auto [handle, wstate] = vve::Window::GetState(m_registry);

        if( wstate().m_isMinimized) {return false;}

		static float x = 0.0f, y = 0.0f;

        {
            ImGui::Begin("Load Object");                          // Create a window called "Hello, world!" and append into it.		
			ImGui::PushItemWidth(300);
			static char* file_dialog_buffer = nullptr;
			{
				static char path_obj[500] = "assets\\viking_room\\viking_room.obj";
				ImGui::TextUnformatted("File: ");
				ImGui::SameLine();
				ImGui::InputText("##path1", path_obj, sizeof(path_obj));
				ImGui::SameLine();
				if (ImGui::Button("Browse##Browse1")) {
				  file_dialog_buffer = path_obj;
				  //FileDialog::file_dialog_open = true;
				  //FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
				}

				ImGui::SameLine();
				//if (FileDialog::file_dialog_open) {
				//  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
				//}

				static aiPostProcessSteps flags = aiProcess_Triangulate;
				static bool checkbox = false;
				ImGui::Checkbox("Flip winding order##Winding1", &checkbox);
				if( checkbox ) flags = aiProcess_FlipWindingOrder;

				ImGui::SameLine();
	            if (ImGui::Button("Create##Create1")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)				
					
					auto handle = m_registry.Insert( vve::Position{ { x, y, 0.1f } }, vve::Rotation{mat3_t{1.0f}}, vve::Scale{vec3_t{1.0f}}); 
					m_handles.push( handle );

					m_engine.SendMessage( 
						MsgSceneCreate{	 vve::ObjectHandle{handle}, vve::ParentHandle{}, vve::Filename{path_obj}, flags });
					x += 2.0f;
				}
			}

			{
				static char path_obj2[500] = "assets\\standard\\sphere.obj";
				ImGui::TextUnformatted("File: ");
				ImGui::SameLine();
				ImGui::InputText("##path2", path_obj2, sizeof(path_obj2));
				ImGui::SameLine();
				if (ImGui::Button("Browse##Browse2")) {
				  file_dialog_buffer = path_obj2;
				  //FileDialog::file_dialog_open = true;
				  //FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
				}

				ImGui::SameLine();
				//if (FileDialog::file_dialog_open) {
				//  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
				//}

				static aiPostProcessSteps flags = aiProcess_Triangulate;
				static bool checkbox = false;
				ImGui::SameLine();
				ImGui::Checkbox("Flip winding order##Winding2", &checkbox);
				if( checkbox ) flags = aiProcess_FlipWindingOrder;

				ImGui::SameLine();
	            if (ImGui::Button("Create##Create2")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)		

					m_engine.SendMessage( MsgSceneLoad{ vve::Filename{path_obj2}, flags });		
					vh::Color color{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.2f, 0.2f, 0.2f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
					auto handle = m_registry.Insert(vve::Position{ { x, y, 0.5f } }, vve::Rotation{mat3_t{1.0f}}, vve::Scale{vec3_t{0.05f}}, color, vve::MeshName{"assets\\standard\\sphere.obj\\sphere"} );
					m_handles.push( handle );

					m_engine.SendMessage(MsgObjectCreate{  vve::ObjectHandle(handle), vve::ParentHandle{} });

					x += 2.0f;		
				}
			}

			{
				static char path_obj3[500] = "assets\\test\\cube1.obj";
				ImGui::TextUnformatted("File: ");
				ImGui::SameLine();
				ImGui::InputText("##path3", path_obj3, sizeof(path_obj3));
				ImGui::SameLine();
				if (ImGui::Button("Browse##Browse3")) {
				  file_dialog_buffer = path_obj3;
				  //FileDialog::file_dialog_open = true;
				  //FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
				}

				//if (FileDialog::file_dialog_open) {
				//  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
				//}

				static aiPostProcessSteps flags = aiProcess_Triangulate;
				static bool checkbox = true;
				ImGui::SameLine();
				ImGui::Checkbox("Flip winding order##Winding3", &checkbox);
				if( checkbox ) flags = aiProcess_FlipWindingOrder;

				ImGui::SameLine();
	            if (ImGui::Button("Create##Create3")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)		

					auto handle = m_registry.Insert( vve::Position{ { x, y, 0.5f } }, vve::Rotation{mat3_t{1.0f}}, vve::Scale{vec3_t{1.0f}});
					m_handles.push( handle );

					m_engine.SendMessage( 
						MsgSceneCreate{
							vve::ObjectHandle( handle ), 
							vve::ParentHandle{}, 
							vve::Filename{path_obj3},
							flags });
					x += 2.0f;		
				}
			}

			{
				if (ImGui::Button("Erase one ##Erase1")) {
					if( !m_handles.empty() ) {
						auto handle = m_handles.top();
						m_engine.SendMessage(MsgObjectDestroy(vve::ObjectHandle(handle)));
						m_handles.pop();
						x -= 2.0f;
					}
				}
			}

            ImGui::End();
        }
		
		return false;
    }


    bool OnKeyDown(Message message) {
        //std::cout << "Key down: " << message.template GetData<MsgKeyDown>().m_key << std::endl;
		return false;
    }

    bool OnKeyUp(Message message) {
        //std::cout << "Key up: " << message.template GetData<MsgKeyUp>().m_key << std::endl;
		return false;
    }

    bool OnKeyRepeat(Message message) {
        //std::cout << "Key repeat: " << message.template GetData<MsgKeyRepeat>().m_key << std::endl;
		return false;
    }

	private:
		vecs::Handle m_planeHandle{};
		std::stack<vecs::Handle> m_handles; //queue scenes here
};



int main() {

    vve::Engine engine("My Engine") ;

	MyGUI mygui{engine};

	engine.Init();
	engine.PrintCallbacks();

    engine.Run();

    return 0;
}

