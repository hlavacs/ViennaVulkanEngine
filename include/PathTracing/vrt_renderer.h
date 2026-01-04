namespace vve {
	class RendererRayTraced : public System {
		friend class Engine;
	public:
		RendererRayTraced(std::string systemName, Engine& engine, std::string windowName);
		~RendererRayTraced();
	private:

		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnRenderNextFrame(Message message);
		bool OnQuit(Message message);

		void createInstance();
		void pickPhysicalDevice();
		void createLogicalDevice();

		void UpdateGeneralDescriptors();
		void UpdateRayTracingDescriptors();

		void updateUniformBuffer(uint32_t currentImage);
		void resizeWindow();


		std::string m_windowName;
		vecs::Ref<WindowState> 		m_windowState{};
		vecs::Ref<WindowSDLState> 	m_windowSDLState{};

		const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
		};

		vkb::Instance vkbInstance;
		vkb::PhysicalDevice vkbphysicalDevice;
		vkb::Device vkbDevice;

		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;
		VkSurfaceKHR surface;
		VkPhysicalDevice physicalDevice;
		VkDevice device;
		VkQueue graphicsQueue;
		VkQueue presentQueue;

		uint32_t graphicsQueueIndex;
		uint32_t presentQueueIndex;

		uint32_t currentFrame = 0;
		bool framebufferResized = false;

		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorPool descriptorPoolRT;
		std::vector<VkDescriptorSet> descriptorSetsRT;
		VkDescriptorSetLayout descriptorSetLayoutRT;

		VkDescriptorPool descriptorPoolTargets;
		std::vector<VkDescriptorSet> descriptorSetsTargets;
		VkDescriptorSetLayout descriptorSetLayoutTargets;

		TextureManager* textureManager;
		ObjectManager* objectManager;
		MaterialManager* materialManager;
		LightManager* lightManager;

		CommandManager* commandManager;
		SwapChain* swapchain;
		PiplineRasterized* rasterizer;
		RenderTarget* mainTarget;
		RenderTarget* depthTarget;
		RenderTarget* RtTarget;
		PiplineRaytraced* raytracer;


		RenderTarget* albedoTarget;
		RenderTarget* normalTarget;
		RenderTarget* specTarget;
		RenderTarget* positionTarget;

		std::vector<RenderTarget*> allTargets;
		std::vector<RenderTarget*> rayTracingTargets;

		std::vector<HostBuffer<UniformBufferObject>*> uniformBuffer_c;

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
		VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

		bool generalDiscriptorsCreated = false;
		bool raytracingDiscriptorsCreated = false;


		vecs::Ref<VulkanState> m_vkState{};
		vecs::Handle m_vulkanStateHandle{};

		std::mt19937 gen;
		std::uniform_int_distribution<uint32_t> dist;
	};
};