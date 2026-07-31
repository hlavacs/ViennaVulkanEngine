/**
 * @file vrt_renderer.h
 * @brief Main ray traced renderer system.
 */

namespace vve {
	/** Top-level renderer that owns Vulkan objects and render systems. */
	class RendererRayTraced : public System {
		friend class Engine;
	public:
		/**
		 * @param systemName System identifier.
		 * @param engine Engine reference for messaging.
		 * @param windowName Window title/name.
		 */
		RendererRayTraced(std::string systemName, Engine& engine, std::string windowName);
		/** Release owned Vulkan resources. */
		~RendererRayTraced();
	private:

		/**
		 * Initialize Vulkan and rendering systems.
		 * @param message Message payload.
		 */
		bool OnInit(Message message);
		/**
		 * Prepare per-frame data.
		 * @param message Message payload.
		 */
		bool OnPrepareNextFrame(Message message);
		/**
		 * Record command buffers for the frame.
		 * @param message Message payload.
		 */
		bool OnRecordNextFrame(Message message);
		/**
		 * Submit and present the frame.
		 * @param message Message payload.
		 */
		bool OnRenderNextFrame(Message message);
		/**
		 * Cleanup and shutdown.
		 * @param message Message payload.
		 */
		bool OnQuit(Message message);

		/** Create Vulkan instance and debug layers. */
		void createInstance();
		/** Select a physical device. */
		void pickPhysicalDevice();
		/** Create the logical device and queues. */
		void createLogicalDevice();

		PerFrameDescriptorPlacment* getUniformBufferDescriptorInput(int binding, VkShaderStageFlags stageFlags);
		PerFrameDescriptorPlacment* getBidirectionalUniformBufferDescriptorInput(int binding, VkShaderStageFlags stageFlags);

		void createCommonDescriptors();
		void createRtDescriptors();
		void createBidirectionalDescriptors();
		void createRtTargetsDescriptors();

		void createBidirectionalTargetsDescriptors();
		void createLightVertexGenerationDescriptors();

		void createReductionDescriptors();

		void createCombinePassDescriptors();
		void createReprojectPassDescriptors();
		void createRestirTemporalDescriptors();
		void createRestirSpatialDescriptors();

		void createRestirGITemporalDescriptors();
		void createRestirGISpatialDescriptors();

		void createRestirLVCTemporalDescriptors();
		void createRestirLVCSpatialDescriptors();

		void createRestirLVCTemporalDescriptorsCombined();
		void createRestirLVCSpatialDescriptorsCombined();

		void createRenderTargetSampler();



		/**
		 * Update per-frame uniform buffer data.
		 * @param currentImage Frame image index.
		 */
		void updateUniformBuffer(uint32_t currentImage);
		/** Handle window resize and swapchain recreation. */
		void resizeWindow();


		VkRenderPass imguiRenderPass = VK_NULL_HANDLE;

		std::string m_windowName;
		vecs::Ref<WindowState> 		m_windowState{};
		vecs::Ref<WindowSDLState> 	m_windowSDLState{};

		const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME
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
		uint32_t nextFrame = 1;
		bool framebufferResized = false;

		DescriptorManager* commonDescriptors;
		DescriptorManager* rtDescriptors;
		DescriptorManager* BidirectionalDescriptors;
		DescriptorManager* rtTargetsDescriptors;
		DescriptorManager* combinePassDescriptors;
		DescriptorManager* reprojectionPassDescriptors;

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

		RenderTarget* lightingPreviousTarget;
		RenderTarget* lightingReprojectedTarget;
		RenderTarget* positionPreviousTarget;
		RenderTarget* reprojectionErrorTarget;
		RenderTarget* positionReprojectedTarget;

		PipelineFilter* reprojectionPass;

		RenderTarget* combinedTarget;
		RenderTarget* accumulatedLightingTarget;
		PipelineFilter* combinePass;


		//reduction piplines
		PipelineFilter* importanceReductionPass;
		PipelineFilter* keepProbReductionPass;

		PipelineFilter* sumResetPass;

		RenderTargetBufferDebug<glm::vec4>* importanceSum;
		RenderTargetBufferDebug<glm::vec4>* keepProbSum;

		DescriptorManager* reductionDescriptors;




		RenderTarget* albedoTarget;
		RenderTarget* normalTarget;
		RenderTarget* specTarget;
		RenderTarget* positionTarget;
		RenderTarget* shadingNormalTarget;

		RenderTargetBuffer<ReservoirDI>* reservoirDI_A;
		RenderTargetBuffer<ReservoirDI>* reservoirDI_B;

		RenderTargetBuffer<ReservoirGI>* reservoirGI_A;
		RenderTargetBuffer<ReservoirGI>* reservoirGI_B;

		RenderTargetBuffer<ReservoirLVC>* reservoirLVC_A;
		RenderTargetBuffer<ReservoirLVC>* reservoirLVC_B;



		VkExtent2D lightVertexCacheSize;
		RenderTargetBuffer<LightVertex>* lightVertexCache;

		PiplineRaytraced* restir_temporal;
		PiplineRaytraced* restir_spatial;

		PiplineRaytraced* restirGI_temporal;
		PiplineRaytraced* restirGI_spatial;

		PiplineRaytraced* restirLVC_temporal;
		PiplineRaytraced* restirLVC_spatial;

		PiplineRaytraced* restirLVC_temporal_combined;
		PiplineRaytraced* restirLVC_spatial_combined;

		PiplineRaytraced* lightVertexGenerationFull;
		PiplineRaytraced* lightVertexGenerationRandomReplacment;
		PiplineRaytraced* lightVertexGenerationWeightedReplacment;
		PiplineRaytraced* bidirectionalPathTracing;

		DescriptorManager* lightVertexGenerationDescriptors;
		DescriptorManager* bidirectionalTargetDescriptors;

		DescriptorManager* restir_temporal_descriptors;
		DescriptorManager* restir_spatial_descriptors;

		DescriptorManager* restirGI_temporal_descriptors;
		DescriptorManager* restirGI_spatial_descriptors;

		DescriptorManager* restirLVC_temporal_descriptors;
		DescriptorManager* restirLVC_spatial_descriptors;

		DescriptorManager* restirLVC_temporal_descriptors_combined;
		DescriptorManager* restirLVC_spatial_descriptors_combined;


		VkExtent2D vplCacheSize;
		RenderTargetBuffer<VPL>* vplCache;
		RenderTargetBuffer<VPLShading>* vplCacheShading;
		DescriptorManager* vplGenerationDescriptors;
		DescriptorManager* instantRadiosityDescriptors;
		std::vector<HostBuffer<InstantRadiosityUniforms>*> instantRadiosityUniformsBuffer;
		void createVPLGenerationDescriptors();
		void createInstantRadiosityDescriptors();
		PerFrameDescriptorPlacment* getInstantRadiosityUniformBufferDescriptorInput(int binding, VkShaderStageFlags stageFlags);

		PiplineRaytraced* vplGenerationRandomReplacment;
		PiplineRaytraced* restir_IR_temporal;
		PiplineRaytraced* restir_IR_spatial;

		PiplineRaytraced* InstantRadiosityTesting;

		PiplineRaytraced* InstantRadiosityNaiveSampling;


		//pdf Estimation
		RenderTargetBuffer<uint32_t>* sort_elements_A;
		RenderTargetBuffer<uint32_t>* sort_elements_B;

		RenderTargetBuffer<uint32_t>* sort_indices_A;
		RenderTargetBuffer<uint32_t>* sort_indices_B;

		RenderTargetBuffer<uint32_t>* sort_histogram;

		DescriptorManager* mortonCodeDescriptors;

		DescriptorManager* sortDescriptorsAB;
		DescriptorManager* sortDescriptorsBA;

		DescriptorManager* histogramDescriptorsAB;
		DescriptorManager* histogramDescriptorsBA;

		DescriptorManager* pdfEstimationDescriptors;

		void createMortonCodeDescriptors();

		void createSortDescriptorsAB();
		void createSortDescriptorsBA();

		void createHistogramDescriptorsAB();
		void createHistogramDescriptorsBA();

		void createPdfEstimationDescriptors();

		PipelineFilter* mortonCode;

		PipelineFilter* sortPiplineAB;
		PipelineFilter* sortPiplineBA;

		PipelineFilter* histogramPiplineAB;
		PipelineFilter* histogramPiplineBA;

		PipelineFilter* pdfEstimation;

		const uint32_t NUM_BLOCKS_PER_WORKGROUP = 32;

		PushConstantsSort pushConstants1;
		PushConstantsSort pushConstants2;
		PushConstantsSort pushConstants3;
		PushConstantsSort pushConstants4;



		std::vector<RenderTarget*> allTargets;
		std::vector<RenderTarget*> rayTracingTargets;


		VkSampler targetSampler{};

		UniformBufferObject uniforms;

		//using int here instead of bool for alighnment reasons
		int isFirstFrame = 1;

		std::vector<HostBuffer<UniformBufferObject>*> uniformBuffer_c;

		std::vector<HostBuffer<BidirectionalUniforms>*> bidirectionalUniformsBuffer;

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
		VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProperties{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };


		vecs::Ref<VulkanState> m_vkState{};
		vecs::Handle m_vulkanStateHandle{};

		vecs::Ref<vvh::VRTSettings> m_renderSettings{};
		vecs::Handle m_renderSettingsHandle{};

		std::mt19937 gen;
		std::uniform_int_distribution<uint32_t> dist;
	};
};
