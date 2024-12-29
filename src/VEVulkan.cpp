#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    template<ArchitectureType ATYPE>
    Vulkan<ATYPE>::Vulkan(std::string systemName, Engine<ATYPE>& engine ) : System<ATYPE>(systemName, engine) {

        engine.RegisterCallback( { 
			{this,      0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} }, 
			{this,      0, "EXTENSIONS", [this](Message message){this->OnExtensions(message);} }, 
			{this,  -3000, "INIT", [this](Message message){this->OnInit(message);} }, 
			{this,   4000, "QUIT", [this](Message message){this->OnQuit(message);} },
		} );
    }

    template<ArchitectureType ATYPE>
    Vulkan<ATYPE>::~Vulkan() {}

    template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
	}

    template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
	}

    template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnInit(Message message) {
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::createInstance( m_validationLayers, m_instanceExtensions, m_instance);
		if (m_engine.GetDebug()) {
	        vh::setupDebugMessenger(m_instance, m_debugMessenger);
		}

        //vh::pickPhysicalDevice(m_instance, m_deviceExtensions, m_window->GetSurface(), m_physicalDevice);
        
		//vh::createLogicalDevice(m_window->GetSurface(), m_physicalDevice, m_queueFamilies, m_validationLayers, m_deviceExtensions, m_device, m_graphicsQueue, m_presentQueue);
        
		//vh::initVMA(m_instance, m_physicalDevice, m_device, m_vmaAllocator);  
        
		m_engine.SendMessage( MsgAnnounce{this} );
    }

	template<ArchitectureType ATYPE>
    void Vulkan<ATYPE>::OnQuit(Message message) {

        //vmaDestroyAllocator(m_vmaAllocator);

        //vkDestroyDevice(m_device, nullptr);

		if (m_engine.GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);
	}

    template class Vulkan<ENGINETYPE_SEQUENTIAL>;
    template class Vulkan<ENGINETYPE_PARALLEL>;

};   // namespace vve


