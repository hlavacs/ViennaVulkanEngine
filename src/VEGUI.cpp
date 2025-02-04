#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	GUI::GUI(std::string systemName, Engine& engine, std::string windowName ) : 
		System(systemName, engine), m_windowName(windowName) {
		m_engine.RegisterCallback( { 
 		  	{this,    0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} }, 
		  	{this, 1000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_KEY_DOWN", [this](Message& message){ return OnKeyDown(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_KEY_REPEAT", [this](Message& message){ return OnKeyDown(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_KEY_UP", [this](Message& message){ return OnKeyUp(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_BUTTON_DOWN", [this](Message& message){ return OnMouseButtonDown(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_BUTTON_UP", [this](Message& message){return OnMouseButtonUp(message);} },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_MOVE", [this](Message& message){ return OnMouseMove(message); } },
			{this, std::numeric_limits<int>::max(), "SDL_MOUSE_WHEEL", [this](Message& message){ return OnMouseWheel(message); } }
		} );
	};


    bool GUI::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == m_windowName ) {
			m_windowSDL = (WindowSDL*)msg.m_sender;
		}
		return false;
	}

    bool GUI::OnRecordNextFrame(Message message) {
        if( m_windowSDL->GetIsMinimized()) { return false; }

		/*static float x = 0.0f, y = 0.0f;

        {
            ImGui::Begin("Load Object");                          // Create a window called "Hello, world!" and append into it.

			static char* file_dialog_buffer = nullptr;
			static char path_obj[500] = "assets\\viking_room\\viking_room.obj";
			ImGui::TextUnformatted("File: ");
			ImGui::SameLine();
			ImGui::InputText("##path1", path_obj, sizeof(path_obj));
			ImGui::SameLine();
			if (ImGui::Button("Browse##path_obj")) {
			  file_dialog_buffer = path_obj;
			  FileDialog::file_dialog_open = true;
			  FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
			}
			
			if (FileDialog::file_dialog_open) {
			  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
			}

            if (ImGui::Button("Create")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)				
				m_engine.SendMessage( 
					MsgSceneCreate{
						this, 
						nullptr, 
						ObjectHandle( m_registry.Insert( Position{ { x, y, 0.0f } }, Rotation{mat3_t{1.0f}}, Scale{vec3_t{1.0f}}) ), 
						ParentHandle{}, 
						Name{path_obj} });

				x += 2.0f;
			}

			static char path_obj2[500] = "assets\\standard\\sphere.obj";
			ImGui::TextUnformatted("File: ");
			ImGui::SameLine();
			ImGui::InputText("##path2", path_obj2, sizeof(path_obj2));
			ImGui::SameLine();
			if (ImGui::Button("Browse2##path_obj2")) {
			  file_dialog_buffer = path_obj2;
			  FileDialog::file_dialog_open = true;
			  FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
			}

			if (FileDialog::file_dialog_open) {
			  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
			}

            if (ImGui::Button("Create2")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)		
				
				m_engine.SendMessage( MsgSceneLoad{ this, nullptr, Name{path_obj2} });		
				vh::Color color{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } };
				auto handle = m_registry.Insert( 
						Position{ { x, y, 0.0f } }, Rotation{mat3_t{1.0f}}, Scale{vec3_t{0.05f}}, color, MeshName{"assets\\standard\\sphere.obj\\sphere"} );

				m_engine.SendMessage(MsgObjectCreate{ this, nullptr, ObjectHandle(handle), ParentHandle{} });

				x += 2.0f;		
			}

            ImGui::End();
        }*/
		return false;
    }

	bool GUI::OnKeyDown(Message message) {
		GetCamera();

		int key;
		real_t dt;
		if(message.HasType<MsgKeyDown>()) {
			auto msg = message.template GetData<MsgKeyDown>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		} else {
			auto msg = message.template GetData<MsgKeyRepeat>();
			key = msg.m_key;
			dt = (real_t)msg.m_dt;
		}

		if( key == SDL_SCANCODE_ESCAPE  ) { m_engine.Stop(); return false; }
		if( key == SDL_SCANCODE_LSHIFT || key == SDL_SCANCODE_RSHIFT  ) { m_shiftPressed = true; return false; }

		auto [pn, rn, sn] 		 = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
	
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle = 0.0f;
		float rotSpeed = 2.0f;

		switch( key )  {
			case SDL_SCANCODE_W : { translate = mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, -1.0f); break; }
			case SDL_SCANCODE_S : { translate = mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, 1.0f); break; }
			case SDL_SCANCODE_A : { translate = mat3_t{ LtoPc } * vec3_t(-1.0f, 0.0f, 0.0f); break; }
			case SDL_SCANCODE_Q : { translate = vec3_t(0.0f, 0.0f, -1.0f); break; }
			case SDL_SCANCODE_E : { translate = vec3_t(0.0f, 0.0f, 1.0f); break; }
			case SDL_SCANCODE_D : { translate = mat3_t{ LtoPc } * vec3_t(1.0f, 0.0f, 0.0f); break; }
			case SDL_SCANCODE_LEFT : { angle = rotSpeed * (float)dt * 1.0f; axis = glm::vec3(0.0, 0.0, 1.0); break; }
			case SDL_SCANCODE_RIGHT : { angle = rotSpeed * (float)dt * -1.0f; axis = glm::vec3(0.0, 0.0, 1.0); break; }
			case SDL_SCANCODE_UP : { angle = rotSpeed * (float)dt * -1.0f; 
									 axis = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} }; break; }
			case SDL_SCANCODE_DOWN : { angle = rotSpeed * (float)dt * 1.0f;
									   axis = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} }; break; }
		}

		float speed = m_shiftPressed ? 30.0f : 3.0f; ///add the new translation vector to the previous one
		pn = pn() + translate * (real_t)dt * speed;

		///combination of yaw and pitch, both wrt to parent space
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle, axis) * mat4_t{ rc } };
		return false;
    }

	bool GUI::OnKeyUp(Message message) {
		auto msg = message.template GetData<MsgKeyUp>();
		int key = msg.m_key;
		if( key == SDL_SCANCODE_LSHIFT || key == SDL_SCANCODE_RSHIFT ) { m_shiftPressed = false; }
		return false;
	}

	bool GUI::OnMouseButtonDown(Message message) {
		auto msg = message.template GetData<MsgMouseButtonDown>();
		if(msg.m_button != SDL_BUTTON_RIGHT) return false;
 		m_mouseButtonDown = true; 
		m_x = m_y = -1; 
		return false;
	}

	bool GUI::OnMouseButtonUp(Message message) {
		auto msg = message.template GetData<MsgMouseButtonUp>();
		if(msg.m_button != SDL_BUTTON_RIGHT) return false;
		m_mouseButtonDown = false; 
		return false;
	}


	bool GUI::OnMouseMove(Message message) {
		if( m_mouseButtonDown == false ) return false;
		GetCamera();

		auto msg = message.template GetData<MsgMouseMove>();
		real_t dt = (real_t)msg.m_dt;
		if( m_x==-1 ) { m_x = msg.m_x; m_y = msg.m_y; }
		int dx = msg.m_x - m_x;
		m_x = msg.m_x;
		int dy = msg.m_y - m_y;
		m_y = msg.m_y;

		auto [pn, rn, sn] = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		
		vec3_t translate = vec3_t(0.0f, 0.0f, 0.0f); //total translation
		vec3_t axis1 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle1 = 0.0f;
		vec3_t axis2 = vec3_t(1.0f); //total rotation around the axes, is 4d !
		float angle2 = 0.0f;
		float rotSpeed = 1.0f;

		//std::cout << "Mouse move: " << dx << ", " << dy << std::endl;

		angle1 = rotSpeed * (float)dt * dx; //left right
		axis1 = glm::vec3(0.0, 0.0, 1.0);
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle1, axis1) * mat4_t{ rc } };

		angle2 = rotSpeed * (float)dt * -dy; //up down
		axis2 = vec3_t{ LtoPc() * vec4_t{1.0f, 0.0f, 0.0f, 0.0f} };
		rc = mat3_t{ glm::rotate(mat4_t{1.0f}, angle2, axis2) * mat4_t{ rc } };

		return false;
	}

	
	bool GUI::OnMouseWheel(Message message) {
		GetCamera();
		auto msg = message.template GetData<MsgMouseWheel>();
		real_t dt = (real_t)msg.m_dt;
		auto [pn, rn, sn] 		 = m_registry.template Get<Position&, Rotation&, Scale&>(m_cameraNodeHandle);
		auto [pc, rc, sc, LtoPc] = m_registry.template Get<Position&, Rotation&, Scale&, LocalToParentMatrix>(m_cameraHandle);		
		float speed = m_shiftPressed ? 2000.0f : 500.0f; ///add the new translation vector to the previous one
		pn = pn() + mat3_t{ LtoPc } * vec3_t(0.0f, 0.0f, -msg.m_y) * (real_t)dt * speed;
		return false;
	}

	void GUI::GetCamera() {
		if(m_cameraHandle.IsValid() == false) { 
			auto [handle, camera, parent] = *m_registry.GetView<vecs::Handle, Camera&, ParentHandle>().begin(); 
			m_cameraHandle = handle;
			m_cameraNodeHandle = parent;
		};
	}

};  // namespace vve

