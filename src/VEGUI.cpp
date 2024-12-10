#include "VHInclude.h"
#include "VHVulkan.h"
#include "VEGUI.h"

namespace vve {

	template<ArchitectureType ATYPE>
	GUI<ATYPE>::GUI(std::string systemName, Engine<ATYPE>* engine ) : System<ATYPE>(systemName, engine) {
		m_engine->RegisterCallback( { 
			  {this, 0, vve::MsgType::RENDER_NEXT_FRAME, [this](vve::Message message){this->OnRenderNextFrame(message);} }
		} );
	};

	template<ArchitectureType ATYPE>
    void GUI<ATYPE>::OnRenderNextFrame(vve::Message message) {
      
        {
            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            if (ImGui::Button("Load File")) {                          // Buttons return true when clicked (most widgets return true when edited/activated)
			}

            ImGui::End();
        }

    }



};  // namespace vve

