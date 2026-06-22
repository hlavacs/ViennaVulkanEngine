#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Ray Tracing Renderer
	//
	// This renderer targets a hardware-accelerated ray tracing pipeline (VK_KHR_ray_tracing_pipeline /
	// VK_KHR_acceleration_structure). The sequence is:
	//   1. Enable ray tracing extensions          -> OnExtensions / m_deviceExtensions
	//   2. Build acceleration structures           -> CreateAccelerationStructures / OnMeshCreate
	//   3. Create ray tracing pipeline + SBT        -> CreateRayTracingPipeline / CreateShaderBindingTable
	//   4. Use ray tracing shaders written in Slang -> m_raygen/miss/closestHitShaderPath

	namespace {
		// Round 'value' up to the next multiple of 'alignment'.
		inline VkDeviceSize AlignUp(const VkDeviceSize value, const VkDeviceSize alignment) {
			return (value + alignment - 1) & ~(alignment - 1);
		}
	} // namespace

	/**
	 * @brief Constructs the ray tracing renderer and registers event callbacks.
	 */
	RendererRaytracing::RendererRaytracing(std::string systemName, Engine& engine, std::string windowName) :
			Renderer(systemName, engine, windowName) {
		engine.RegisterCallbacks({
				{this, 0, "EXTENSIONS", [this](Message& message) { return OnExtensions(message); }},
				{this, 1000, "INIT", [this](Message& message) { return OnInit(message); }},
				{this, 0, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); }},
				{this, 0, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); }},
				{this, 0, "RENDER_NEXT_FRAME", [this](Message& message) { return OnRenderNextFrame(message); }},
				{this, 1000, "TEXTURE_CREATE", [this](Message& message) { return OnTextureCreate(message); }},
				{this, 0, "TEXTURE_DESTROY", [this](Message& message) { return OnTextureDestroy(message); }},
				{this, 0, "MESH_CREATE", [this](Message& message) { return OnMeshCreate(message); }},
				{this, 0, "MESH_DESTROY", [this](Message& message) { return OnMeshDestroy(message); }},
				{this, 2000, "QUIT", [this](Message& message) { return OnQuit(message); }},
		});
	}

	/**
	 * @brief Destructor for the ray tracing renderer.
	 */
	RendererRaytracing::~RendererRaytracing() {}

	/**
	 * @brief (1) Append the ray tracing instance/device extensions to the requested set.
	 */
	bool RendererRaytracing::OnExtensions(Message message) {
		auto msg = message.GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

	/**
	 * @brief Initializes the ray tracing renderer: device, acceleration structures and RT pipeline.
	 */
	bool RendererRaytracing::OnInit(Message message) {
		Renderer::OnInit(message);

		// Instance / surface / physical-device / logical-device / VMA / swap chain.
		CreateDeviceAndSwapChain();

		// (1) Query ray tracing capabilities of the selected device.
		InitRayTracingProperties();

		// (2) Build bottom- and top-level acceleration structures from loaded meshes.
		CreateAccelerationStructures();

		// (3) Create the ray tracing pipeline and the shader binding table.
		CreateRayTracingPipeline();
		CreateShaderBindingTable();

		// Storage image the raygen shader writes into before presenting.
		CreateStorageImage();

		UpdateDescriptorSets();
		return false;
	}

	/**
	 * @brief Creates the instance, surface, device, VMA allocator, and swap chain.
	 *
	 * The logical device and the VMA allocator
	 * are created by hand here so that the ray tracing / acceleration structure /
	 * buffer-device-address features can be enabled.
	 */
	void RendererRaytracing::CreateDeviceAndSwapChain() {
		auto engineState = m_engine.GetState();

		if( engineState.m_debug ) {
			m_instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		volkInitialize();
		m_vkState().m_apiVersionInstance = engineState.m_apiVersion;
		vvh::DevCreateInstance({.m_validationLayers = m_validationLayers,
								.m_instanceExtensions = m_instanceExtensions,
								.m_name = engineState.m_name,
								.m_apiVersion = engineState.m_apiVersion,
								.m_debug = engineState.m_debug,
								.m_instance = m_vkState().m_instance});

		volkLoadInstance(m_vkState().m_instance);

		if( engineState.m_debug ) {
			vvh::DevSetupDebugMessenger(m_vkState().m_instance, m_vkState().m_debugMessenger);
		}

		if( SDL_Vulkan_CreateSurface(
					m_windowSDLState().m_sdlWindow, m_vkState().m_instance, nullptr, &m_vkState().m_surface) == 0 ) {
			printf("Failed to create Vulkan surface.\n");
		}

		m_vkState().m_apiVersionDevice = engineState.m_minimumVersion;
		vvh::DevPickPhysicalDevice({.m_instance = m_vkState().m_instance,
									.m_deviceExtensions = m_deviceExtensions,
									.m_surface = m_vkState().m_surface,
									.m_apiVersion = m_vkState().m_apiVersionDevice,
									.m_physicalDevice = m_vkState().m_physicalDevice});

		vkGetPhysicalDeviceProperties(m_vkState().m_physicalDevice, &m_vkState().m_physicalDeviceProperties);
		vkGetPhysicalDeviceFeatures(m_vkState().m_physicalDevice, &m_vkState().m_physicalDeviceFeatures);

		// --- Create the logical device with the ray tracing feature chain ---
		m_vkState().m_queueFamilies = vvh::DevFindQueueFamilies(
				{.m_physicalDevice = m_vkState().m_physicalDevice, .m_surface = m_vkState().m_surface});

		std::set<uint32_t> uniqueQueueFamilies = {m_vkState().m_queueFamilies.graphicsFamily.value(),
												  m_vkState().m_queueFamilies.presentFamily.value()};

		float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		for( uint32_t queueFamily : uniqueQueueFamilies ) {
			VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
		bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

		m_accelFeature.accelerationStructure = VK_TRUE;
		m_rtPipelineFeature.rayTracingPipeline = VK_TRUE;

		// Descriptor indexing for the bindless texture array used by the hit shader.
		m_descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
		m_descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		m_descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
		m_descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;

		// Chain: features2 -> bufferDeviceAddress -> accelerationStructure ->
		//        rayTracingPipeline -> descriptorIndexing
		bufferDeviceAddressFeatures.pNext = &m_accelFeature;
		m_accelFeature.pNext = &m_rtPipelineFeature;
		m_rtPipelineFeature.pNext = &m_descriptorIndexingFeatures;
		m_descriptorIndexingFeatures.pNext = nullptr;

		VkPhysicalDeviceFeatures2 deviceFeatures2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		deviceFeatures2.features = deviceFeatures;
		deviceFeatures2.pNext = &bufferDeviceAddressFeatures;

		auto extensions = vvh::ToCharPtr(m_deviceExtensions);
		auto layers = vvh::ToCharPtr(m_validationLayers);

		VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		createInfo.pNext = &deviceFeatures2;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = nullptr;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		if( engineState.m_debug ) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
			createInfo.ppEnabledLayerNames = layers.data();
		}

		if( vkCreateDevice(m_vkState().m_physicalDevice, &createInfo, nullptr, &m_vkState().m_device) != VK_SUCCESS ) {
			throw std::runtime_error("Failed to create logical device (ray tracing)");
		}

		volkLoadDevice(m_vkState().m_device);
		vkGetDeviceQueue(m_vkState().m_device,
						 m_vkState().m_queueFamilies.graphicsFamily.value(),
						 0,
						 &m_vkState().m_graphicsQueue);
		vkGetDeviceQueue(m_vkState().m_device,
						 m_vkState().m_queueFamilies.presentFamily.value(),
						 0,
						 &m_vkState().m_presentQueue);

		// --- VMA allocator with buffer device address support ---
		VmaVulkanFunctions vulkanFunctions{};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorCreateInfo{};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		allocatorCreateInfo.vulkanApiVersion = engineState.m_apiVersion;
		allocatorCreateInfo.physicalDevice = m_vkState().m_physicalDevice;
		allocatorCreateInfo.device = m_vkState().m_device;
		allocatorCreateInfo.instance = m_vkState().m_instance;
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
		vmaCreateAllocator(&allocatorCreateInfo, &m_vkState().m_vmaAllocator);

		// --- Swap chain + image views ---
		vvh::DevCreateSwapChain({m_windowSDLState().m_sdlWindow,
								 m_vkState().m_surface,
								 m_vkState().m_physicalDevice,
								 m_vkState().m_device,
								 m_vkState().m_swapChain,
								 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT});
		vvh::DevCreateImageViews({m_vkState().m_device, m_vkState().m_swapChain});

		// --- Command pools + command buffers ---
		vvh::ComCreateCommandPool({.m_surface = m_vkState().m_surface,
								   .m_physicalDevice = m_vkState().m_physicalDevice,
								   .m_device = m_vkState().m_device,
								   .m_queueFamilyIndex = m_vkState().m_queueFamilies.graphicsFamily.value(),
								   .m_commandPool = m_vkState().m_commandPool});
		vvh::ComCreateCommandPool({.m_surface = m_vkState().m_surface,
								   .m_physicalDevice = m_vkState().m_physicalDevice,
								   .m_device = m_vkState().m_device,
								   .m_queueFamilyIndex = m_vkState().m_queueFamilies.graphicsFamily.value(),
								   .m_commandPool = m_commandPool});

		vvh::ComCreateCommandBuffers({.m_device = m_vkState().m_device,
									  .m_commandPool = m_commandPool,
									  .m_commandBuffers = m_commandBuffers});

		// --- Render pass + depth + framebuffers for the shared swap chain ---
		// These let overlay renderers (e.g., ImGui) draw into the swap chain image
		// after the ray tracing result has been blitted into it.
		m_vkState().m_depthMapFormat = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice);

		vvh::RenCreateRenderPass({.m_depthFormat = m_vkState().m_depthMapFormat,
								  .m_device = m_vkState().m_device,
								  .m_swapChain = m_vkState().m_swapChain,
								  .m_clear = false,
								  .m_renderPass = m_renderPass});

		vvh::RenCreateDepthResources({.m_physicalDevice = m_vkState().m_physicalDevice,
									  .m_device = m_vkState().m_device,
									  .m_vmaAllocator = m_vkState().m_vmaAllocator,
									  .m_swapChain = m_vkState().m_swapChain,
									  .m_depthImage = m_vkState().m_depthImage});

		vvh::ImgTransitionImageLayout({.m_device = m_vkState().m_device,
									   .m_graphicsQueue = m_vkState().m_graphicsQueue,
									   .m_commandPool = m_commandPool,
									   .m_image = m_vkState().m_depthImage.m_depthImage,
									   .m_format = m_vkState().m_depthMapFormat,
									   .m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
									   .m_mipLevels = 1,
									   .m_layers = 1,
									   .m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   .m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});

		vvh::RenCreateFramebuffers({.m_device = m_vkState().m_device,
									.m_depthImage = m_vkState().m_depthImage,
									.m_renderPass = m_renderPass,
									.m_swapChain = m_vkState().m_swapChain});

		// Bring all swap chain images into a defined (present) layout.
		for( auto image : m_vkState().m_swapChain.m_swapChainImages ) {
			vvh::ImgTransitionImageLayout2({.m_device = m_vkState().m_device,
											.m_graphicsQueue = m_vkState().m_graphicsQueue,
											.m_commandPool = m_commandPool,
											.m_image = image,
											.m_format = m_vkState().m_swapChain.m_swapChainImageFormat,
											.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
											.m_newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
		}

		vvh::SynCreateSemaphores({.m_device = m_vkState().m_device,
								  .m_imageAvailableSemaphores = m_imageAvailableSemaphores,
								  .m_renderFinishedSemaphores = m_renderFinishedSemaphores,
								  .m_size = 3,
								  .m_intermediateSemaphores = m_intermediateSemaphores});
		vvh::SynCreateFences({.m_device = m_vkState().m_device, .m_size = MAX_FRAMES_IN_FLIGHT, .m_fences = m_fences});

		// Camera uniform buffer (view / projection inverse) used by the raygen shader.
		VkDeviceSize uboSize = sizeof(vvh::rt::CameraRT);
		vvh::BufCreateBuffers({.m_device = m_vkState().m_device,
							   .m_vmaAllocator = m_vkState().m_vmaAllocator,
							   .m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							   .m_size = uboSize,
							   .m_buffer = m_uniformBuffer});

		// Per-instance material/geometry SSBO read by the closest-hit shader.
		vvh::BufCreateBuffers({.m_device = m_vkState().m_device,
							   .m_vmaAllocator = m_vkState().m_vmaAllocator,
							   .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
							   .m_size = MAX_RT_INSTANCES * sizeof(vvh::rt::InstanceDataGpu),
							   .m_buffer = m_instanceDataBuffer});

		// Scene lights SSBO read by the closest-hit shader.
		vvh::BufCreateBuffers({.m_device = m_vkState().m_device,
							   .m_vmaAllocator = m_vkState().m_vmaAllocator,
							   .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
							   .m_size = MAX_NUMBER_LIGHTS * sizeof(vvh::Light),
							   .m_buffer = m_lightsBuffer});
	}

	/**
	 * @brief Recreates the swap chain and the matching storage image after a resize.
	 */
	void RendererRaytracing::RecreateSwapChain() {
		int width = 0, height = 0;
		SDL_GetWindowSize(m_windowSDLState().m_sdlWindow, &width, &height);
		while( width == 0 || height == 0 ) {
			SDL_Event event;
			SDL_WaitEvent(&event);
			SDL_GetWindowSize(m_windowSDLState().m_sdlWindow, &width, &height);
		}

		vkDeviceWaitIdle(m_vkState().m_device);

		// Destroy the framebuffers and depth image that depend on the old swap chain.
		for( auto framebuffer : m_vkState().m_swapChain.m_swapChainFramebuffers ) {
			vkDestroyFramebuffer(m_vkState().m_device, framebuffer, nullptr);
		}
		m_vkState().m_swapChain.m_swapChainFramebuffers.clear();

		vkDestroyImageView(m_vkState().m_device, m_vkState().m_depthImage.m_depthImageView, nullptr);
		vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_image = m_vkState().m_depthImage.m_depthImage,
							  .m_imageAllocation = m_vkState().m_depthImage.m_depthImageAllocation});

		for( const auto imageView : m_vkState().m_swapChain.m_swapChainImageViews ) {
			vkDestroyImageView(m_vkState().m_device, imageView, nullptr);
		}
		vkDestroySwapchainKHR(m_vkState().m_device, m_vkState().m_swapChain.m_swapChain, nullptr);

		vvh::DevCreateSwapChain({m_windowSDLState().m_sdlWindow,
								 m_vkState().m_surface,
								 m_vkState().m_physicalDevice,
								 m_vkState().m_device,
								 m_vkState().m_swapChain,
								 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT});
		vvh::DevCreateImageViews({m_vkState().m_device, m_vkState().m_swapChain});

		for( auto image : m_vkState().m_swapChain.m_swapChainImages ) {
			vvh::ImgTransitionImageLayout2({.m_device = m_vkState().m_device,
											.m_graphicsQueue = m_vkState().m_graphicsQueue,
											.m_commandPool = m_commandPool,
											.m_image = image,
											.m_format = m_vkState().m_swapChain.m_swapChainImageFormat,
											.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
											.m_newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
		}

		// Recreate the depth image + framebuffers for the new swap chain extent.
		vvh::RenCreateDepthResources({.m_physicalDevice = m_vkState().m_physicalDevice,
									  .m_device = m_vkState().m_device,
									  .m_vmaAllocator = m_vkState().m_vmaAllocator,
									  .m_swapChain = m_vkState().m_swapChain,
									  .m_depthImage = m_vkState().m_depthImage});

		vvh::ImgTransitionImageLayout({.m_device = m_vkState().m_device,
									   .m_graphicsQueue = m_vkState().m_graphicsQueue,
									   .m_commandPool = m_commandPool,
									   .m_image = m_vkState().m_depthImage.m_depthImage,
									   .m_format = m_vkState().m_depthMapFormat,
									   .m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
									   .m_mipLevels = 1,
									   .m_layers = 1,
									   .m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   .m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});

		vvh::RenCreateFramebuffers({.m_device = m_vkState().m_device,
									.m_depthImage = m_vkState().m_depthImage,
									.m_renderPass = m_renderPass,
									.m_swapChain = m_vkState().m_swapChain});

		// The storage image must match the new swap chain extent.
		vkDestroyImageView(m_vkState().m_device, m_storageImage.m_mapImageView, nullptr);
		vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_image = m_storageImage.m_mapImage,
							  .m_imageAllocation = m_storageImage.m_mapImageAllocation});
		CreateStorageImage();
		UpdateDescriptorSets();
	}

	/**
	 * @brief (1) Query ray tracing pipeline properties and acceleration structure features.
	 */
	void RendererRaytracing::InitRayTracingProperties() {
		VkPhysicalDeviceProperties2 deviceProperties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		deviceProperties2.pNext = &m_rtPipelineProperties;
		vkGetPhysicalDeviceProperties2(m_vkState().m_physicalDevice, &deviceProperties2);
	}

	/**
	 * @brief Returns the device address of a buffer.
	 */
	auto RendererRaytracing::GetBufferDeviceAddress(VkBuffer buffer) -> VkDeviceAddress {
		VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		info.buffer = buffer;
		return vkGetBufferDeviceAddress(m_vkState().m_device, &info);
	}

	/**
	 * @brief Allocates the device-local buffer that backs an acceleration structure.
	 */
	void RendererRaytracing::CreateAccelerationStructureBuffer(AccelerationStructure& as, const VkDeviceSize size) {
		constexpr VkBufferUsageFlags usage =
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		constexpr VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		constexpr VmaAllocationCreateFlags vmaFlags = 0;
		vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_size = size,
							  .m_usageFlags = usage,
							  .m_properties = props,
							  .m_vmaFlags = vmaFlags,
							  .m_buffer = as.m_buffer,
							  .m_allocation = as.m_allocation,
							  .m_allocationInfo = nullptr});
	}

	/**
	 * @brief (2) Build the bottom- and top-level acceleration structures.
	 *
	 * BLAS are built lazily per mesh in OnMeshCreate. At init we only (re)build the
	 * TLAS from whatever meshes already exist.
	 */
	void RendererRaytracing::CreateAccelerationStructures() {
		CreateTopLevelAS();
	}

	/**
	 * @brief Builds a bottom level acceleration structure for a single triangle mesh.
	 */
	auto RendererRaytracing::CreateBottomLevelAS(vvh::Mesh& mesh) -> AccelerationStructure {
		AccelerationStructure blas{};

		auto numVertices = static_cast<uint32_t>(mesh.m_verticesData.m_positions.size());
		auto primitiveCount = static_cast<uint32_t>(mesh.m_indices.size() / 3);

		VkDeviceOrHostAddressConstKHR vertexAddress{};
		vertexAddress.deviceAddress = GetBufferDeviceAddress(mesh.m_vertexBuffer);
		VkDeviceOrHostAddressConstKHR indexAddress{};
		indexAddress.deviceAddress = GetBufferDeviceAddress(mesh.m_indexBuffer);

		VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.vertexData = vertexAddress;
		geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
		geometry.geometry.triangles.maxVertex = numVertices > 0 ? numVertices - 1 : 0;
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geometry.geometry.triangles.indexData = indexAddress;
		geometry.geometry.triangles.transformData.deviceAddress = 0;

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		vkGetAccelerationStructureBuildSizesKHR(m_vkState().m_device,
												VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&buildInfo,
												&primitiveCount,
												&sizeInfo);

		CreateAccelerationStructureBuffer(blas, sizeInfo.accelerationStructureSize);

		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.buffer = blas.m_buffer;
		createInfo.size = sizeInfo.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(m_vkState().m_device, &createInfo, nullptr, &blas.m_handle);

		// Scratch buffer for the build.
		VkBuffer scratchBuffer{VK_NULL_HANDLE};
		VmaAllocation scratchAllocation{nullptr};
		VkBufferUsageFlags scratchUsage =
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VkMemoryPropertyFlags scratchProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VmaAllocationCreateFlags scratchVmaFlags = 0;
		vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_size = sizeInfo.buildScratchSize,
							  .m_usageFlags = scratchUsage,
							  .m_properties = scratchProps,
							  .m_vmaFlags = scratchVmaFlags,
							  .m_buffer = scratchBuffer,
							  .m_allocation = scratchAllocation,
							  .m_allocationInfo = nullptr});

		buildInfo.dstAccelerationStructure = blas.m_handle;
		buildInfo.scratchData.deviceAddress = GetBufferDeviceAddress(scratchBuffer);

		VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
		rangeInfo.primitiveCount = primitiveCount;
		rangeInfo.primitiveOffset = 0;
		rangeInfo.firstVertex = 0;
		rangeInfo.transformOffset = 0;
		const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

		VkCommandBuffer cmd = vvh::ComBeginSingleTimeCommands({m_vkState().m_device, m_commandPool});
		vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
		vvh::ComEndSingleTimeCommands({m_vkState().m_device, m_vkState().m_graphicsQueue, m_commandPool, cmd});

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
		addressInfo.accelerationStructure = blas.m_handle;
		blas.m_deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_vkState().m_device, &addressInfo);

		vvh::BufDestroyBuffer({m_vkState().m_device, m_vkState().m_vmaAllocator, scratchBuffer, scratchAllocation});
		return blas;
	}

	/**
	 * @brief (Re)builds the top level acceleration structure from all BLAS instances.
	 */
	void RendererRaytracing::CreateTopLevelAS() {
		if( m_topLevelAS.m_handle != VK_NULL_HANDLE ) {
			DestroyAccelerationStructure(m_topLevelAS);
			m_topLevelAS = {};
		}

		if( m_bottomLevelAS.empty() ) {
			return;
		}

		std::vector<VkAccelerationStructureInstanceKHR> instances;

		// Convert a column-major glm world matrix into the row-major 3x4
		// VkTransformMatrixKHR expected by the acceleration structure builder.
		auto toVkTransform = [](const glm::mat4& m) -> VkTransformMatrixKHR {
			VkTransformMatrixKHR t{};
			for( int row = 0; row < 3; ++row ) {
				for( int col = 0; col < 4; ++col ) {
					t.matrix[row][col] = m[col][row];
				}
			}
			return t;
		};

		// Per-instance material / geometry records consumed by the closest-hit
		// shader, kept in lock-step with `instances` (indexed by instanceCustomIndex).
		std::vector<vvh::rt::InstanceDataGpu> instanceData;
		m_textureInfos.clear();
		std::unordered_map<VkImageView, int32_t> textureSlots;

		// One TLAS instance per scene object: reference the BLAS built for the
		// object's mesh and place it with the object's local-to-world matrix.
		// Using identity transforms here would collapse every object onto the
		// origin (and a large ground plane would then engulf the camera).
		for( auto [oHandle, meshHandle, lToW] : m_registry.GetView<vecs::Handle, MeshHandle, LocalToWorldMatrix&>() ) {
			vecs::Handle mh = meshHandle();
			for( uint32_t i = 0; i < m_bottomLevelAS.size(); ++i ) {
				if( m_bottomLevelASMeshes[i] != mh ) {
					continue;
				}
				if( instanceData.size() >= MAX_RT_INSTANCES ) {
					break;
				}

				VkAccelerationStructureInstanceKHR instance{};
				instance.transform = toVkTransform(lToW());
				// The custom index selects the matching InstanceData record below.
				instance.instanceCustomIndex = static_cast<uint32_t>(instanceData.size());
				// Light-visualizer entities (those carrying a light component) keep
				// their marker mesh in the TLAS so primary/reflection rays still draw
				// them, but we clear the ShadowRay bit so shadow rays (traced with the
				// ShadowRay mask) skip them and the lights no longer self-occlude.
				const bool isLightMarker = m_registry.Has<PointLight>(oHandle) || m_registry.Has<SpotLight>(oHandle) ||
										   m_registry.Has<DirectionalLight>(oHandle);
				instance.mask = false ? 0xFF & ~static_cast<uint32_t>(RayMaskFlags::ShadowRay) : 0xFF;
				instance.instanceShaderBindingTableRecordOffset = 0;
				instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
				instance.accelerationStructureReference = m_bottomLevelAS[i].m_deviceAddress;
				instances.push_back(instance);

				// --- Build the per-instance material / geometry record ---
				auto mesh = m_registry.Get<vvh::Mesh&>(mh);
				const std::string type = mesh().m_verticesData.getType();
				const auto offs = mesh().m_verticesData.getOffsets();
				auto blockOffset = [&](char c) -> uint32_t {
					auto p = type.find(c);
					return p == std::string::npos ? 0u : static_cast<uint32_t>(offs[p]);
				};

				using InstanceFlags = vvh::rt::InstanceFlags;
				vvh::rt::InstanceDataGpu rec{};
				rec.vertexAddress = GetBufferDeviceAddress(mesh().m_vertexBuffer);
				rec.indexAddress = GetBufferDeviceAddress(mesh().m_indexBuffer);
				rec.normalOffset = blockOffset('N');
				rec.uvOffset = blockOffset('U');
				rec.colorOffset = blockOffset('C');
				if( type.find('N') != std::string::npos ) {
					rec.flags |= vvh::ToUnderlying(InstanceFlags::HasNormal);
				}
				if( type.find('U') != std::string::npos ) {
					rec.flags |= vvh::ToUnderlying(InstanceFlags::HasUv);
				}
				if( type.find('C') != std::string::npos ) {
					rec.flags |= vvh::ToUnderlying(InstanceFlags::HasVertexColor);
				}

				if( m_registry.Has<vvh::Color>(oHandle) ) {
					const auto& col = m_registry.Get<vvh::Color>(oHandle);
					rec.ambient = col.m_ambientColor;
					rec.diffuse = col.m_diffuseColor;
					rec.specular = col.m_specularColor;
					rec.flags |= vvh::ToUnderlying(InstanceFlags::HasMaterialColor);
					rec.reflectivity = glm::clamp(col.m_specularColor.w, 0.0f, 1.0f);
				}

				// Per-object UV tiling (matches the Forward/Deferred rasterizers).
				if( m_registry.Has<UVScale>(oHandle) ) {
					rec.uvScale = glm::vec2(m_registry.Get<UVScale>(oHandle)());
				}

				rec.textureIndex = -1;
				if( m_registry.Has<TextureHandle>(oHandle) && (rec.flags & vvh::ToUnderlying(InstanceFlags::HasUv)) ) {
					const auto& tHandle = m_registry.Get<TextureHandle>(oHandle);
					const vvh::Image& texture = m_registry.Get<vvh::Image&>(tHandle);
					if( texture.m_mapImageView != VK_NULL_HANDLE ) {
						auto it = textureSlots.find(texture.m_mapImageView);
						if( it != textureSlots.end() ) {
							rec.textureIndex = it->second;
						} else if( m_textureInfos.size() < MAX_RT_TEXTURES ) {
							rec.textureIndex = static_cast<int32_t>(m_textureInfos.size());
							textureSlots[texture.m_mapImageView] = rec.textureIndex;
							VkDescriptorImageInfo info{};
							info.sampler = texture.m_mapSampler;
							info.imageView = texture.m_mapImageView;
							info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							m_textureInfos.push_back(info);
						}
					}
				}

				instanceData.push_back(rec);
				break;
			}
		}

		if( instances.empty() ) {
			return;
		}

		// Upload the per-instance records for the closest-hit shader.
		m_instanceCount = static_cast<uint32_t>(instanceData.size());
		if( m_instanceCount > 0 && m_instanceDataBuffer.m_uniformBuffersMapped[0] != nullptr ) {
			memcpy(m_instanceDataBuffer.m_uniformBuffersMapped[0],
				   instanceData.data(),
				   instanceData.size() * sizeof(vvh::rt::InstanceDataGpu));
		}

		VkDeviceSize instancesSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

		VkBuffer instancesBuffer{VK_NULL_HANDLE};
		VmaAllocation instancesAllocation{nullptr};
		VmaAllocationInfo instancesAllocInfo{};
		VkBufferUsageFlags instUsage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
									   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VkMemoryPropertyFlags instProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VmaAllocationCreateFlags instVmaFlags =
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_size = instancesSize,
							  .m_usageFlags = instUsage,
							  .m_properties = instProps,
							  .m_vmaFlags = instVmaFlags,
							  .m_buffer = instancesBuffer,
							  .m_allocation = instancesAllocation,
							  .m_allocationInfo = &instancesAllocInfo});
		memcpy(instancesAllocInfo.pMappedData, instances.data(), instancesSize);

		VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		geometry.geometry.instances.arrayOfPointers = VK_FALSE;
		geometry.geometry.instances.data.deviceAddress = GetBufferDeviceAddress(instancesBuffer);

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;

		auto primitiveCount = static_cast<uint32_t>(instances.size());
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		vkGetAccelerationStructureBuildSizesKHR(m_vkState().m_device,
												VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&buildInfo,
												&primitiveCount,
												&sizeInfo);

		CreateAccelerationStructureBuffer(m_topLevelAS, sizeInfo.accelerationStructureSize);

		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.buffer = m_topLevelAS.m_buffer;
		createInfo.size = sizeInfo.accelerationStructureSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(m_vkState().m_device, &createInfo, nullptr, &m_topLevelAS.m_handle);

		VkBuffer scratchBuffer{VK_NULL_HANDLE};
		VmaAllocation scratchAllocation{nullptr};
		VkBufferUsageFlags scratchUsage =
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VkMemoryPropertyFlags scratchProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VmaAllocationCreateFlags scratchVmaFlags = 0;
		vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_size = sizeInfo.buildScratchSize,
							  .m_usageFlags = scratchUsage,
							  .m_properties = scratchProps,
							  .m_vmaFlags = scratchVmaFlags,
							  .m_buffer = scratchBuffer,
							  .m_allocation = scratchAllocation,
							  .m_allocationInfo = nullptr});

		buildInfo.dstAccelerationStructure = m_topLevelAS.m_handle;
		buildInfo.scratchData.deviceAddress = GetBufferDeviceAddress(scratchBuffer);

		VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
		rangeInfo.primitiveCount = primitiveCount;
		const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

		VkCommandBuffer cmd = vvh::ComBeginSingleTimeCommands({m_vkState().m_device, m_commandPool});
		vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
		vvh::ComEndSingleTimeCommands({m_vkState().m_device, m_vkState().m_graphicsQueue, m_commandPool, cmd});

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
		addressInfo.accelerationStructure = m_topLevelAS.m_handle;
		m_topLevelAS.m_deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_vkState().m_device, &addressInfo);

		vvh::BufDestroyBuffer({m_vkState().m_device, m_vkState().m_vmaAllocator, scratchBuffer, scratchAllocation});
		vvh::BufDestroyBuffer({m_vkState().m_device, m_vkState().m_vmaAllocator, instancesBuffer, instancesAllocation});
	}

	/**
	 * @brief Destroys a single acceleration structure and its backing buffer.
	 */
	void RendererRaytracing::DestroyAccelerationStructure(const AccelerationStructure& as) {
		if( as.m_handle != VK_NULL_HANDLE ) {
			vkDestroyAccelerationStructureKHR(m_vkState().m_device, as.m_handle, nullptr);
		}
		if( as.m_buffer != VK_NULL_HANDLE ) {
			vvh::BufDestroyBuffer({m_vkState().m_device, m_vkState().m_vmaAllocator, as.m_buffer, as.m_allocation});
		}
	}

	/**
	 * @brief Loads a SPIR-V shader module from disk.
	 */
	auto RendererRaytracing::LoadShaderModule(const std::string& path) -> VkShaderModule {
		const std::vector<char> code = vvh::ReadFile(path);
		VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule shaderModule{VK_NULL_HANDLE};
		if( vkCreateShaderModule(m_vkState().m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS ) {
			throw std::runtime_error("Failed to create ray tracing shader module: " + path);
		}
		return shaderModule;
	}

	/**
	 * @brief (3) Create the ray tracing pipeline (raygen/miss/closest-hit shader groups).
	 */
	void RendererRaytracing::CreateRayTracingPipeline() {
		// --- Descriptor set layout: TLAS (0), storage image (1), camera UBO (2) ---
		VkDescriptorSetLayoutBinding accelBinding{};
		accelBinding.binding = 0;
		accelBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		accelBinding.descriptorCount = 1;
		accelBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		VkDescriptorSetLayoutBinding storageImageBinding{};
		storageImageBinding.binding = 1;
		storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		storageImageBinding.descriptorCount = 1;
		storageImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		VkDescriptorSetLayoutBinding uboBinding{};
		uboBinding.binding = 2;
		uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboBinding.descriptorCount = 1;
		uboBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		// Binding 3: per-instance material/geometry SSBO (closest hit).
		VkDescriptorSetLayoutBinding instanceBinding{};
		instanceBinding.binding = 3;
		instanceBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		instanceBinding.descriptorCount = 1;
		instanceBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		// Binding 4: scene lights SSBO (closest hit).
		VkDescriptorSetLayoutBinding lightsBinding{};
		lightsBinding.binding = 4;
		lightsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsBinding.descriptorCount = 1;
		lightsBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		// Binding 5: bindless texture array (closest hit).
		VkDescriptorSetLayoutBinding textureBinding{};
		textureBinding.binding = 5;
		textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		textureBinding.descriptorCount = MAX_RT_TEXTURES;
		textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		std::array bindings = {
				accelBinding, storageImageBinding, uboBinding, instanceBinding, lightsBinding, textureBinding};

		// The texture array is partially bound: only the used slots are written.
		std::array<VkDescriptorBindingFlags, 6> bindingFlags{};
		bindingFlags[5] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
		bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
		bindingFlagsInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		layoutInfo.pNext = &bindingFlagsInfo;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();
		if( vkCreateDescriptorSetLayout(m_vkState().m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) !=
			VK_SUCCESS ) {
			throw std::runtime_error("Failed to create ray tracing descriptor set layout");
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
		if( vkCreatePipelineLayout(m_vkState().m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
			VK_SUCCESS ) {
			throw std::runtime_error(
					"Failed to create ray tracing pipeline layout"); // TODO Replace with special exception
		}

		// --- Shader stages ---
		VkShaderModule raygenModule = LoadShaderModule(m_raygenShaderPath);
		VkShaderModule missModule = LoadShaderModule(m_missShaderPath);
		VkShaderModule shadowMissModule = LoadShaderModule(m_shadowMissShaderPath);
		VkShaderModule chitModule = LoadShaderModule(m_closestHitShaderPath);

		std::array<VkPipelineShaderStageCreateInfo, 4> stages{};
		stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stages[0].module = raygenModule;
		stages[0].pName = "main";
		stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[1].module = missModule;
		stages[1].pName = "main";
		stages[2] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[2].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[2].module = shadowMissModule;
		stages[2].pName = "main";
		stages[3] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[3].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		stages[3].module = chitModule;
		stages[3].pName = "main";

		// --- Shader groups: raygen (0), miss (1), shadow miss (2), closest-hit (3) ---
		m_shaderGroups.clear();
		VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;

		// raygen
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader = 0;
		m_shaderGroups.push_back(group);

		// miss (primary / reflection rays)
		group.generalShader = 1;
		m_shaderGroups.push_back(group);

		// shadow miss (shadow rays)
		group.generalShader = 2;
		m_shaderGroups.push_back(group);

		// closest hit (triangles hit group)
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = 3;
		m_shaderGroups.push_back(group);

		// Recursion: primary -> reflection chain + shadow rays. Clamp to device limit.
		uint32_t desiredDepth = 4;
		uint32_t maxDepth = std::min(desiredDepth, m_rtPipelineProperties.maxRayRecursionDepth);

		VkRayTracingPipelineCreateInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
		pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
		pipelineInfo.pStages = stages.data();
		pipelineInfo.groupCount = static_cast<uint32_t>(m_shaderGroups.size());
		pipelineInfo.pGroups = m_shaderGroups.data();
		pipelineInfo.maxPipelineRayRecursionDepth = maxDepth;
		pipelineInfo.layout = m_pipelineLayout;

		if( vkCreateRayTracingPipelinesKHR(
					m_vkState().m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) !=
			VK_SUCCESS ) {
			throw std::runtime_error("Failed to create ray tracing pipeline");
		}

		vkDestroyShaderModule(m_vkState().m_device, raygenModule, nullptr);
		vkDestroyShaderModule(m_vkState().m_device, missModule, nullptr);
		vkDestroyShaderModule(m_vkState().m_device, shadowMissModule, nullptr);
		vkDestroyShaderModule(m_vkState().m_device, chitModule, nullptr);

		// --- Descriptor pool + set ---
		std::array<VkDescriptorPoolSize, 5> poolSizes{};
		poolSizes[0] = {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
		poolSizes[1] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1};
		poolSizes[2] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
		poolSizes[3] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
		poolSizes[4] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_RT_TEXTURES};

		VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;
		if( vkCreateDescriptorPool(m_vkState().m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS ) {
			throw std::runtime_error("Failed to create ray tracing descriptor pool");
		}

		m_descriptorSets.resize(1);
		VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_descriptorSetLayout;
		if( vkAllocateDescriptorSets(m_vkState().m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS ) {
			throw std::runtime_error("Failed to allocate ray tracing descriptor set");
		}
	}

	/**
	 * @brief (3) Create the shader binding table for the ray tracing pipeline.
	 */
	void RendererRaytracing::CreateShaderBindingTable() {
		const uint32_t handleSize = m_rtPipelineProperties.shaderGroupHandleSize;
		const uint32_t handleAlignment = m_rtPipelineProperties.shaderGroupHandleAlignment;
		const uint32_t baseAlignment = m_rtPipelineProperties.shaderGroupBaseAlignment;
		const auto handleSizeAligned = static_cast<uint32_t>(AlignUp(handleSize, handleAlignment));
		const auto groupCount = static_cast<uint32_t>(m_shaderGroups.size());
		const uint32_t sbtSize = groupCount * handleSize;

		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		if( vkGetRayTracingShaderGroupHandlesKHR(
					m_vkState().m_device, m_pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data()) !=
			VK_SUCCESS ) {
			throw std::runtime_error("Failed to get ray tracing shader group handles");
		}

		// Builds an SBT region holding `recordCount` consecutive group handles
		// (starting at firstGroup). The miss region needs two records: the
		// primary/reflection miss (group 1) and the shadow miss (group 2).
		auto createSBT = [&](ShaderBindingTable& sbt, uint32_t firstGroup, uint32_t recordCount) {
			const VkDeviceSize regionSize = static_cast<VkDeviceSize>(handleSizeAligned) * recordCount;

			VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			bufferInfo.size = regionSize;
			bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
							   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo allocCreateInfo{};
			allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocCreateInfo.flags =
					VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

			VmaAllocationInfo allocInfo{};
			vmaCreateBufferWithAlignment(m_vkState().m_vmaAllocator,
										 &bufferInfo,
										 &allocCreateInfo,
										 baseAlignment,
										 &sbt.m_buffer,
										 &sbt.m_allocation,
										 &allocInfo);

			auto* dst = static_cast<uint8_t*>(allocInfo.pMappedData);
			for( uint32_t i = 0; i < recordCount; ++i ) {
				memcpy(dst + i * handleSizeAligned,
					   shaderHandleStorage.data() + (firstGroup + i) * handleSize,
					   handleSize);
			}

			sbt.m_region.deviceAddress = GetBufferDeviceAddress(sbt.m_buffer);
			sbt.m_region.stride = handleSizeAligned;
			sbt.m_region.size = regionSize;
		};

		createSBT(m_raygenSBT, 0, 1); // raygen (group 0)
		createSBT(m_missSBT, 1, 2);	  // miss (group 1) + shadow miss (group 2)
		createSBT(m_hitSBT, 3, 1);	  // closest hit (group 3)
	}

	/**
	 * @brief Create the storage image the ray generation shader writes into.
	 */
	void RendererRaytracing::CreateStorageImage() {
		auto [width, height] = m_vkState().m_swapChain.m_swapChainExtent;
		constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
		constexpr VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		constexpr VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		constexpr VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_storageImage.m_width = static_cast<int>(width);
		m_storageImage.m_height = static_cast<int>(height);

		vvh::ImgCreateImage2({.m_physicalDevice = m_vkState().m_physicalDevice,
							  .m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_width = width,
							  .m_height = height,
							  .m_format = format,
							  .m_tiling = tiling,
							  .m_usage = usage,
							  .m_imageLayout = initialLayout,
							  .m_properties = props,
							  .m_image = m_storageImage.m_mapImage,
							  .m_imageAllocation = m_storageImage.m_mapImageAllocation});

		m_storageImage.m_mapImageView = vvh::ImgCreateImageView2({.m_device = m_vkState().m_device,
																  .m_image = m_storageImage.m_mapImage,
																  .m_format = format,
																  .m_aspects = VK_IMAGE_ASPECT_COLOR_BIT});

		// Transition into VK_IMAGE_LAYOUT_GENERAL so the raygen shader can store to it.
		vvh::ImgTransitionImageLayout2({.m_device = m_vkState().m_device,
										.m_graphicsQueue = m_vkState().m_graphicsQueue,
										.m_commandPool = m_commandPool,
										.m_image = m_storageImage.m_mapImage,
										.m_format = format,
										.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
										.m_newLayout = VK_IMAGE_LAYOUT_GENERAL});
	}

	/**
	 * @brief Writes the TLAS / storage image / camera UBO into the descriptor set.
	 */
	void RendererRaytracing::UpdateDescriptorSets() {
		if( m_descriptorSets.empty() ) {
			return;
		}

		std::vector<VkWriteDescriptorSet> writes;

		// Binding 0: top level acceleration structure (only if it exists).
		VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
		if( m_topLevelAS.m_handle != VK_NULL_HANDLE ) {
			accelInfo.accelerationStructureCount = 1;
			accelInfo.pAccelerationStructures = &m_topLevelAS.m_handle;

			VkWriteDescriptorSet accelWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			accelWrite.pNext = &accelInfo;
			accelWrite.dstSet = m_descriptorSets[0];
			accelWrite.dstBinding = 0;
			accelWrite.descriptorCount = 1;
			accelWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			writes.push_back(accelWrite);
		}

		// Binding 1: storage image.
		VkDescriptorImageInfo storageImageInfo{};
		storageImageInfo.imageView = m_storageImage.m_mapImageView;
		storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet imageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		imageWrite.dstSet = m_descriptorSets[0];
		imageWrite.dstBinding = 1;
		imageWrite.descriptorCount = 1;
		imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageWrite.pImageInfo = &storageImageInfo;
		writes.push_back(imageWrite);

		// Binding 2: camera uniform buffer.
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_uniformBuffer.m_uniformBuffers[0];
		bufferInfo.offset = 0;
		bufferInfo.range = m_uniformBuffer.m_bufferSize;

		VkWriteDescriptorSet bufferWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		bufferWrite.dstSet = m_descriptorSets[0];
		bufferWrite.dstBinding = 2;
		bufferWrite.descriptorCount = 1;
		bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferWrite.pBufferInfo = &bufferInfo;
		writes.push_back(bufferWrite);

		// Binding 3: per-instance material/geometry SSBO.
		VkDescriptorBufferInfo instanceInfo{};
		instanceInfo.buffer = m_instanceDataBuffer.m_uniformBuffers[0];
		instanceInfo.offset = 0;
		instanceInfo.range = m_instanceDataBuffer.m_bufferSize;

		VkWriteDescriptorSet instanceWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		instanceWrite.dstSet = m_descriptorSets[0];
		instanceWrite.dstBinding = 3;
		instanceWrite.descriptorCount = 1;
		instanceWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		instanceWrite.pBufferInfo = &instanceInfo;
		writes.push_back(instanceWrite);

		// Binding 4: scene lights SSBO.
		VkDescriptorBufferInfo lightsInfo{};
		lightsInfo.buffer = m_lightsBuffer.m_uniformBuffers[0];
		lightsInfo.offset = 0;
		lightsInfo.range = m_lightsBuffer.m_bufferSize;

		VkWriteDescriptorSet lightsWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		lightsWrite.dstSet = m_descriptorSets[0];
		lightsWrite.dstBinding = 4;
		lightsWrite.descriptorCount = 1;
		lightsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsWrite.pBufferInfo = &lightsInfo;
		writes.push_back(lightsWrite);

		// Binding 5: bindless texture array (only the used, partially-bound slots).
		VkWriteDescriptorSet textureWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		if( !m_textureInfos.empty() ) {
			textureWrite.dstSet = m_descriptorSets[0];
			textureWrite.dstBinding = 5;
			textureWrite.dstArrayElement = 0;
			textureWrite.descriptorCount = static_cast<uint32_t>(m_textureInfos.size());
			textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			textureWrite.pImageInfo = m_textureInfos.data();
			writes.push_back(textureWrite);
		}

		vkUpdateDescriptorSets(m_vkState().m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/**
	 * @brief Records an image layout transition into an already-open command buffer.
	 */
	void RendererRaytracing::RecordImageLayoutTransition(VkCommandBuffer cmd,
														 VkImage image,
														 VkImageLayout oldLayout,
														 VkImageLayout newLayout,
														 VkPipelineStageFlags srcStage,
														 VkPipelineStageFlags dstStage,
														 VkAccessFlags srcAccess,
														 VkAccessFlags dstAccess) {
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;
		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	/**
	 * @brief Gathers all scene lights into the lights SSBO read by the hit shader.
	 * @return Per-type light counts (x=point, y=directional, z=spot, w=total).
	 */
	glm::ivec4 RendererRaytracing::UpdateLights() {
		std::vector<vvh::Light> lights(MAX_NUMBER_LIGHTS);
		int total = 0;
		glm::ivec4 counts{0};
		counts.x = RegisterLight<PointLight>(1.0f, lights, total);
		counts.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		counts.z = RegisterLight<SpotLight>(3.0f, lights, total);
		counts.w = total;

		if( total > 0 && m_lightsBuffer.m_uniformBuffersMapped[0] != nullptr ) {
			memcpy(m_lightsBuffer.m_uniformBuffersMapped[0], lights.data(), total * sizeof(vvh::Light));
		}
		return counts;
	}

	/**
	 * @brief Prepares the next frame: rebuilds the TLAS if needed, updates the
	 *        camera UBO, and acquires the next swap chain image.
	 */
	bool RendererRaytracing::OnPrepareNextFrame(Message message) {
		// Rebuild the top level acceleration structure. This has to happen every
		// frame so that per-object world transforms (which can change over time)
		// are reflected in the TLAS instances. The dirty flag only tracks whether
		// the set of meshes (BLAS) changed.
		if( !m_bottomLevelAS.empty() ) {
			vkDeviceWaitIdle(m_vkState().m_device);
			CreateTopLevelAS();
			UpdateDescriptorSets();
		}
		m_accelerationStructureDirty = false;

		m_vkState().m_currentFrame = (m_vkState().m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_vkState().m_commandBuffersSubmit.clear();

		vkWaitForFences(m_vkState().m_device, 1, &m_fences[m_vkState().m_currentFrame], VK_TRUE, UINT64_MAX);

		// Gather scene lights for the closest-hit shader.
		glm::ivec4 lightCounts = UpdateLights();

		// Update the camera (view / projection inverse, position, light counts).
		auto cameraView = m_registry.GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>();
		if( cameraView.begin() != cameraView.end() ) {
			auto [lToW, view, proj] = *cameraView.begin();
			vvh::rt::CameraRT cam{};
			cam.viewInverse = glm::inverse(view());
			cam.projInverse = glm::inverse(proj());
			cam.cameraPos = glm::vec4(glm::vec3(lToW()[3]), 1.0f);
			cam.numLights = lightCounts;
			memcpy(m_uniformBuffer.m_uniformBuffersMapped[0], &cam, sizeof(cam));
		}

		const VkResult result = vkAcquireNextImageKHR(m_vkState().m_device,
													  m_vkState().m_swapChain.m_swapChain,
													  UINT64_MAX,
													  m_imageAvailableSemaphores[m_vkState().m_currentFrame],
													  VK_NULL_HANDLE,
													  &m_vkState().m_imageIndex);

		if( result == VK_ERROR_OUT_OF_DATE_KHR ) {
			RecreateSwapChain();
			m_engine.SendMsg(MsgWindowSize{});
		} else {
			assert(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		}
		return false;
	}

	/**
	 * @brief Records the ray tracing dispatch and the blit into the swap chain image.
	 */
	bool RendererRaytracing::OnRecordNextFrame(Message message) {
		VkCommandBuffer cmd = m_commandBuffers[m_vkState().m_currentFrame];
		VkImage swapImage = m_vkState().m_swapChain.m_swapChainImages[m_vkState().m_imageIndex];
		VkExtent2D extent = m_vkState().m_swapChain.m_swapChainExtent;

		vkResetCommandBuffer(cmd, 0);
		vvh::ComBeginCommandBuffer({.m_commandBuffer = cmd});

		if( m_topLevelAS.m_handle != VK_NULL_HANDLE ) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
			vkCmdBindDescriptorSets(cmd,
									VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
									m_pipelineLayout,
									0,
									1,
									&m_descriptorSets[0],
									0,
									nullptr);

			VkStridedDeviceAddressRegionKHR callableRegion{};
			vkCmdTraceRaysKHR(cmd,
							  &m_raygenSBT.m_region,
							  &m_missSBT.m_region,
							  &m_hitSBT.m_region,
							  &callableRegion,
							  extent.width,
							  extent.height,
							  1);
		}

		// Make the raygen writes available and prepare both images for the blit.
		RecordImageLayoutTransition(cmd,
									m_storageImage.m_mapImage,
									VK_IMAGE_LAYOUT_GENERAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
									VK_PIPELINE_STAGE_TRANSFER_BIT,
									VK_ACCESS_SHADER_WRITE_BIT,
									VK_ACCESS_TRANSFER_READ_BIT);

		RecordImageLayoutTransition(cmd,
									swapImage,
									VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
									VK_PIPELINE_STAGE_TRANSFER_BIT,
									0,
									VK_ACCESS_TRANSFER_WRITE_BIT);

		VkImageBlit blit{};
		blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		blit.srcOffsets[1] = {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1};
		blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		blit.dstOffsets[1] = {static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1};
		vkCmdBlitImage(cmd,
					   m_storageImage.m_mapImage,
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   swapImage,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1,
					   &blit,
					   VK_FILTER_NEAREST);

		// Leave the swap chain image ready for overlay rendering (ImGui) which
		// runs after this renderer and uses a render pass expecting the image in
		// COLOR_ATTACHMENT_OPTIMAL. The transition to PRESENT_SRC_KHR happens in
		// OnRenderNextFrame after all command buffers have been submitted.
		RecordImageLayoutTransition(cmd,
									swapImage,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									VK_PIPELINE_STAGE_TRANSFER_BIT,
									VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									VK_ACCESS_TRANSFER_WRITE_BIT,
									VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		RecordImageLayoutTransition(cmd,
									m_storageImage.m_mapImage,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									VK_IMAGE_LAYOUT_GENERAL,
									VK_PIPELINE_STAGE_TRANSFER_BIT,
									VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
									VK_ACCESS_TRANSFER_READ_BIT,
									VK_ACCESS_SHADER_WRITE_BIT);

		vvh::ComEndCommandBuffer({.m_commandBuffer = cmd});
		SubmitCommandBuffer(cmd);
		return false;
	}

	/**
	 * @brief Submits command buffers and presents the rendered frame.
	 */
	bool RendererRaytracing::OnRenderNextFrame(Message message) {
		const size_t size = m_vkState().m_commandBuffersSubmit.size();
		if( size > m_intermediateSemaphores.size() ) {
			vvh::SynCreateSemaphores({.m_device = m_vkState().m_device,
									  .m_imageAvailableSemaphores = m_imageAvailableSemaphores,
									  .m_renderFinishedSemaphores = m_renderFinishedSemaphores,
									  .m_size = size,
									  .m_intermediateSemaphores = m_intermediateSemaphores});
		}

		vvh::ComSubmitCommandBuffers({m_vkState().m_device,
									  m_vkState().m_graphicsQueue,
									  m_vkState().m_commandBuffersSubmit,
									  m_imageAvailableSemaphores,
									  m_renderFinishedSemaphores,
									  m_intermediateSemaphores,
									  m_fences,
									  m_vkState().m_currentFrame});

		// Overlay renderers (ImGui) left the swap chain image in
		// COLOR_ATTACHMENT_OPTIMAL; transition it to PRESENT_SRC_KHR before present.
		vvh::ImgTransitionImageLayout2({.m_device = m_vkState().m_device,
										.m_graphicsQueue = m_vkState().m_graphicsQueue,
										.m_commandPool = m_commandPool,
										.m_image = m_vkState().m_swapChain.m_swapChainImages[m_vkState().m_imageIndex],
										.m_format = m_vkState().m_swapChain.m_swapChainImageFormat,
										.m_oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										.m_newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});

		const VkResult result =
				vvh::ComPresentImage({.m_presentQueue = m_vkState().m_presentQueue,
									  .m_swapChain = m_vkState().m_swapChain,
									  .m_imageIndex = m_vkState().m_imageIndex,
									  .m_signalSemaphore = m_renderFinishedSemaphores[m_vkState().m_currentFrame]});

		if( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_vkState().m_framebufferResized ) {
			m_vkState().m_framebufferResized = false;
			RecreateSwapChain();
			m_engine.SendMsg(MsgWindowSize{});
		} else {
			assert(result == VK_SUCCESS);
		}
		return false;
	}

	/**
	 * @brief Cleans up all ray tracing resources when shutting down the renderer.
	 */
	bool RendererRaytracing::OnQuit(Message message) {
		vkDeviceWaitIdle(m_vkState().m_device);

		// Shader binding table buffers.
		for( const ShaderBindingTable* sbt : {&m_raygenSBT, &m_missSBT, &m_hitSBT} ) {
			if( sbt->m_buffer != VK_NULL_HANDLE ) {
				vvh::BufDestroyBuffer(
						{m_vkState().m_device, m_vkState().m_vmaAllocator, sbt->m_buffer, sbt->m_allocation});
			}
		}

		// Pipeline + descriptors.
		if( m_pipeline != VK_NULL_HANDLE ) {
			vkDestroyPipeline(m_vkState().m_device, m_pipeline, nullptr);
		}
		if( m_pipelineLayout != VK_NULL_HANDLE ) {
			vkDestroyPipelineLayout(m_vkState().m_device, m_pipelineLayout, nullptr);
		}
		if( m_descriptorPool != VK_NULL_HANDLE ) {
			vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		}
		if( m_descriptorSetLayout != VK_NULL_HANDLE ) {
			vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayout, nullptr);
		}

		// Acceleration structures.
		DestroyAccelerationStructure(m_topLevelAS);
		for( auto& blas : m_bottomLevelAS ) {
			DestroyAccelerationStructure(blas);
		}
		m_bottomLevelAS.clear();
		m_bottomLevelASMeshes.clear();

		// Storage image.
		vkDestroyImageView(m_vkState().m_device, m_storageImage.m_mapImageView, nullptr);
		vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_image = m_storageImage.m_mapImage,
							  .m_imageAllocation = m_storageImage.m_mapImageAllocation});

		// Camera uniform buffer.
		vvh::BufDestroyBuffer2({.m_device = m_vkState().m_device,
								.m_vmaAllocator = m_vkState().m_vmaAllocator,
								.m_buffers = m_uniformBuffer});

		// Whitted shading buffers (per-instance data + lights).
		vvh::BufDestroyBuffer2({.m_device = m_vkState().m_device,
								.m_vmaAllocator = m_vkState().m_vmaAllocator,
								.m_buffers = m_instanceDataBuffer});
		vvh::BufDestroyBuffer2({.m_device = m_vkState().m_device,
								.m_vmaAllocator = m_vkState().m_vmaAllocator,
								.m_buffers = m_lightsBuffer});

		// Mesh vertex / index buffers.
		for( auto geometry : m_registry.GetView<vvh::Mesh&>() ) {
			vvh::BufDestroyBuffer({m_vkState().m_device,
								   m_vkState().m_vmaAllocator,
								   geometry().m_indexBuffer,
								   geometry().m_indexBufferAllocation});
			vvh::BufDestroyBuffer({m_vkState().m_device,
								   m_vkState().m_vmaAllocator,
								   geometry().m_vertexBuffer,
								   geometry().m_vertexBufferAllocation});
		}

		// Textures.
		for( auto texture : m_registry.GetView<vvh::Image&>() ) {
			vkDestroySampler(m_vkState().m_device, texture().m_mapSampler, nullptr);
			vkDestroyImageView(m_vkState().m_device, texture().m_mapImageView, nullptr);
			vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
								  .m_vmaAllocator = m_vkState().m_vmaAllocator,
								  .m_image = texture().m_mapImage,
								  .m_imageAllocation = texture().m_mapImageAllocation});
		}

		vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);
		vkDestroyCommandPool(m_vkState().m_device, m_vkState().m_commandPool, nullptr);

		vvh::SynDestroyFences({m_vkState().m_device, m_fences});
		vvh::SynDestroySemaphores({m_vkState().m_device,
								   m_imageAvailableSemaphores,
								   m_renderFinishedSemaphores,
								   m_intermediateSemaphores});

		// Framebuffers + render pass + depth image for the shared swap chain.
		for( auto framebuffer : m_vkState().m_swapChain.m_swapChainFramebuffers ) {
			vkDestroyFramebuffer(m_vkState().m_device, framebuffer, nullptr);
		}
		m_vkState().m_swapChain.m_swapChainFramebuffers.clear();

		if( m_renderPass != VK_NULL_HANDLE ) {
			vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		}

		vkDestroyImageView(m_vkState().m_device, m_vkState().m_depthImage.m_depthImageView, nullptr);
		vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_image = m_vkState().m_depthImage.m_depthImage,
							  .m_imageAllocation = m_vkState().m_depthImage.m_depthImageAllocation});

		for( const auto imageView : m_vkState().m_swapChain.m_swapChainImageViews ) {
			vkDestroyImageView(m_vkState().m_device, imageView, nullptr);
		}
		vkDestroySwapchainKHR(m_vkState().m_device, m_vkState().m_swapChain.m_swapChain, nullptr);

		vmaDestroyAllocator(m_vkState().m_vmaAllocator);
		vkDestroyDevice(m_vkState().m_device, nullptr);
		vkDestroySurfaceKHR(m_vkState().m_instance, m_vkState().m_surface, nullptr);

		if( m_engine.GetState().m_debug ) {
			vvh::DevDestroyDebugUtilsMessengerEXT({.m_instance = m_vkState().m_instance,
												   .m_debugMessenger = m_vkState().m_debugMessenger,
												   .m_pAllocator = nullptr});
		}
		vkDestroyInstance(m_vkState().m_instance, nullptr);
		return false;
	}

	//-------------------------------------------------------------------------------------------------------

	/**
	 * @brief Creates a texture from image data (identical to RendererVulkan::OnTextureCreate).
	 */
	bool RendererRaytracing::OnTextureCreate(Message message) {
		const auto msg = message.GetData<MsgTextureCreate>();
		auto handle = msg.m_handle;
		auto texture = m_registry.Get<vvh::Image&>(handle);
		const auto pixels = texture().m_pixels;

		vvh::ImgCreateTextureImage({m_vkState().m_physicalDevice,
									m_vkState().m_device,
									m_vkState().m_vmaAllocator,
									m_vkState().m_graphicsQueue,
									m_commandPool,
									pixels,
									texture().m_width,
									texture().m_height,
									texture().m_size,
									texture});
		vvh::ImgCreateTextureImageView({m_vkState().m_device, texture});
		vvh::ImgCreateTextureSampler({m_vkState().m_physicalDevice, m_vkState().m_device, texture});
		return false;
	}

	/**
	 * @brief Destroys a texture and frees associated resources.
	 */
	bool RendererRaytracing::OnTextureDestroy(Message message) {
		auto handle = message.GetData<MsgTextureDestroy>().m_handle;
		auto texture = m_registry.Get<vvh::Image&>(handle);
		vkDestroySampler(m_vkState().m_device, texture().m_mapSampler, nullptr);
		vkDestroyImageView(m_vkState().m_device, texture().m_mapImageView, nullptr);
		vvh::ImgDestroyImage({.m_device = m_vkState().m_device,
							  .m_vmaAllocator = m_vkState().m_vmaAllocator,
							  .m_image = texture().m_mapImage,
							  .m_imageAllocation = texture().m_mapImageAllocation});
		m_registry.Erase(handle);
		return false;
	}

	/**
	 * @brief Creates ray-tracing capable vertex/index buffers for a mesh and builds its BLAS.
	 */
	bool RendererRaytracing::OnMeshCreate(Message message) {
		auto handle = message.GetData<MsgMeshCreate>().m_handle;
		auto mesh = m_registry.Get<vvh::Mesh&>(handle);

		// Buffers used both as vertex/index buffers and as acceleration-structure build input.
		constexpr VkBufferUsageFlags asUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

		auto createDeviceLocalBuffer = [&](const void* src,
										   VkDeviceSize size,
										   VkBufferUsageFlags extraUsage,
										   VkBuffer& outBuffer,
										   VmaAllocation& outAllocation) {
			VkBuffer stagingBuffer{VK_NULL_HANDLE};
			VmaAllocation stagingAllocation{nullptr};
			VmaAllocationInfo stagingInfo{};
			VkBufferUsageFlags srcUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			VkMemoryPropertyFlags hostProps =
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			VmaAllocationCreateFlags hostFlags =
					VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
								  .m_size = size,
								  .m_usageFlags = srcUsage,
								  .m_properties = hostProps,
								  .m_vmaFlags = hostFlags,
								  .m_buffer = stagingBuffer,
								  .m_allocation = stagingAllocation,
								  .m_allocationInfo = &stagingInfo});
			memcpy(stagingInfo.pMappedData, src, size);

			VkBufferUsageFlags dstUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | asUsage | extraUsage;
			VkMemoryPropertyFlags devProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			VmaAllocationCreateFlags devFlags = 0;
			vvh::BufCreateBuffer({.m_vmaAllocator = m_vkState().m_vmaAllocator,
								  .m_size = size,
								  .m_usageFlags = dstUsage,
								  .m_properties = devProps,
								  .m_vmaFlags = devFlags,
								  .m_buffer = outBuffer,
								  .m_allocation = outAllocation,
								  .m_allocationInfo = nullptr});

			vvh::BufCopyBuffer(
					{m_vkState().m_device, m_vkState().m_graphicsQueue, m_commandPool, stagingBuffer, outBuffer, size});
			vvh::BufDestroyBuffer({m_vkState().m_device, m_vkState().m_vmaAllocator, stagingBuffer, stagingAllocation});
		};

		// Vertex buffer (full interleaved attribute data; positions are the first block).
		VkDeviceSize vertexSize = mesh().m_verticesData.getSize();
		std::vector<uint8_t> vertexStaging(vertexSize);
		mesh().m_verticesData.copyData(vertexStaging.data());
		createDeviceLocalBuffer(vertexStaging.data(),
								vertexSize,
								VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
								mesh().m_vertexBuffer,
								mesh().m_vertexBufferAllocation);

		// Index buffer.
		VkDeviceSize indexSize = sizeof(uint32_t) * mesh().m_indices.size();
		createDeviceLocalBuffer(mesh().m_indices.data(),
								indexSize,
								VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
								mesh().m_indexBuffer,
								mesh().m_indexBufferAllocation);

		// Build the BLAS for this mesh and flag the TLAS for a rebuild.
		AccelerationStructure blas = CreateBottomLevelAS(mesh());
		m_bottomLevelAS.push_back(blas);
		m_bottomLevelASMeshes.push_back(handle);
		m_accelerationStructureDirty = true;
		return false;
	}

	/**
	 * @brief Destroys a mesh, its buffers, and BLAS, then flags the TLAS for a rebuild.
	 */
	bool RendererRaytracing::OnMeshDestroy(Message message) {
		auto handle = message.GetData<MsgMeshDestroy>().m_handle;
		auto mesh = m_registry.Get<vvh::Mesh&>(handle);

		vkDeviceWaitIdle(m_vkState().m_device);

		for( size_t i = 0; i < m_bottomLevelASMeshes.size(); ++i ) {
			if( m_bottomLevelASMeshes[i] == handle ) {
				DestroyAccelerationStructure(m_bottomLevelAS[i]);
				m_bottomLevelAS.erase(m_bottomLevelAS.begin() + i);
				m_bottomLevelASMeshes.erase(m_bottomLevelASMeshes.begin() + i);
				break;
			}
		}

		vvh::BufDestroyBuffer({m_vkState().m_device,
							   m_vkState().m_vmaAllocator,
							   mesh().m_indexBuffer,
							   mesh().m_indexBufferAllocation});
		vvh::BufDestroyBuffer({m_vkState().m_device,
							   m_vkState().m_vmaAllocator,
							   mesh().m_vertexBuffer,
							   mesh().m_vertexBufferAllocation});

		m_accelerationStructureDirty = true;
		return false;
	}

}; // namespace vve
