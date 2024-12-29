#pragma once


namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

   	template<ArchitectureType ATYPE>
    class Vulkan : public System<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using System<ATYPE>::m_registry;
		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgAnnounce;
		using typename System<ATYPE>::MsgExtensions;

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
		void OnExtensions(Message message);
        void OnInit(Message message);
        void OnQuit(Message message);

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

