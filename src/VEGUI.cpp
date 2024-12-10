#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindow.h"
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

            if (ImGui::Button("Load File")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)
			}

            ImGui::End();
        }

    }

	template class GUI<ENGINETYPE_SEQUENTIAL>;
	template class GUI<ENGINETYPE_PARALLEL>;

};  // namespace vve

