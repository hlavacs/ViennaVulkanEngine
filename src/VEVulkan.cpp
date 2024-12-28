#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    template<ArchitectureType ATYPE>
    Vulkan<ATYPE>::Vulkan(std::string systemName, Engine<ATYPE>& engine ) : System<ATYPE>(systemName, engine) {

        engine.RegisterCallback( { 
			{this,  -2000, "INIT", [this](Message message){this->OnInit(message);} }, 
			{this,  -1000, "QUIT", [this](Message message){this->OnQuit(message);} },
		} );
    }

    template<ArchitectureType ATYPE>
    Vulkan<ATYPE>::~Vulkan() {}

    template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnInit(Message message) {

    	/*m_windowSDL = (WindowSDL<ATYPE>*)m_window;
		
		auto instanceExtensions = m_windowSDL->GetInstanceExtensions();
		
		if (m_engine.GetDebug()) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::createInstance( m_validationLayers, instanceExtensions, m_instance);
		if (m_engine.GetDebug()) {
	        vh::setupDebugMessenger(m_instance, m_debugMessenger);
		}
		*/

    }


	template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnQuit(Message message) {
        //vkDestroyInstance(m_instance, nullptr);
	}

    template class Vulkan<ENGINETYPE_SEQUENTIAL>;
    template class Vulkan<ENGINETYPE_PARALLEL>;

};   // namespace vve


