#pragma once

#include <any>

namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Ray Tracing Renderer

	/**
     * @brief Bare Vulkan ray tracing renderer.
     *
     * This class mirrors the structure of vve::RendererVulkan but is tailored for
     * a hardware accelerated ray tracing pipeline (VK_KHR_ray_tracing_pipeline /
     * VK_KHR_acceleration_structure). It is intended as a skeleton that will, in
     * perspective, implement the following:
     *   1. Enable the ray tracing device/instance extensions.
     *   2. Build the bottom- and top-level acceleration structures (BLAS/TLAS).
     *   3. Create the ray tracing pipeline together with the shader binding table.
     *   4. Use ray tracing shaders authored in Slang (raygen/miss/closest-hit).
     */
	class RendererRaytracing : public Renderer {
	public:
		/**
         * @brief Constructor for the ray tracing renderer.
         * @param systemName Name of the system.
         * @param engine Reference to the engine.
         * @param windowName Name of the associated window.
         */
		RendererRaytracing(std::string systemName, Engine& engine, std::string windowName);

		/**
         * @brief Destructor for the ray tracing renderer.
         */
		virtual ~RendererRaytracing();

	protected:
		// --- Engine lifecycle / message callbacks (mirrors RendererVulkan) ---

		/// Append the ray tracing instance/device extensions to the requested set.
		bool OnExtensions(Message message);
		/// Initialize device resources, acceleration structures and the RT pipeline.
		bool OnInit(Message message);

		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnRenderNextFrame(Message message);

		bool OnTextureCreate(Message message);
		bool OnTextureDestroy(Message message);
		bool OnMeshCreate(Message message);
		bool OnMeshDestroy(Message message);

		bool OnQuit(Message message);

		// --- Ray tracing specific setup steps (called from OnInit) ---

		/// (1) Query ray tracing properties/features once the device is created.
		void InitRayTracingProperties();
		/// (2) Build the bottom- and top-level acceleration structures.
		void CreateAccelerationStructures();
		/// (3) Create the ray tracing pipeline and the shader binding table.
		void CreateRayTracingPipeline();
		void CreateShaderBindingTable();
		/// Storage image the ray generation shader writes to before blit/present.
		void CreateStorageImage();

		// --- Validation / extensions ---

		const std::vector<std::string> m_validationLayers = {"VK_LAYER_KHRONOS_validation"};

		std::vector<std::string> m_instanceExtensions = {
#ifdef __APPLE__
				"VK_MVK_macos_surface", ("VK_KHR_portability_enumeration")
#endif
		};

		// Device extensions required for hardware ray tracing.
		std::vector<std::string> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
													   VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
													   VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
													   VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
													   VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
													   VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
#ifdef __APPLE__
													   ,
													   VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
		};

		// --- Ray tracing pipeline properties ---
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rtPipelineFeature{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR m_accelFeature{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
		VkPhysicalDeviceShaderObjectFeaturesEXT m_shaderObjectFeatures{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT};

		// --- Acceleration structures ---
		struct AccelerationStructure {
			VkAccelerationStructureKHR m_handle{VK_NULL_HANDLE};
			VkDeviceAddress m_deviceAddress{0};
			VkBuffer m_buffer{VK_NULL_HANDLE};
			VmaAllocation m_allocation{nullptr};
		};
		std::vector<AccelerationStructure> m_bottomLevelAS;
		AccelerationStructure m_topLevelAS;

		// --- Ray tracing pipeline + shader binding table ---
		VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
		VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
		std::vector<VkDescriptorSet> m_descriptorSets;
		VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
		VkPipeline m_pipeline{VK_NULL_HANDLE};

		std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shaderGroups;

		// Shader binding table buffers (raygen / miss / hit).
		struct ShaderBindingTable {
			VkBuffer m_buffer{VK_NULL_HANDLE};
			VmaAllocation m_allocation{nullptr};
			VkStridedDeviceAddressRegionKHR m_region{};
		};
		ShaderBindingTable m_raygenSBT;
		ShaderBindingTable m_missSBT;
		ShaderBindingTable m_hitSBT;

		// Ray tracing shaders authored in Slang and compiled to SPIR-V.
		std::string m_raygenShaderPath{"shaders/Raytracing/raygen.rgen.spv"};
		std::string m_missShaderPath{"shaders/Raytracing/miss.rmiss.spv"};
		std::string m_closestHitShaderPath{"shaders/Raytracing/closesthit.rchit.spv"};

		// --- Storage image written by the ray generation shader ---
		vvh::Image m_storageImage;

		// --- Command + synchronization (mirrors RendererVulkan) ---
		VkCommandPool m_commandPool{VK_NULL_HANDLE};
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
		std::vector<vvh::Semaphores> m_intermediateSemaphores;
		std::vector<VkFence> m_fences;
	};

}; // namespace vve
