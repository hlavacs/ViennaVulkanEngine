#include "VHInclude.h"
#include "VEInclude.h"
#include "L2DFileDialog.h"

namespace vve {

	GUI::GUI(std::string systemName, Engine& engine, std::string windowName ) : 
		System(systemName, engine), m_windowName(windowName) {
		m_engine.RegisterCallback( { 
 		  {this,    0, "ANNOUNCE", [this](Message message){ return OnAnnounce(message);} }, 
		  {this, 1000, "RECORD_NEXT_FRAME", [this](Message message){ return OnRecordNextFrame(message);} }
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

        {
            ImGui::Begin("Load Object");                          // Create a window called "Hello, world!" and append into it.

			static char* file_dialog_buffer = nullptr;
			static char path_obj[500] = "assets\\models\\viking_room.obj";
			ImGui::TextUnformatted("OBJ File: ");
			ImGui::SameLine();
			ImGui::InputText("##path", path_obj, sizeof(path_obj));
			ImGui::SameLine();
			if (ImGui::Button("Browse##path_obj")) {
			  file_dialog_buffer = path_obj;
			  FileDialog::file_dialog_open = true;
			  FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
			}

			static char path_texture[500] = "assets\\textures\\viking_room.png";
			ImGui::TextUnformatted("Txt File: ");
			ImGui::SameLine();
			ImGui::InputText("##path", path_texture, sizeof(path_obj));
			ImGui::SameLine();
			if (ImGui::Button("Browse##path_texture")) {
			  file_dialog_buffer = path_texture;
			  FileDialog::file_dialog_open = true;
			  FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
			}

			if (FileDialog::file_dialog_open) {
			  FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, file_dialog_buffer, sizeof(file_dialog_buffer), FileDialog::file_dialog_open_type);
			}

            if (ImGui::Button("Load")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)
				static float x = 0.0f;
				
				auto handle = m_registry.Insert( Position{vec3_t(0.0f, x, 0.0f)}, 
								   Rotation{mat3_t{1.0f}},
								   Scale{vec3_t{1.0f, 1.0f, 1.0f}} );

				m_engine.SendMessage( MsgLoadObject{this, nullptr, handle, {}, path_texture, path_obj} );
				x += 2.0f;
			}

            ImGui::End();
        }
		return false;
    }


};  // namespace vve

