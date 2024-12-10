#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindow.h"
#include "L2DFileDialog.h"
#include "VEGUI.h"

namespace vve {

	template<ArchitectureType ATYPE>
	GUI<ATYPE>::GUI(std::string systemName, Engine<ATYPE>* engine ) : System<ATYPE>(systemName, engine) {
		m_engine->RegisterCallback( { 
			  {this, -10000, vve::MsgType::RENDER_NEXT_FRAME, [this](vve::Message message){this->OnRenderNextFrame(message);} }
		} );
	};

	template<ArchitectureType ATYPE>
    void GUI<ATYPE>::OnRenderNextFrame(vve::Message message) {
        if( m_engine->GetMainWindow()->GetIsMinimized()) { return; }

        {
            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

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
			}

            ImGui::End();
        }

    }

	template class GUI<ENGINETYPE_SEQUENTIAL>;
	template class GUI<ENGINETYPE_PARALLEL>;

};  // namespace vve

