#pragma once


namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

   	template<ArchitectureType ATYPE>
    class Vulkan : public System<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using System<ATYPE>::m_registry;

    public:

        Vulkan(std::string systemName, Engine<ATYPE>& engine );
        virtual ~Vulkan();

        auto GetInstance() -> VkInstance { return m_instance; }
		auto GetPhysicalDevice() -> VkPhysicalDevice { return m_physicalDevice; }
		auto GetDevice() -> VkDevice { return m_device; }
		auto GetQueueFamilies() -> vh::QueueFamilyIndices { return m_queueFamilies; }
		auto GetGraphicsQueue() -> VkQueue { return m_graphicsQueue; }
		auto GetPresentQueue() -> VkQueue { return m_presentQueue; }
		auto GetVmaAllocator() -> VmaAllocator { return m_vmaAllocator; }

    private:
		virtual void OnAnnounce(Message message);
		virtual void OnExtensions(Message message);
        virtual void OnInit(Message message);
        virtual void OnQuit(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

		std::vector<const char*> m_instanceExtensions;

        std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

		VmaAllocator m_vmaAllocator;
        VkInstance m_instance;
	    VkDebugUtilsMessengerEXT m_debugMessenger;
	    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	    VkDevice m_device;
	    vh::QueueFamilyIndices m_queueFamilies;
	    VkQueue m_graphicsQueue;
	    VkQueue m_presentQueue;
    };


};   // namespace vve

