module;
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VVE_SIMPLE_DEFINED_STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>
#ifdef VVE_SIMPLE_DEFINED_STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#undef VVE_SIMPLE_DEFINED_STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image_write.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Math;

/**
	* @file
	* @brief Vulkan instance ownership for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanInstance owns only VkInstance creation and teardown.
	* - VulkanSurface owns only VkSurfaceKHR creation and teardown.
	* - VulkanPhysicalDevice selects a physical device and queue family indices.
	* - VulkanDevice owns only VkDevice creation and queue retrieval.
	* - VulkanSwapchain owns only VkSwapchainKHR creation and swapchain image retrieval.
	* - VulkanImageViews owns only VkImageView creation and teardown for borrowed swapchain images.
	* - VulkanDepthImage owns one depth VkImage, its device memory, and its VkImageView.
	* - ShadowMap owns one square sampled D32 depth image, view, sampler, backing memory, and unused shadow pipeline.
	* - VulkanRenderPass owns only VkRenderPass creation and teardown for a single forward color subpass.
	* - VulkanFramebuffers owns only VkFramebuffer creation and teardown for borrowed swapchain image views.
	* - VulkanDescriptorSetLayout owns only VkDescriptorSetLayout creation and teardown for frame uniforms, shadow-map sampling, and object-texture sampling.
	* - VulkanVertexInputDescription stores the fixed Vertex binding and attribute layout for the forward pipeline.
	* - VulkanPipelineLayout owns only VkPipelineLayout creation and teardown for one descriptor set and model push constants.
	* - VulkanShaderModule owns only VkShaderModule creation from SPIR-V bytes and teardown.
	* - VulkanGraphicsPipeline owns only VkPipeline creation for the simple forward pass and teardown.
	* - VulkanCommandPool owns only VkCommandPool creation for the graphics queue family and teardown.
	* - VulkanCommandBuffers owns primary command-buffer allocation from a borrowed command pool and teardown.
	* - VulkanFrameSync owns per-frame semaphores and fences for the simple forward renderer.
	* - VulkanBuffer owns one VkBuffer and its backing VkDeviceMemory allocation.
	* - VulkanReadback copies one finished color VkImage into a host-visible VulkanBuffer and exposes CPU pixel bytes.
	* - TextureImage owns one sampled RGBA8 texture image, view, sampler, and backing memory.
	* - writeReadbackPng encodes VulkanReadback bytes as deterministic opaque RGBA8 PNG files.
	* - VulkanDescriptorPool owns only VkDescriptorPool creation and teardown for uniform-buffer, shadow-map, and object-texture descriptor sets.
	* - VulkanDescriptorSets allocates per-frame uniform-buffer, shadow-map, and object-texture descriptor sets from a borrowed pool and layout.
	* - VulkanMesh owns the vertex and index buffers and index count for one uploaded CPU mesh.
	* - ObjectPushConstants stores per-object draw data copied through Vulkan push constants.
	* - FrameUniforms stores the shared view and projection matrices for set 0 binding 0.
	* - VulkanUniformBuffers owns one host-visible FrameUniforms buffer per frame.
	*/
export namespace vve::simple {
	struct VulkanVertexInputDescription; ///< Fixed mesh vertex layout reused by forward and shadow pipelines.
	struct TextureImage;                 ///< Internal sampled texture image owner for later material bindings.

	/// @brief Plain per-object push-constant data matching the Slang ObjectPushConstants block layout.
	struct ObjectPushConstants {
		Mat4 model{};                          ///< Object-local model matrix selected before each draw call.
		std::uint32_t useBaseColorTexture{0U}; ///< Non-zero when the object wants the optional base-color texture.
	};

	/// @brief Minimal Vulkan root object; no device, surface, swapchain, commands, or sync are created here.
	struct VulkanInstance {
		VkInstance instance{VK_NULL_HANDLE};         ///< Owned Vulkan instance handle.
		std::vector<char const *> extensions{};      ///< SDL-required instance extensions used for creation.
		std::vector<char const *> layers{};          ///< Optional validation layers enabled when available.
		bool validationEnabled{false};               ///< True when VK_LAYER_KHRONOS_validation was enabled.

		VulkanInstance() = default;
		VulkanInstance(const VulkanInstance &) = delete;
		VulkanInstance &operator=(const VulkanInstance &) = delete;

		/**
			* @brief Creates a Vulkan instance with SDL platform extensions and optional validation.
			*
			* @param applicationName Human-readable application name stored in VkApplicationInfo.
			* @return Vulkan result from extension discovery or vkCreateInstance.
			*/
		[[nodiscard]] VkResult create(std::string_view applicationName = "VVE Simple") {
			cleanup();

			std::uint32_t extensionCount{};
			const char *const *const sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
			if (sdlExtensions == nullptr) { return VK_ERROR_EXTENSION_NOT_PRESENT; }

			extensions.clear();
			extensions.reserve(extensionCount);
			for (std::uint32_t index{}; index < extensionCount; ++index) {
				if (sdlExtensions[index] != nullptr) { extensions.push_back(sdlExtensions[index]); }
			}

			layers.clear();
			validationEnabled = validationLayerAvailable();
			if (validationEnabled) { layers.push_back(validationLayerName); }

			const auto appName = std::string{applicationName};
			const VkApplicationInfo appInfo{
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = appName.c_str(),
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "ViennaVulkanEngine Simple",
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_API_VERSION_1_4,
			};

			const VkInstanceCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
				.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
				.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data(),
			};

			const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
			if (result != VK_SUCCESS) { instance = VK_NULL_HANDLE; }
			return result;
		}

		/**
			* @brief Destroys the owned Vulkan instance if one exists.
			*/
		void cleanup() {
			if (instance != VK_NULL_HANDLE) {
				vkDestroyInstance(instance, nullptr);
				instance = VK_NULL_HANDLE;
			}
		}

		/**
			* @brief Destroys the owned Vulkan instance on scope exit.
			*/
		~VulkanInstance() { cleanup(); }

	private:
		static constexpr char const *validationLayerName{"VK_LAYER_KHRONOS_validation"}; ///< Standard Vulkan validation layer.

		/**
			* @brief Checks whether the optional standard validation layer is installed.
			*
			* @return True if Vulkan reports VK_LAYER_KHRONOS_validation in the instance layer list.
			*/
		[[nodiscard]] static bool validationLayerAvailable() {
			std::uint32_t layerCount{};
			if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS) { return false; }

			auto layerProperties = std::vector<VkLayerProperties>(layerCount);
			if (layerCount != 0U && vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data()) != VK_SUCCESS) {
				return false;
			}

			return std::ranges::any_of(layerProperties, [](const VkLayerProperties &layer) {
				return std::string_view{layer.layerName} == validationLayerName;
			});
		}
	};

	/// @brief Minimal Vulkan window surface object; no device, swapchain, commands, or sync are created here.
	struct VulkanSurface {
		VkSurfaceKHR surface{VK_NULL_HANDLE};        ///< Owned Vulkan surface handle.
		VkInstance instance{VK_NULL_HANDLE};         ///< Borrowed Vulkan instance used to destroy the surface.

		VulkanSurface() = default;
		VulkanSurface(const VulkanSurface &) = delete;
		VulkanSurface &operator=(const VulkanSurface &) = delete;

		/**
			* @brief Creates an SDL-backed Vulkan surface for an existing Vulkan instance.
			*
			* @param owningInstance Vulkan instance that owns the platform surface connection.
			* @param window SDL window that provides the native platform surface.
			* @return VK_SUCCESS when SDL created the surface, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult create(VkInstance owningInstance, SDL_Window *window) {
			cleanup();
			instance = owningInstance;
			if (instance == VK_NULL_HANDLE || window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
				surface = VK_NULL_HANDLE;
				instance = VK_NULL_HANDLE;
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned Vulkan surface if one exists.
			*/
		void cleanup() {
			if (surface != VK_NULL_HANDLE) {
				SDL_Vulkan_DestroySurface(instance, surface, nullptr);
				surface = VK_NULL_HANDLE;
				instance = VK_NULL_HANDLE;
			}
		}

		/**
			* @brief Destroys the owned Vulkan surface on scope exit.
			*/
		~VulkanSurface() { cleanup(); }
	};

	/// @brief Non-owning Vulkan physical-device selector for graphics and presentation support.
	struct VulkanPhysicalDevice {
		VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};             ///< Borrowed physical device selected from the instance.
		std::optional<std::uint32_t> graphicsQueueFamily{};          ///< Queue family index supporting graphics commands.
		std::optional<std::uint32_t> presentQueueFamily{};           ///< Queue family index supporting presentation to the surface.

		/**
			* @brief Selects the first physical device that supports graphics, presentation, and swapchains.
			*
			* @param instance Vulkan instance that owns the physical-device list.
			* @param surface Vulkan surface used to test presentation support.
			* @return VK_SUCCESS when a device was selected, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult select(VkInstance instance, VkSurfaceKHR surface) {
			reset();
			if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			std::uint32_t deviceCount{};
			VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			if (deviceCount == 0U) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			auto devices = std::vector<VkPhysicalDevice>(deviceCount);
			result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
			if (result != VK_SUCCESS) { return result; }

			for (const VkPhysicalDevice candidate : devices) {
				result = selectIfSuitable(candidate, surface);
				if (result == VK_SUCCESS && selected()) { return VK_SUCCESS; }
				if (result != VK_SUCCESS && result != VK_ERROR_FEATURE_NOT_PRESENT) { reset(); return result; }
			}

			reset();
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		/**
			* @brief Reports whether a device and both required queue family indices are available.
			*
			* @return True after successful selection.
			*/
		[[nodiscard]] bool selected() const {
			return physicalDevice != VK_NULL_HANDLE && graphicsQueueFamily.has_value() && presentQueueFamily.has_value();
		}

	private:
		static constexpr char const *swapchainExtensionName{VK_KHR_SWAPCHAIN_EXTENSION_NAME}; ///< Required presentation extension.

		/**
			* @brief Clears the borrowed physical-device handle and discovered queue family indices.
			*/
		void reset() {
			physicalDevice = VK_NULL_HANDLE;
			graphicsQueueFamily.reset();
			presentQueueFamily.reset();
		}

		/**
			* @brief Tests one physical device and records it when all required capabilities exist.
			*
			* @param candidate Physical device being inspected.
			* @param surface Surface used for presentation support checks.
			* @return VK_SUCCESS for selected devices, VK_ERROR_FEATURE_NOT_PRESENT for unsuitable devices, or a query error.
			*/
		[[nodiscard]] VkResult selectIfSuitable(VkPhysicalDevice candidate, VkSurfaceKHR surface) {
			std::optional<std::uint32_t> graphicsFamily{};
			std::optional<std::uint32_t> presentationFamily{};

			std::uint32_t queueFamilyCount{};
			vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
			auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
			if (queueFamilyCount != 0U) {
				vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());
			}

			for (std::uint32_t index{}; index < queueFamilyCount; ++index) {
				if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) { graphicsFamily = index; }

				VkBool32 presentSupported{VK_FALSE};
				const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &presentSupported);
				if (result != VK_SUCCESS) { return result; }
				if (presentSupported == VK_TRUE) { presentationFamily = index; }

				if (graphicsFamily.has_value() && presentationFamily.has_value()) { break; }
			}

			const VkResult result = supportsSwapchain(candidate);
			if (result != VK_SUCCESS) { return result; }
			if (!graphicsFamily.has_value() || !presentationFamily.has_value()) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			physicalDevice = candidate;
			graphicsQueueFamily = graphicsFamily;
			presentQueueFamily = presentationFamily;
			return VK_SUCCESS;
		}

		/**
			* @brief Verifies that the physical device exposes the required swapchain extension.
			*
			* @param candidate Physical device whose device extensions are queried.
			* @return VK_SUCCESS when VK_KHR_swapchain is present, otherwise a Vulkan error code.
			*/
		[[nodiscard]] static VkResult supportsSwapchain(VkPhysicalDevice candidate) {
			std::uint32_t extensionCount{};
			VkResult result = vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
			if (result != VK_SUCCESS) { return result; }

			auto extensions = std::vector<VkExtensionProperties>(extensionCount);
			if (extensionCount != 0U) {
				result = vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data());
				if (result != VK_SUCCESS) { return result; }
			}

			const bool found = std::ranges::any_of(extensions, [](const VkExtensionProperties &extension) {
				return std::string_view{extension.extensionName} == swapchainExtensionName;
			});
			return found ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
		}
	};

	/// @brief Minimal Vulkan logical-device owner; no swapchain, render pass, commands, or sync are created here.
	struct VulkanDevice {
		VkDevice device{VK_NULL_HANDLE};              ///< Owned Vulkan logical device handle.
		VkQueue graphicsQueue{VK_NULL_HANDLE};        ///< Borrowed graphics queue retrieved from the device.
		VkQueue presentQueue{VK_NULL_HANDLE};         ///< Borrowed presentation queue retrieved from the device.

		VulkanDevice() = default;
		VulkanDevice(const VulkanDevice &) = delete;
		VulkanDevice &operator=(const VulkanDevice &) = delete;

		/**
			* @brief Creates a logical device for the selected physical device and retrieves its queues.
			*
			* @param selectedDevice Physical-device selection containing both required queue family indices.
			* @return Vulkan result from validation or vkCreateDevice.
		*/
		[[nodiscard]] VkResult create(const VulkanPhysicalDevice &selectedDevice) {
			cleanup();
			if (!selectedDevice.graphicsQueueFamily.has_value() || !selectedDevice.presentQueueFamily.has_value()) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			return create(selectedDevice.physicalDevice, *selectedDevice.graphicsQueueFamily, *selectedDevice.presentQueueFamily);
		}

		/**
			* @brief Creates a logical device and retrieves graphics and presentation queues.
			*
			* @param physicalDevice Vulkan physical device used to create the logical device.
			* @param graphicsQueueFamily Queue family index for graphics commands.
			* @param presentQueueFamily Queue family index for surface presentation.
			* @return VK_SUCCESS when the logical device and queues are available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, std::uint32_t graphicsQueueFamily, std::uint32_t presentQueueFamily) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const float queuePriority{1.0F};
			auto queueFamilies = std::vector<std::uint32_t>{};
			for (const std::uint32_t family : {graphicsQueueFamily, presentQueueFamily}) {
				const bool alreadyQueued = std::ranges::any_of(queueFamilies, [family](std::uint32_t queued) { return queued == family; });
				if (!alreadyQueued) { queueFamilies.push_back(family); }
			}

			auto queueInfos = std::vector<VkDeviceQueueCreateInfo>{};
			queueInfos.reserve(queueFamilies.size());
			for (const std::uint32_t family : queueFamilies) {
				queueInfos.push_back(VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = family,
					.queueCount = 1U,
					.pQueuePriorities = &queuePriority,
				});
			}

			const auto extensions = std::array<char const *, 1U>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
			const VkDeviceCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
				.pQueueCreateInfos = queueInfos.data(),
				.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.data(),
			};

			const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
			if (result != VK_SUCCESS) {
				device = VK_NULL_HANDLE;
				graphicsQueue = VK_NULL_HANDLE;
				presentQueue = VK_NULL_HANDLE;
				return result;
			}

			vkGetDeviceQueue(device, graphicsQueueFamily, 0U, &graphicsQueue);
			vkGetDeviceQueue(device, presentQueueFamily, 0U, &presentQueue);
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned logical device if one exists and clears borrowed queue handles.
			*/
		void cleanup() {
			if (device != VK_NULL_HANDLE) { vkDestroyDevice(device, nullptr); }
			device = VK_NULL_HANDLE;
			graphicsQueue = VK_NULL_HANDLE;
			presentQueue = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned logical device on scope exit.
			*/
		~VulkanDevice() { cleanup(); }
	};

	/// @brief Minimal Vulkan swapchain owner; no image views, render pass, commands, or sync are created here.
	struct VulkanSwapchain {
		VkSwapchainKHR swapchain{VK_NULL_HANDLE};     ///< Owned Vulkan swapchain handle.
		VkFormat imageFormat{VK_FORMAT_UNDEFINED};    ///< Chosen swapchain image format.
		VkExtent2D extent{};                          ///< Chosen swapchain image extent.
		std::vector<VkImage> images{};                ///< Borrowed images owned by the swapchain implementation.
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the swapchain.

		VulkanSwapchain() = default;
		VulkanSwapchain(const VulkanSwapchain &) = delete;
		VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

		/**
			* @brief Creates a surface swapchain and stores its implementation-owned images.
			*
			* @param physicalDevice Vulkan physical device used for surface capability queries.
			* @param owningDevice Logical device that owns the created swapchain.
			* @param surface Window surface targeted by presentation.
			* @param graphicsQueueFamily Queue family index used for graphics commands.
			* @param presentQueueFamily Queue family index used for presentation.
			* @param width Requested fallback width when the surface extent is not fixed.
			* @param height Requested fallback height when the surface extent is not fixed.
			* @return VK_SUCCESS when the swapchain and image list are available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			VkDevice owningDevice,
			VkSurfaceKHR surface,
			std::uint32_t graphicsQueueFamily,
			std::uint32_t presentQueueFamily,
			std::uint32_t width,
			std::uint32_t height
		) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VkSurfaceCapabilitiesKHR capabilities{};
			VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
			if (result != VK_SUCCESS) { return result; }

			std::uint32_t formatCount{};
			result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			if (formatCount == 0U) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
			auto formats = std::vector<VkSurfaceFormatKHR>(formatCount);
			result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
			if (result != VK_SUCCESS) { return result; }

			std::uint32_t presentModeCount{};
			result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			auto presentModes = std::vector<VkPresentModeKHR>(presentModeCount);
			if (presentModeCount != 0U) {
				result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
				if (result != VK_SUCCESS) { return result; }
			}

			const VkSurfaceFormatKHR chosenFormat = chooseFormat(formats);
			const VkPresentModeKHR presentMode = choosePresentMode(presentModes);
			const VkExtent2D chosenExtent = chooseExtent(capabilities, width, height);
			const std::uint32_t imageCount = chooseImageCount(capabilities);
			const auto queueFamilies = std::array<std::uint32_t, 2U>{graphicsQueueFamily, presentQueueFamily};
			const bool concurrentSharing = graphicsQueueFamily != presentQueueFamily;

			const VkSwapchainCreateInfoKHR createInfo{
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.surface = surface,
				.minImageCount = imageCount,
				.imageFormat = chosenFormat.format,
				.imageColorSpace = chosenFormat.colorSpace,
				.imageExtent = chosenExtent,
				.imageArrayLayers = 1U,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	///< Swapchain image is rendered to and copied from for readback.
				.imageSharingMode = concurrentSharing ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = concurrentSharing ? static_cast<std::uint32_t>(queueFamilies.size()) : 0U,
				.pQueueFamilyIndices = concurrentSharing ? queueFamilies.data() : nullptr,
				.preTransform = capabilities.currentTransform,
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = presentMode,
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE,
			};

			device = owningDevice;
			result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			imageFormat = chosenFormat.format;
			extent = chosenExtent;
			result = retrieveImages();
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned swapchain if one exists and clears borrowed image handles.
			*/
		void cleanup() {
			if (swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(device, swapchain, nullptr); }
			swapchain = VK_NULL_HANDLE;
			imageFormat = VK_FORMAT_UNDEFINED;
			extent = {};
			images.clear();
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned swapchain on scope exit.
			*/
		~VulkanSwapchain() { cleanup(); }

	private:
		static constexpr std::uint32_t variableExtent{0xFFFFFFFFU}; ///< Vulkan marker for application-selected surface extent.

		/**
			* @brief Selects the preferred SRGB surface format or the first reported format.
			*
			* @param formats Surface formats reported by Vulkan.
			* @return Preferred format when available, otherwise the first reported format.
			*/
		[[nodiscard]] static VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR> &formats) {
			const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR &format) {
				return format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			});
			return preferred != formats.end() ? *preferred : formats.front();
		}

		/**
			* @brief Selects mailbox presentation when available, otherwise FIFO.
			*
			* @param presentModes Surface present modes reported by Vulkan.
			* @return Preferred mailbox present mode or guaranteed FIFO fallback.
			*/
		[[nodiscard]] static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> &presentModes) {
			const bool hasMailbox = std::ranges::any_of(presentModes, [](VkPresentModeKHR mode) {
				return mode == VK_PRESENT_MODE_MAILBOX_KHR;
			});
			return hasMailbox ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
		}

		/**
			* @brief Selects the fixed surface extent or clamps the requested size into the supported range.
			*
			* @param capabilities Surface capabilities reported by Vulkan.
			* @param width Requested width for variable-size surfaces.
			* @param height Requested height for variable-size surfaces.
			* @return Surface image extent used by the swapchain.
			*/
		[[nodiscard]] static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities, std::uint32_t width, std::uint32_t height) {
			if (capabilities.currentExtent.width != variableExtent) { return capabilities.currentExtent; }
			return VkExtent2D{
				.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
				.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
			};
		}

		/**
			* @brief Selects one more image than the minimum while respecting a non-zero maximum.
			*
			* @param capabilities Surface capabilities reported by Vulkan.
			* @return Swapchain image count requested during creation.
			*/
		[[nodiscard]] static std::uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR &capabilities) {
			std::uint32_t imageCount = capabilities.minImageCount + 1U;
			if (capabilities.maxImageCount != 0U) { imageCount = std::min(imageCount, capabilities.maxImageCount); }
			return imageCount;
		}

		/**
			* @brief Retrieves the implementation-owned swapchain image handles.
			*
			* @return Vulkan result from vkGetSwapchainImagesKHR.
			*/
		[[nodiscard]] VkResult retrieveImages() {
			std::uint32_t imageCount{};
			VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			images.resize(imageCount);
			if (imageCount != 0U) {
				result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
				if (result != VK_SUCCESS) { images.clear(); return result; }
			}
			return VK_SUCCESS;
		}
	};

	/// @brief Minimal Vulkan image-view owner; no render pass, framebuffers, commands, or sync are created here.
	struct VulkanImageViews {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy image views.
		std::vector<VkImageView> views{};             ///< Owned image views created for borrowed swapchain images.

		VulkanImageViews() = default;
		VulkanImageViews(const VulkanImageViews &) = delete;
		VulkanImageViews &operator=(const VulkanImageViews &) = delete;

		/**
			* @brief Creates one color image view for each borrowed swapchain image.
			*
			* @param owningDevice Logical device that owns the image-view handles.
			* @param images Borrowed swapchain image handles; they are not destroyed by this object.
			* @param imageFormat Swapchain image format used by each created image view.
			* @return VK_SUCCESS when all image views were created, otherwise the first Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, const std::vector<VkImage> &images, VkFormat imageFormat) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			views.reserve(images.size());
			for (const VkImage image : images) {
				const VkImageViewCreateInfo createInfo{
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = image,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = imageFormat,
					.components = {
						.r = VK_COMPONENT_SWIZZLE_IDENTITY,
						.g = VK_COMPONENT_SWIZZLE_IDENTITY,
						.b = VK_COMPONENT_SWIZZLE_IDENTITY,
						.a = VK_COMPONENT_SWIZZLE_IDENTITY,
					},
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0U,
						.levelCount = 1U,
						.baseArrayLayer = 0U,
						.layerCount = 1U,
					},
				};

				VkImageView view{VK_NULL_HANDLE};
				const VkResult result = vkCreateImageView(device, &createInfo, nullptr, &view);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				views.push_back(view);
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned image views and clears the borrowed device handle.
			*/
		void cleanup() {
			for (const VkImageView view : views) {
				if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); }
			}
			views.clear();
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned image views on scope exit.
			*/
		~VulkanImageViews() { cleanup(); }
	};

	/// @brief Minimal Vulkan depth-attachment owner; no render pass, framebuffers, commands, or sync are created here.
	struct VulkanDepthImage {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the image resources.
		VkImage image{VK_NULL_HANDLE};                ///< Owned depth image handle.
		VkDeviceMemory memory{VK_NULL_HANDLE};        ///< Owned device-local memory backing the depth image.
		VkImageView view{VK_NULL_HANDLE};             ///< Owned depth image view used as a framebuffer attachment.

		VulkanDepthImage() = default;
		VulkanDepthImage(const VulkanDepthImage &) = delete;
		VulkanDepthImage &operator=(const VulkanDepthImage &) = delete;

		/**
			* @brief Creates a device-local D32 depth image and a matching depth image view.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the image, memory, and view.
			* @param extent Swapchain-sized image extent for the depth attachment.
			* @return VK_SUCCESS when the depth resources are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, VkDevice owningDevice, VkExtent2D extent) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || extent.width == 0U || extent.height == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			device = owningDevice;
			/// @brief Depth image descriptor for a single-sample 2D attachment.
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.extent = {.width = extent.width, .height = extent.height, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = 1U,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, image, &requirements);
			const std::optional<std::uint32_t> memoryType = findMemoryType(
				physicalDevice,
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);
			if (!memoryType.has_value()) { cleanup(); return VK_ERROR_FEATURE_NOT_PRESENT; }

			/// @brief Device-local allocation descriptor for the depth attachment image.
			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vkBindImageMemory(device, image, memory, 0U);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			/// @brief Depth-only view descriptor for framebuffer attachment use.
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
			};

			result = vkCreateImageView(device, &viewInfo, nullptr, &view);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned depth image view, image, and memory allocation.
			*/
		void cleanup() {
			if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); }
			if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); }
			if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); }
			view = VK_NULL_HANDLE;
			image = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned depth resources on scope exit.
			*/
		~VulkanDepthImage() { cleanup(); }

	private:
		friend struct ShadowMap; ///< Allows the shadow map to reuse the depth-image memory-type selector.
		friend struct TextureImage; ///< Allows texture uploads to reuse the image memory-type selector.

		/**
			* @brief Finds the first physical-device memory type satisfying a type mask and required properties.
			*
			* @param physicalDevice Physical device whose memory properties are queried.
			* @param typeFilter Bit mask of memory types compatible with the resource.
			* @param requiredProperties Required Vulkan memory-property flags.
			* @return The first compatible memory type index, or std::nullopt when none match.
			*/
		[[nodiscard]] static std::optional<std::uint32_t> findMemoryType(
			VkPhysicalDevice physicalDevice,
			std::uint32_t typeFilter,
			VkMemoryPropertyFlags requiredProperties
		) {
			VkPhysicalDeviceMemoryProperties memoryProperties{};
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

			for (std::uint32_t index{}; index < memoryProperties.memoryTypeCount; ++index) {
				const VkMemoryType &memoryType = memoryProperties.memoryTypes[index];
				const bool typeMatches = (typeFilter & (1U << index)) != 0U;
				const bool propertiesMatch = (memoryType.propertyFlags & requiredProperties) == requiredProperties;
				if (typeMatches && propertiesMatch) { return index; }
			}

			return std::nullopt;
		}
	};

	/// @brief Minimal standalone shadow-map owner; it is not bound or rendered by the forward renderer yet.
	struct ShadowMap {
		static constexpr std::uint32_t resolution{1024U};          ///< Fixed square shadow-map side length in pixels.
		VkDevice device{VK_NULL_HANDLE};                           ///< Borrowed Vulkan logical device used to destroy resources.
		VkImage image{VK_NULL_HANDLE};                             ///< Owned D32 depth image handle.
		VkDeviceMemory memory{VK_NULL_HANDLE};                     ///< Owned device-local memory backing the depth image.
		VkImageView view{VK_NULL_HANDLE};                          ///< Owned depth image view for attachment and sampling use.
		VkSampler sampler{VK_NULL_HANDLE};                         ///< Owned clamp sampler for later shadow-map reads.
		VkRenderPass renderPass{VK_NULL_HANDLE};                   ///< Owned depth-only render pass compatible with the shadow image.
		VkFramebuffer framebuffer{VK_NULL_HANDLE};                 ///< Owned square framebuffer attaching the shadow depth view.
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};           ///< Owned layout for frame uniforms and model push constants.
		VkPipeline pipeline{VK_NULL_HANDLE};                       ///< Owned depth-only shadow graphics pipeline, currently unused.

		ShadowMap() = default;
		ShadowMap(const ShadowMap &) = delete;
		ShadowMap &operator=(const ShadowMap &) = delete;

		/**
			* @brief Creates a square D32 depth image usable as a depth attachment and sampled image.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the image, memory, view, and sampler.
			* @return VK_SUCCESS when the shadow-map resources are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, VkDevice owningDevice) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			/// @brief Depth image descriptor for a fixed-size sampled shadow attachment.
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.extent = {.width = resolution, .height = resolution, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = 1U,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, image, &requirements);
			const std::optional<std::uint32_t> memoryType = VulkanDepthImage::findMemoryType(
				physicalDevice,
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);
			if (!memoryType.has_value()) { cleanup(); return VK_ERROR_FEATURE_NOT_PRESENT; }

			/// @brief Device-local allocation descriptor for the shadow-map image.
			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vkBindImageMemory(device, image, memory, 0U);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			/// @brief Depth-only view descriptor for future depth attachment and sampling use.
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_D32_SFLOAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
			};

			result = vkCreateImageView(device, &viewInfo, nullptr, &view);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			/// @brief Clamp sampler descriptor for future depth sampling without comparison state.
			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.minLod = 0.0F,
				.maxLod = 1.0F,
			};

			result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			const VkAttachmentDescription attachment{
				.format = VK_FORMAT_D32_SFLOAT,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkAttachmentReference depthReference{
				.attachment = 0U,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			};
			const VkSubpassDescription subpass{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.pDepthStencilAttachment = &depthReference,
			};
			/// @brief Shadow depth writes must be visible before the forward pass samples them.
			const std::array<VkSubpassDependency, 2U> dependencies{{
				{
					.srcSubpass = VK_SUBPASS_EXTERNAL,
					.dstSubpass = 0U,
					.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				},
				{
					.srcSubpass = 0U,
					.dstSubpass = VK_SUBPASS_EXTERNAL,
					.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				},
			}};
			const VkRenderPassCreateInfo renderPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 1U,
				.pAttachments = &attachment,
				.subpassCount = 1U,
				.pSubpasses = &subpass,
				.dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
				.pDependencies = dependencies.data(),
			};

			result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			const VkFramebufferCreateInfo framebufferInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = renderPass,
				.attachmentCount = 1U,
				.pAttachments = &view,
				.width = resolution,
				.height = resolution,
				.layers = 1U,
			};

			result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Creates the unused depth-only graphics pipeline for future shadow rendering.
			*
			* @param setLayout Existing frame-uniform descriptor-set layout used as set 0.
			* @param shadowVertexSpirvPath Path to simple_forward.shadow.vert.spv.
			* @param vertexInput Existing mesh vertex layout shared with the forward pipeline.
			* @return VK_SUCCESS when the shadow pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createPipeline(VkDescriptorSetLayout setLayout, std::string_view shadowVertexSpirvPath, const VulkanVertexInputDescription &vertexInput);

		/**
			* @brief Destroys the shadow graphics pipeline and pipeline layout before render-pass teardown.
			*/
		void cleanupPipeline() {
			if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr); }
			if (pipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); }
			pipeline = VK_NULL_HANDLE;
			pipelineLayout = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned shadow framebuffer, render pass, sampler, depth image view, image, and memory.
			*/
		void cleanup() {
			cleanupPipeline();
			if (framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(device, framebuffer, nullptr); }
			if (renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(device, renderPass, nullptr); }
			if (sampler != VK_NULL_HANDLE) { vkDestroySampler(device, sampler, nullptr); }
			if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); }
			if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); }
			if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); }
			framebuffer = VK_NULL_HANDLE;
			renderPass = VK_NULL_HANDLE;
			sampler = VK_NULL_HANDLE;
			view = VK_NULL_HANDLE;
			image = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned shadow-map resources on scope exit.
			*/
		~ShadowMap() { cleanup(); }
	};

	/// @brief Minimal Vulkan render-pass owner; no image views, pipeline, framebuffers, commands, or sync are created here.
	struct VulkanRenderPass {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the render pass.
		VkRenderPass renderPass{VK_NULL_HANDLE};      ///< Owned render pass handle for one forward color subpass.

		VulkanRenderPass() = default;
		VulkanRenderPass(const VulkanRenderPass &) = delete;
		VulkanRenderPass &operator=(const VulkanRenderPass &) = delete;

		/**
			* @brief Creates a single-subpass forward render pass with swapchain color and depth attachments.
			*
			* @param owningDevice Logical device that owns the created render pass.
			* @param colorFormat Swapchain color format used by attachment 0.
			* @param depthFormat Depth-stencil format used by attachment 1.
			* @return VK_SUCCESS when the render pass is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkFormat colorFormat, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const std::array<VkAttachmentDescription, 2> attachments{{
				{
					.format = colorFormat,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				},
				{
					.format = depthFormat,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
					.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				},
			}};
			const VkAttachmentReference colorReference{
				.attachment = 0U,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};
			const VkAttachmentReference depthReference{
				.attachment = 1U,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			};
			const VkSubpassDescription subpass{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1U,
				.pColorAttachments = &colorReference,
				.pDepthStencilAttachment = &depthReference,
			};
			const VkSubpassDependency dependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0U,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
					| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
					| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask = 0U,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			};
			const VkRenderPassCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
				.attachmentCount = 2U,
				.pAttachments = attachments.data(),
				.subpassCount = 1U,
				.pSubpasses = &subpass,
				.dependencyCount = 1U,
				.pDependencies = &dependency,
			};

			device = owningDevice;
			const VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, &renderPass);
			if (result != VK_SUCCESS) {
				renderPass = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned render pass and clears the borrowed device handle.
			*/
		void cleanup() {
			if (renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(device, renderPass, nullptr); }
			renderPass = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned render pass on scope exit.
			*/
		~VulkanRenderPass() { cleanup(); }
	};

	/// @brief Minimal Vulkan framebuffer owner; no pipeline, commands, or sync are created here.
	struct VulkanFramebuffers {
		VkDevice device{VK_NULL_HANDLE};                  ///< Borrowed Vulkan logical device used to destroy framebuffers.
		std::vector<VkFramebuffer> framebuffers{};        ///< Owned framebuffers matching the borrowed swapchain image views.

		VulkanFramebuffers() = default;
		VulkanFramebuffers(const VulkanFramebuffers &) = delete;
		VulkanFramebuffers &operator=(const VulkanFramebuffers &) = delete;

		/**
			* @brief Creates one framebuffer per swapchain image view with color attachment 0 and shared depth attachment 1.
			*
			* @param owningDevice Logical device that owns the created framebuffer handles.
			* @param renderPass Borrowed render pass compatible with color and depth attachments.
			* @param imageViews Borrowed swapchain image views used as color attachments at index 0.
			* @param extent Swapchain extent used as framebuffer width and height.
			* @param depthView Borrowed depth image view shared as attachment index 1 by all framebuffers.
			* @return VK_SUCCESS when all framebuffers were created, otherwise the first Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkDevice owningDevice,
			VkRenderPass renderPass,
			const std::vector<VkImageView> &imageViews,
			VkExtent2D extent,
			VkImageView depthView = VK_NULL_HANDLE
		) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			framebuffers.reserve(imageViews.size());
			for (const VkImageView imageView : imageViews) {
				const std::array<VkImageView, 2> attachments{imageView, depthView};
				const VkFramebufferCreateInfo createInfo{
					.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					.renderPass = renderPass,
					.attachmentCount = 2U,
					.pAttachments = attachments.data(),
					.width = extent.width,
					.height = extent.height,
					.layers = 1U,
				};

				VkFramebuffer framebuffer{VK_NULL_HANDLE};
				const VkResult result = vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				framebuffers.push_back(framebuffer);
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned framebuffers and clears the borrowed device handle.
			*/
		void cleanup() {
			for (const VkFramebuffer framebuffer : framebuffers) {
				if (framebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(device, framebuffer, nullptr); }
			}
			framebuffers.clear();
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned framebuffers on scope exit.
			*/
		~VulkanFramebuffers() { cleanup(); }
	};

	/// @brief Minimal Vulkan descriptor-set-layout owner for frame uniforms, the shadow map, and one object texture; no pipeline layout is created here.
	struct VulkanDescriptorSetLayout {
		VkDevice device{VK_NULL_HANDLE};                                  ///< Borrowed Vulkan logical device used to destroy the layout.
		VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};         ///< Owned descriptor-set layout for set 0 frame uniforms, shadow map, and object texture.

		VulkanDescriptorSetLayout() = default;
		VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout &) = delete;
		VulkanDescriptorSetLayout &operator=(const VulkanDescriptorSetLayout &) = delete;

		/**
			* @brief Creates set 0 with binding 0 as FrameUniforms, binding 1 as the shadow map, and binding 2 as one object texture.
			*
			* @param owningDevice Logical device that owns the created descriptor-set layout.
			* @return VK_SUCCESS when the descriptor-set layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorSetLayoutBinding frameUniformBinding{
				.binding = 0U,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			};
			const VkDescriptorSetLayoutBinding shadowMapBinding{
				.binding = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 1 exposes the sampled shadow map to the fragment shader.
			const VkDescriptorSetLayoutBinding objectTextureBinding{
				.binding = 2U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1U,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr,
			}; ///< Binding 2 reserves one sampled object base-color texture for later fragment shading.
			const std::array bindings{frameUniformBinding, shadowMapBinding, objectTextureBinding};
			const VkDescriptorSetLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(bindings.size()),
				.pBindings = bindings.data(),
			};

			device = owningDevice;
			const VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout);
			if (result != VK_SUCCESS) {
				descriptorSetLayout = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned descriptor-set layout and clears the borrowed device handle.
			*/
		void cleanup() {
			if (descriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); }
			descriptorSetLayout = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned descriptor-set layout on scope exit.
			*/
		~VulkanDescriptorSetLayout() { cleanup(); }
	};

	/// @brief Plain vertex input layout value matching the simple forward renderer Vertex attributes.
	struct VulkanVertexInputDescription {
		VkVertexInputBindingDescription binding{
			.binding = 0U,
			.stride = sizeof(vve::simple::Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		}; ///< Binding 0 consumes one complete Vertex per input vertex.
		std::array<VkVertexInputAttributeDescription, 3> attributes{{
			{.location = 0U, .binding = 0U, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vve::simple::Vertex, position)},
			{.location = 1U, .binding = 0U, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(vve::simple::Vertex, color)},
			{.location = 2U, .binding = 0U, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(vve::simple::Vertex, texCoord)},
		}}; ///< Position, color, and texture-coordinate attributes consumed by the vertex shader.
	};

	/// @brief Minimal Vulkan pipeline-layout owner; no graphics pipeline, commands, or sync are created here.
	struct VulkanPipelineLayout {
		VkDevice device{VK_NULL_HANDLE};                  ///< Borrowed Vulkan logical device used to destroy the pipeline layout.
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};   ///< Owned pipeline layout for set 0 and model push constants.

		VulkanPipelineLayout() = default;
		VulkanPipelineLayout(const VulkanPipelineLayout &) = delete;
		VulkanPipelineLayout &operator=(const VulkanPipelineLayout &) = delete;

		/**
			* @brief Creates a pipeline layout with one descriptor set and one vertex-visible object push constant range.
			*
			* @param owningDevice Logical device that owns the created pipeline layout.
			* @param setLayout Descriptor-set layout used as set 0 by the graphics pipeline.
			* @return VK_SUCCESS when the pipeline layout is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkDescriptorSetLayout setLayout) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkPushConstantRange modelPushConstants{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.offset = 0U,
				.size = sizeof(ObjectPushConstants),
			};
			const VkPipelineLayoutCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.setLayoutCount = 1U,
				.pSetLayouts = &setLayout,
				.pushConstantRangeCount = 1U,
				.pPushConstantRanges = &modelPushConstants,
			};

			device = owningDevice;
			const VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout);
			if (result != VK_SUCCESS) {
				pipelineLayout = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned pipeline layout and clears the borrowed device handle.
			*/
		void cleanup() {
			if (pipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); }
			pipelineLayout = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned pipeline layout on scope exit.
			*/
		~VulkanPipelineLayout() { cleanup(); }
	};

	/// @brief Minimal Vulkan shader-module owner; no layouts, pipelines, commands, or sync are created here.
	struct VulkanShaderModule {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the shader module.
		VkShaderModule shaderModule{VK_NULL_HANDLE};  ///< Owned shader module created from a SPIR-V binary.

		VulkanShaderModule() = default;
		VulkanShaderModule(const VulkanShaderModule &) = delete;
		VulkanShaderModule &operator=(const VulkanShaderModule &) = delete;

		/**
			* @brief Loads a SPIR-V binary and creates a Vulkan shader module.
			*
			* @param owningDevice Logical device that owns the created shader module.
			* @param spirvPath Path to a binary SPIR-V file whose size is a multiple of 32-bit words.
			* @return VK_SUCCESS when the shader module is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, std::string_view spirvPath) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			auto file = std::ifstream{std::string{spirvPath}, std::ios::binary | std::ios::ate};
			if (!file.is_open()) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto fileSize = static_cast<std::streamoff>(file.tellg());
			if (fileSize <= 0) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto byteCount = static_cast<std::size_t>(fileSize);
			if (byteCount % sizeof(std::uint32_t) != 0U) { return VK_ERROR_INITIALIZATION_FAILED; }
			auto code = std::vector<std::uint32_t>(byteCount / sizeof(std::uint32_t)); // Stores aligned 32-bit SPIR-V words.
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(byteCount));
			if (!file) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkShaderModuleCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = byteCount,
				.pCode = code.data(),
			};

			device = owningDevice;
			const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
			if (result != VK_SUCCESS) {
				shaderModule = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned shader module and clears the borrowed device handle.
			*/
		void cleanup() {
			if (shaderModule != VK_NULL_HANDLE) { vkDestroyShaderModule(device, shaderModule, nullptr); }
			shaderModule = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned shader module on scope exit.
			*/
		~VulkanShaderModule() { cleanup(); }
	};

	/**
		* @brief Creates the fixed-function shadow graphics pipeline for the depth-only shadow render pass.
		*
		* @param setLayout Existing frame-uniform descriptor-set layout used as set 0.
		* @param shadowVertexSpirvPath Path to the shadow vertex SPIR-V binary.
		* @param vertexInput Existing mesh vertex binding and attribute description.
		* @return VK_SUCCESS when the pipeline layout and pipeline are ready, otherwise a Vulkan error code.
		*/
	[[nodiscard]] VkResult ShadowMap::createPipeline(VkDescriptorSetLayout setLayout, std::string_view shadowVertexSpirvPath, const VulkanVertexInputDescription &vertexInput) {
		cleanupPipeline();
		if (device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		VulkanShaderModule shadowVertexModule{}; // Temporary module is only needed while creating the pipeline.
		VkResult result = shadowVertexModule.create(device, shadowVertexSpirvPath);
		if (result != VK_SUCCESS) { return result; }

		const VkPushConstantRange modelPushConstants{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0U,
			.size = sizeof(ObjectPushConstants),
		};
		const VkPipelineLayoutCreateInfo layoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1U,
			.pSetLayouts = &setLayout,
			.pushConstantRangeCount = 1U,
			.pPushConstantRanges = &modelPushConstants,
		};

		result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) { cleanupPipeline(); return result; }

		constexpr char vertexEntry[]{"shadowVertexMain"};
		const VkPipelineShaderStageCreateInfo shaderStage{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shadowVertexModule.shaderModule,
			.pName = vertexEntry,
		};

		const VkVertexInputBindingDescription &binding = vertexInput.binding;
		const auto &attributes = vertexInput.attributes;
		const VkPipelineVertexInputStateCreateInfo vertexInputState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1U,
			.pVertexBindingDescriptions = &binding,
			.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
			.pVertexAttributeDescriptions = attributes.data(),
		};
		const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};
		const VkExtent2D extent{.width = resolution, .height = resolution};
		const VkViewport viewport{
			.x = 0.0F,
			.y = 0.0F,
			.width = static_cast<float>(extent.width),
			.height = static_cast<float>(extent.height),
			.minDepth = 0.0F,
			.maxDepth = 1.0F,
		};
		const VkRect2D scissor{.offset = {.x = 0, .y = 0}, .extent = extent};
		const VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1U,
			.pViewports = &viewport,
			.scissorCount = 1U,
			.pScissors = &scissor,
		};
		const VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,			///< Shadow pass renders all faces because orthoVulkan Y-flip inverts winding.
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_TRUE,
			.depthBiasConstantFactor = 0.0F,			///< Constant depth bias is removed for visible contact shadows.
			.depthBiasSlopeFactor = 0.0F,			///< Slope depth bias is removed for visible contact shadows.
			.lineWidth = 1.0F,
		};
		const VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};
		const VkPipelineColorBlendStateCreateInfo colorBlending{ ///< No color attachments or fragment shader are used.
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.attachmentCount = 0U,
			.pAttachments = nullptr,
		};
		const VkPipelineDepthStencilStateCreateInfo depthStencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
		};
		const VkGraphicsPipelineCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 1U,
			.pStages = &shaderStage,
			.pVertexInputState = &vertexInputState,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = &colorBlending,
			.pDynamicState = nullptr,
			.layout = pipelineLayout,
			.renderPass = renderPass,
			.subpass = 0U,
			.basePipelineHandle = VK_NULL_HANDLE,
		};

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &pipeline);
		if (result != VK_SUCCESS) { cleanupPipeline(); }
		return result;
	}

	/// @brief Minimal Vulkan graphics-pipeline owner for the simple forward color pass.
	struct VulkanGraphicsPipeline {
		VkDevice device{VK_NULL_HANDLE};      ///< Borrowed Vulkan logical device used to destroy the pipeline.
		VkPipeline pipeline{VK_NULL_HANDLE};  ///< Owned graphics pipeline for the simple forward pass.

		VulkanGraphicsPipeline() = default;
		VulkanGraphicsPipeline(const VulkanGraphicsPipeline &) = delete;
		VulkanGraphicsPipeline &operator=(const VulkanGraphicsPipeline &) = delete;

		/**
			* @brief Creates the fixed-function graphics pipeline for the simple forward render pass.
			*
			* @param owningDevice Logical device that owns the created pipeline.
			* @param renderPass Borrowed render pass compatible with one color attachment.
			* @param pipelineLayout Borrowed pipeline layout used by the shader stages.
			* @param vertexModule Borrowed vertex shader module with entry point vertexMain.
			* @param fragmentModule Borrowed fragment shader module with entry point fragmentMain.
			* @param vertexInput Borrowed vertex binding and attribute description used by the pipeline.
			* @param extent Swapchain extent used for the static viewport and scissor.
			* @return VK_SUCCESS when the graphics pipeline is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkDevice owningDevice,
			VkRenderPass renderPass,
			VkPipelineLayout pipelineLayout,
			VkShaderModule vertexModule,
			VkShaderModule fragmentModule,
			const VulkanVertexInputDescription &vertexInput,
			VkExtent2D extent
		) {
			cleanup();
			if (
				owningDevice == VK_NULL_HANDLE ||
				renderPass == VK_NULL_HANDLE ||
				pipelineLayout == VK_NULL_HANDLE ||
				vertexModule == VK_NULL_HANDLE ||
				fragmentModule == VK_NULL_HANDLE
			) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			constexpr char vertexEntry[]{"vertexMain"};
			constexpr char fragmentEntry[]{"fragmentMain"};
			const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{{
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.module = vertexModule,
					.pName = vertexEntry,
				},
				{
					.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.module = fragmentModule,
					.pName = fragmentEntry,
				},
			}};

			const VkVertexInputBindingDescription &binding = vertexInput.binding;
			const auto &attributes = vertexInput.attributes;
			const VkPipelineVertexInputStateCreateInfo vertexInputState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.vertexBindingDescriptionCount = 1U,
				.pVertexBindingDescriptions = &binding,
				.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
				.pVertexAttributeDescriptions = attributes.data(),
			};
			const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				.primitiveRestartEnable = VK_FALSE,
			};
			const VkViewport viewport{
				.x = 0.0F,
				.y = 0.0F,
				.width = static_cast<float>(extent.width),
				.height = static_cast<float>(extent.height),
				.minDepth = 0.0F,
				.maxDepth = 1.0F,
			};
			const VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = extent,
			};
			const VkPipelineViewportStateCreateInfo viewportState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount = 1U,
				.pViewports = &viewport,
				.scissorCount = 1U,
				.pScissors = &scissor,
			};
			const VkPipelineRasterizationStateCreateInfo rasterizer{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_BACK_BIT,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthBiasEnable = VK_FALSE,
				.lineWidth = 1.0F,
			};
			const VkPipelineMultisampleStateCreateInfo multisampling{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = VK_FALSE,
			};
			const VkPipelineColorBlendAttachmentState colorBlendAttachment{
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			};
			const VkPipelineColorBlendStateCreateInfo colorBlending{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = VK_FALSE,
				.attachmentCount = 1U,
				.pAttachments = &colorBlendAttachment,
			};
			const VkPipelineDepthStencilStateCreateInfo depthStencil{ ///< Enables nearest visible fragments to write depth.
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_TRUE,
				.depthWriteEnable = VK_TRUE,
				.depthCompareOp = VK_COMPARE_OP_LESS,
				.depthBoundsTestEnable = VK_FALSE,
				.stencilTestEnable = VK_FALSE,
			};
			const VkGraphicsPipelineCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.stageCount = static_cast<std::uint32_t>(shaderStages.size()),
				.pStages = shaderStages.data(),
				.pVertexInputState = &vertexInputState,
				.pInputAssemblyState = &inputAssembly,
				.pViewportState = &viewportState,
				.pRasterizationState = &rasterizer,
				.pMultisampleState = &multisampling,
				.pDepthStencilState = &depthStencil,
				.pColorBlendState = &colorBlending,
				.pDynamicState = nullptr,
				.layout = pipelineLayout,
				.renderPass = renderPass,
				.subpass = 0U,
				.basePipelineHandle = VK_NULL_HANDLE,
			};

			device = owningDevice;
			const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &createInfo, nullptr, &pipeline);
			if (result != VK_SUCCESS) {
				pipeline = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned graphics pipeline and clears the borrowed device handle.
			*/
		void cleanup() {
			if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr); }
			pipeline = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned graphics pipeline on scope exit.
			*/
		~VulkanGraphicsPipeline() { cleanup(); }
	};

	/// @brief Minimal Vulkan command-pool owner for resettable graphics command buffers.
	struct VulkanCommandPool {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the command pool.
		VkCommandPool commandPool{VK_NULL_HANDLE};    ///< Owned command pool for the graphics queue family.

		VulkanCommandPool() = default;
		VulkanCommandPool(const VulkanCommandPool &) = delete;
		VulkanCommandPool &operator=(const VulkanCommandPool &) = delete;

		/**
			* @brief Creates a resettable command pool for the graphics queue family.
			*
			* @param owningDevice Logical device that owns the created command pool.
			* @param graphicsQueueFamily Queue family index used for graphics commands.
			* @return VK_SUCCESS when the command pool is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, std::uint32_t graphicsQueueFamily) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandPoolCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = graphicsQueueFamily,
			};

			device = owningDevice;
			const VkResult result = vkCreateCommandPool(device, &createInfo, nullptr, &commandPool);
			if (result != VK_SUCCESS) {
				commandPool = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned command pool and clears the borrowed device handle.
			*/
		void cleanup() {
			if (commandPool != VK_NULL_HANDLE) { vkDestroyCommandPool(device, commandPool, nullptr); }
			commandPool = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned command pool on scope exit.
			*/
		~VulkanCommandPool() { cleanup(); }
	};

	/// @brief Minimal Vulkan command-buffer owner for primary command buffers allocated from a borrowed pool.
	struct VulkanCommandBuffers {
		VkDevice device{VK_NULL_HANDLE};                       ///< Borrowed Vulkan logical device used to free command buffers.
		VkCommandPool commandPool{VK_NULL_HANDLE};             ///< Borrowed command pool that owns the command-buffer allocations.
		std::vector<VkCommandBuffer> commandBuffers{};         ///< Owned primary command buffers freed back to the borrowed pool.

		VulkanCommandBuffers() = default;
		VulkanCommandBuffers(const VulkanCommandBuffers &) = delete;
		VulkanCommandBuffers &operator=(const VulkanCommandBuffers &) = delete;

		/**
			* @brief Allocates primary command buffers from an existing command pool.
			*
			* @param owningDevice Logical device that owns the command pool.
			* @param owningPool Command pool used for command-buffer allocation and release.
			* @param count Number of primary command buffers requested.
			* @return VK_SUCCESS when all command buffers are allocated, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkCommandPool owningPool, std::uint32_t count) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || owningPool == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			commandPool = owningPool;
			commandBuffers.resize(count);
			const VkCommandBufferAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = owningPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = count,
			};

			const VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data());
			if (result != VK_SUCCESS) {
				commandBuffers.clear();
				device = VK_NULL_HANDLE;
				commandPool = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Frees the owned command buffers and clears borrowed handles.
			*/
		void cleanup() {
			if (commandPool != VK_NULL_HANDLE && !commandBuffers.empty()) {
				vkFreeCommandBuffers(device, commandPool, static_cast<std::uint32_t>(commandBuffers.size()), commandBuffers.data());
			}
			commandBuffers.clear();
			device = VK_NULL_HANDLE;
			commandPool = VK_NULL_HANDLE;
		}

		/**
			* @brief Frees the owned command buffers on scope exit.
			*/
		~VulkanCommandBuffers() { cleanup(); }
	};

	/// @brief Minimal Vulkan frame-synchronization owner for per-frame rendering primitives.
	struct VulkanFrameSync {
		VkDevice device{VK_NULL_HANDLE};                              ///< Borrowed Vulkan logical device used to destroy sync objects.
		std::vector<VkSemaphore> imageAvailableSemaphores{};          ///< Owned semaphores signaled when swapchain images are ready.
		std::vector<VkSemaphore> renderFinishedSemaphores{};          ///< Owned semaphores signaled when rendering has finished.
		std::vector<VkFence> inFlightFences{};                        ///< Owned fences tracking submitted work for each frame slot.

		VulkanFrameSync() = default;
		VulkanFrameSync(const VulkanFrameSync &) = delete;
		VulkanFrameSync &operator=(const VulkanFrameSync &) = delete;

		/**
			* @brief Creates frame-indexed acquire sync and image-indexed present-wait semaphores.
			*
			* @param owningDevice Logical device that owns the created synchronization primitives.
			* @param framesInFlight Number of frame slots requiring acquire semaphores and fences.
			* @param swapchainImageCount Number of swapchain images requiring independent present-wait semaphores.
			* @return VK_SUCCESS when all synchronization primitives are available, otherwise the first Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, std::uint32_t framesInFlight, std::uint32_t swapchainImageCount) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || framesInFlight == 0U || swapchainImageCount == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }

			device = owningDevice;
			imageAvailableSemaphores.reserve(framesInFlight);
			renderFinishedSemaphores.reserve(swapchainImageCount);
			inFlightFences.reserve(framesInFlight);

			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				/// @brief Semaphore creation descriptor using default binary semaphore behavior.
				const VkSemaphoreCreateInfo semaphoreInfo{
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				};
				/// @brief Fence creation descriptor starts signaled so the first frame can proceed.
				const VkFenceCreateInfo fenceInfo{
					.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
					.flags = VK_FENCE_CREATE_SIGNALED_BIT,
				};

				VkSemaphore imageAvailable{VK_NULL_HANDLE};
				VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				imageAvailableSemaphores.push_back(imageAvailable);

				VkFence inFlight{VK_NULL_HANDLE};
				result = vkCreateFence(device, &fenceInfo, nullptr, &inFlight);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				inFlightFences.push_back(inFlight);
			}

			// Present waits can outlive a frame slot, so each swapchain image owns its signal semaphore.
			for (std::uint32_t image{}; image < swapchainImageCount; ++image) {
				const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
				VkSemaphore renderFinished{VK_NULL_HANDLE};
				const VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				renderFinishedSemaphores.push_back(renderFinished);
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned frame synchronization primitives and clears the borrowed device handle.
			*/
		void cleanup() {
			for (const VkSemaphore semaphore : imageAvailableSemaphores) {
				if (semaphore != VK_NULL_HANDLE) { vkDestroySemaphore(device, semaphore, nullptr); }
			}
			for (const VkSemaphore semaphore : renderFinishedSemaphores) {
				if (semaphore != VK_NULL_HANDLE) { vkDestroySemaphore(device, semaphore, nullptr); }
			}
			for (const VkFence fence : inFlightFences) {
				if (fence != VK_NULL_HANDLE) { vkDestroyFence(device, fence, nullptr); }
			}
			imageAvailableSemaphores.clear();
			renderFinishedSemaphores.clear();
			inFlightFences.clear();
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned frame synchronization primitives on scope exit.
			*/
		~VulkanFrameSync() { cleanup(); }
	};

	/// @brief Minimal Vulkan buffer owner for one buffer and its device-memory allocation.
	struct VulkanBuffer {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy the buffer and free memory.
		VkBuffer buffer{VK_NULL_HANDLE};              ///< Owned Vulkan buffer handle.
		VkDeviceMemory memory{VK_NULL_HANDLE};        ///< Owned Vulkan device-memory allocation backing the buffer.
		VkDeviceSize size{0};                         ///< Requested buffer size in bytes.

		VulkanBuffer() = default;
		VulkanBuffer(const VulkanBuffer &) = delete;
		VulkanBuffer &operator=(const VulkanBuffer &) = delete;

		/**
			* @brief Transfers one owned buffer allocation and leaves the source empty.
			*/
		VulkanBuffer(VulkanBuffer &&other) noexcept
			: device{std::exchange(other.device, VK_NULL_HANDLE)},
				buffer{std::exchange(other.buffer, VK_NULL_HANDLE)},
				memory{std::exchange(other.memory, VK_NULL_HANDLE)},
				size{std::exchange(other.size, 0U)} {}

		/**
			* @brief Replaces this allocation with another owned buffer allocation.
			*
			* @return This buffer after taking ownership from the source.
			*/
		VulkanBuffer &operator=(VulkanBuffer &&other) noexcept {
			if (this != &other) {
				cleanup();
				device = std::exchange(other.device, VK_NULL_HANDLE);
				buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
				memory = std::exchange(other.memory, VK_NULL_HANDLE);
				size = std::exchange(other.size, 0U);
			}
			return *this;
		}

		/**
			* @brief Creates a buffer, allocates suitable device memory, and binds the allocation to the buffer.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the buffer and memory allocation.
			* @param bufferSize Requested buffer size in bytes.
			* @param usage Vulkan usage flags for the buffer.
			* @param memoryProperties Required memory-property flags for the backing allocation.
			* @return VK_SUCCESS when the buffer is ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			VkDevice owningDevice,
			VkDeviceSize bufferSize,
			VkBufferUsageFlags usage,
			VkMemoryPropertyFlags memoryProperties
		) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || bufferSize == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			device = owningDevice;
			size = bufferSize;
			/// @brief Buffer creation descriptor for an exclusive-queue buffer.
			const VkBufferCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = bufferSize,
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			};

			VkResult result = vkCreateBuffer(device, &createInfo, nullptr, &buffer);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VkMemoryRequirements requirements{};
			vkGetBufferMemoryRequirements(device, buffer, &requirements);
			const std::optional<std::uint32_t> memoryType = findMemoryType(physicalDevice, requirements.memoryTypeBits, memoryProperties);
			if (!memoryType.has_value()) { cleanup(); return VK_ERROR_FEATURE_NOT_PRESENT; }

			/// @brief Memory allocation descriptor for the selected compatible memory type.
			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vkBindBufferMemory(device, buffer, memory, 0U);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Uploads bytes into this host-visible buffer allocation.
			*
			* @param data Source bytes copied into the mapped allocation.
			* @param byteCount Number of bytes copied from the source pointer.
			* @return VK_SUCCESS after copying, VK_ERROR_INITIALIZATION_FAILED for invalid input, or a vkMapMemory error.
			*/
		[[nodiscard]] VkResult upload(const void *data, VkDeviceSize byteCount) {
			if (memory == VK_NULL_HANDLE || data == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (byteCount == 0U) { return VK_SUCCESS; }
			if (byteCount > size) { return VK_ERROR_INITIALIZATION_FAILED; }

			void *mapped{};
			const VkResult result = vkMapMemory(device, memory, 0U, byteCount, 0U, &mapped);
			if (result != VK_SUCCESS) { return result; }

			std::memcpy(mapped, data, byteCount);
			vkUnmapMemory(device, memory);
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned buffer, frees its memory, and clears the borrowed device handle.
			*/
		void cleanup() {
			if (buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, buffer, nullptr); }
			if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); }
			buffer = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
			size = 0U;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned buffer and frees its memory on scope exit.
			*/
		~VulkanBuffer() { cleanup(); }

	private:
		/**
			* @brief Finds the first physical-device memory type satisfying a type mask and required properties.
			*
			* @param physicalDevice Physical device whose memory properties are queried.
			* @param typeFilter Bit mask of memory types compatible with the resource.
			* @param requiredProperties Required Vulkan memory-property flags.
			* @return The first compatible memory type index, or std::nullopt when none match.
			*/
		[[nodiscard]] static std::optional<std::uint32_t> findMemoryType(
			VkPhysicalDevice physicalDevice,
			std::uint32_t typeFilter,
			VkMemoryPropertyFlags requiredProperties
		) {
			VkPhysicalDeviceMemoryProperties memoryProperties{};
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

			for (std::uint32_t index{}; index < memoryProperties.memoryTypeCount; ++index) {
				const VkMemoryType &memoryType = memoryProperties.memoryTypes[index];
				const bool typeMatches = (typeFilter & (1U << index)) != 0U;
				const bool propertiesMatch = (memoryType.propertyFlags & requiredProperties) == requiredProperties;
				if (typeMatches && propertiesMatch) { return index; }
			}

			return std::nullopt;
		}
	};

	/// @brief Minimal Vulkan color-image readback owner for one host-visible transfer destination buffer.
	struct VulkanReadback {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used for commands, fences, and mapping.
		VkQueue graphicsQueue{VK_NULL_HANDLE};        ///< Borrowed graphics queue used to submit the one-time copy command buffer.
		VkCommandPool commandPool{VK_NULL_HANDLE};    ///< Borrowed command pool used to allocate the temporary command buffer.
		VkExtent2D extent{};                          ///< Captured image size in pixels.
		VkFormat format{VK_FORMAT_UNDEFINED};         ///< Captured color-image format used to compute byte size.
		VulkanBuffer buffer{};                        ///< Owned host-visible transfer destination buffer.
		std::vector<std::byte> pixels{};              ///< CPU copy of the mapped readback buffer bytes.

		VulkanReadback() = default;
		VulkanReadback(const VulkanReadback &) = delete;
		VulkanReadback &operator=(const VulkanReadback &) = delete;

		/**
			* @brief Creates a host-visible transfer destination buffer sized for one color image.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the readback buffer and temporary synchronization objects.
			* @param queue Graphics queue used later for the one-time copy submission.
			* @param pool Command pool used later for one temporary primary command buffer.
			* @param imageExtent Width and height of the source color image.
			* @param imageFormat Color format used to derive the readback byte count.
			* @return VK_SUCCESS when the readback buffer is ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			VkDevice owningDevice,
			VkQueue queue,
			VkCommandPool pool,
			VkExtent2D imageExtent,
			VkFormat imageFormat
		) {
			cleanup();
			const std::optional<VkDeviceSize> bytesPerPixel = formatByteSize(imageFormat);
			if (
				physicalDevice == VK_NULL_HANDLE ||
				owningDevice == VK_NULL_HANDLE ||
				queue == VK_NULL_HANDLE ||
				pool == VK_NULL_HANDLE ||
				imageExtent.width == 0U ||
				imageExtent.height == 0U ||
				!bytesPerPixel.has_value()
			) {
				return !bytesPerPixel.has_value() ? VK_ERROR_FORMAT_NOT_SUPPORTED : VK_ERROR_INITIALIZATION_FAILED;
			}

			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(imageExtent.width) * imageExtent.height * *bytesPerPixel;
			VkResult result = buffer.create(
				physicalDevice,
				owningDevice,
				byteCount,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			device = owningDevice;
			graphicsQueue = queue;
			commandPool = pool;
			extent = imageExtent;
			format = imageFormat;
			pixels.resize(static_cast<std::size_t>(byteCount));
			return VK_SUCCESS;
		}

		/**
			* @brief Copies a finished color image into the host-visible buffer and refreshes CPU pixel bytes.
			*
			* @param sourceImage Borrowed color image to copy from.
			* @param sourceLayout Current image layout before the transfer-source transition.
			* @param finalLayout Image layout restored after the copy command.
			* @return VK_SUCCESS when pixels contains the readback bytes, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult capture(
			VkImage sourceImage,
			VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		) {
			if (sourceImage == VK_NULL_HANDLE || buffer.buffer == VK_NULL_HANDLE || buffer.memory == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
			const VkCommandBufferAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1U,
			};
			VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			VkFence fence{VK_NULL_HANDLE};
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
			if (result != VK_SUCCESS) { vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer); return result; }

			result = recordCopy(commandBuffer, sourceImage, sourceLayout, finalLayout);
			if (result == VK_SUCCESS) {
				const VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1U,
					.pCommandBuffers = &commandBuffer,
				};
				result = vkQueueSubmit(graphicsQueue, 1U, &submitInfo, fence);
			}
			if (result == VK_SUCCESS) { result = vkWaitForFences(device, 1U, &fence, VK_TRUE, UINT64_MAX); }
			if (result == VK_SUCCESS) { result = readMappedPixels(); }

			vkDestroyFence(device, fence, nullptr);
			vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer);
			return result;
		}

		/**
			* @brief Returns the last captured raw image bytes in Vulkan format order.
			*
			* @return Read-only span over the CPU-side pixel byte vector.
			*/
		[[nodiscard]] std::span<const std::byte> pixelBytes() const { return pixels; }

		/**
			* @brief Releases the owned readback buffer and clears borrowed handles.
			*/
		void cleanup() {
			buffer.cleanup();
			pixels.clear();
			extent = {};
			format = VK_FORMAT_UNDEFINED;
			graphicsQueue = VK_NULL_HANDLE;
			commandPool = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned readback buffer on scope exit.
			*/
		~VulkanReadback() { cleanup(); }

	private:
		friend struct TextureImage; ///< Allows texture upload commands to reuse the local color-image transition helper.

		/**
			* @brief Records the layout transitions and image-to-buffer copy into one primary command buffer.
			*
			* @param commandBuffer Temporary primary command buffer to record.
			* @param sourceImage Borrowed color image copied into the readback buffer.
			* @param sourceLayout Layout expected before the copy.
			* @param finalLayout Layout restored after the copy.
			* @return VK_SUCCESS when recording completed, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult recordCopy(
			VkCommandBuffer commandBuffer,
			VkImage sourceImage,
			VkImageLayout sourceLayout,
			VkImageLayout finalLayout
		) {
			const VkCommandBufferBeginInfo beginInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			transitionImage(commandBuffer, sourceImage, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			const VkBufferImageCopy copyRegion{
				.bufferOffset = 0U,
				.bufferRowLength = 0U,
				.bufferImageHeight = 0U,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
				.imageOffset = {.x = 0, .y = 0, .z = 0},
				.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
			};
			vkCmdCopyImageToBuffer(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.buffer, 1U, &copyRegion);
			transitionImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, finalLayout);
			return vkEndCommandBuffer(commandBuffer);
		}

		/**
			* @brief Copies the host-visible readback allocation into the CPU byte vector.
			*
			* @return VK_SUCCESS when the memory mapping and copy completed, otherwise a vkMapMemory result.
			*/
		[[nodiscard]] VkResult readMappedPixels() {
			void *mapped{};
			const VkResult result = vkMapMemory(device, buffer.memory, 0U, buffer.size, 0U, &mapped);
			if (result != VK_SUCCESS) { return result; }
			pixels.resize(static_cast<std::size_t>(buffer.size));
			std::memcpy(pixels.data(), mapped, static_cast<std::size_t>(buffer.size));
			vkUnmapMemory(device, buffer.memory);
			return VK_SUCCESS;
		}

		/**
			* @brief Records a color-image layout transition for one mip level and one array layer.
			*
			* @param commandBuffer Command buffer receiving the barrier.
			* @param image Color image being transitioned.
			* @param oldLayout Layout before the barrier.
			* @param newLayout Layout after the barrier.
			*/
		static void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
			if (oldLayout == newLayout) { return; }
			const VkImageMemoryBarrier barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = accessMask(oldLayout, false),
				.dstAccessMask = accessMask(newLayout, true),
				.oldLayout = oldLayout,
				.newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
			};
			vkCmdPipelineBarrier(commandBuffer, stageMask(oldLayout), stageMask(newLayout), 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);
		}

		/**
			* @brief Selects the conservative pipeline stage used for a color-image layout.
			*
			* @param layout Vulkan image layout being synchronized.
			* @return Pipeline stage compatible with the supported color-image layouts.
			*/
		[[nodiscard]] static VkPipelineStageFlags stageMask(VkImageLayout layout) {
			if (layout == VK_IMAGE_LAYOUT_UNDEFINED) { return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { return VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { return VK_PIPELINE_STAGE_TRANSFER_BIT; }
			if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
			if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; }
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}

		/**
			* @brief Selects the memory access mask used for a color-image layout.
			*
			* @param layout Vulkan image layout being synchronized.
			* @param destination True when the layout is the barrier destination layout.
			* @return Access mask compatible with the supported color-image layouts.
			*/
		[[nodiscard]] static VkAccessFlags accessMask(VkImageLayout layout, bool destination) {
			if (layout == VK_IMAGE_LAYOUT_UNDEFINED) { return 0U; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { return VK_ACCESS_TRANSFER_READ_BIT; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { return VK_ACCESS_TRANSFER_WRITE_BIT; }
			if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { return destination ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_READ_BIT; }
			if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { return destination ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; }
			return 0U;
		}

		/**
			* @brief Returns the byte width of supported color formats used by the simple swapchain.
			*
			* @param imageFormat Vulkan color format being read back.
			* @return Bytes per pixel for supported formats, otherwise std::nullopt.
			*/
		[[nodiscard]] static std::optional<VkDeviceSize> formatByteSize(VkFormat imageFormat) {
			switch (imageFormat) {
				case VK_FORMAT_R8G8B8A8_UNORM:
				case VK_FORMAT_R8G8B8A8_SRGB:
				case VK_FORMAT_B8G8R8A8_UNORM:
				case VK_FORMAT_B8G8R8A8_SRGB:
				case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
				case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
					return 4U;
				default:
					return std::nullopt;
			}
		}
	};

	/// @brief Internal owner for one sampled 8-bit RGBA texture image uploaded with a staging buffer.
	struct TextureImage {
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used to destroy texture resources.
		VkImage image{VK_NULL_HANDLE};                ///< Owned device-local sampled color image.
		VkDeviceMemory memory{VK_NULL_HANDLE};        ///< Owned device-local memory backing the texture image.
		VkImageView view{VK_NULL_HANDLE};             ///< Owned RGBA image view for shader sampling.
		VkSampler sampler{VK_NULL_HANDLE};            ///< Owned linear repeat sampler for texture reads.
		VkExtent2D extent{};                          ///< Loaded texture width and height in pixels.

		TextureImage() = default;
		TextureImage(const TextureImage &) = delete;
		TextureImage &operator=(const TextureImage &) = delete;

		/**
			* @brief Loads an RGBA file from disk and uploads it into a sampled SRGB Vulkan image.
			*
			* @param physicalDevice Physical device used to select memory types.
			* @param owningDevice Logical device that owns the image, view, sampler, and upload commands.
			* @param graphicsQueue Queue used for the one-time staging copy submission.
			* @param commandPool Command pool used for the temporary upload command buffer.
			* @param imagePath Filesystem path passed to stb_image for 8-bit RGBA loading.
			* @return VK_SUCCESS when the texture is ready for descriptor binding, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			VkDevice owningDevice,
			VkQueue graphicsQueue,
			VkCommandPool commandPool,
			const std::filesystem::path &imagePath
		) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || imagePath.empty()) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			int width{};
			int height{};
			int channels{};
			const auto pathString = imagePath.string(); // stb_image requires a null-terminated filesystem path.
			auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
				stbi_load(pathString.c_str(), &width, &height, &channels, STBI_rgb_alpha),
				stbi_image_free
			};
			if (!pixels || width <= 0 || height <= 0) { return VK_ERROR_INITIALIZATION_FAILED; }

			const auto textureExtent = VkExtent2D{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)};
			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
			return create(physicalDevice, owningDevice, graphicsQueue, commandPool, std::span{reinterpret_cast<const std::byte *>(pixels.get()), static_cast<std::size_t>(byteCount)}, textureExtent);
		}

		/**
			* @brief Uploads tight RGBA bytes into a sampled SRGB Vulkan image.
			*
			* @param physicalDevice Physical device used to select memory types.
			* @param owningDevice Logical device that owns the image, view, sampler, and upload commands.
			* @param graphicsQueue Queue used for the one-time staging copy submission.
			* @param commandPool Command pool used for the temporary upload command buffer.
			* @param rgbaPixels CPU-side 8-bit RGBA pixels matching extent width times height.
			* @param textureExtent Texture width and height in pixels.
			* @return VK_SUCCESS when the texture is ready for descriptor binding, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			VkDevice owningDevice,
			VkQueue graphicsQueue,
			VkCommandPool commandPool,
			std::span<const std::byte> rgbaPixels,
			VkExtent2D textureExtent
		) {
			cleanup();
			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(textureExtent.width) * textureExtent.height * 4U;
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandPool == VK_NULL_HANDLE || textureExtent.width == 0U || textureExtent.height == 0U || rgbaPixels.size() != static_cast<std::size_t>(byteCount)) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			VulkanBuffer stagingBuffer{};
			VkResult result = stagingBuffer.create(
				physicalDevice,
				owningDevice,
				byteCount,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			);
			if (result != VK_SUCCESS) { return result; }

			result = stagingBuffer.upload(rgbaPixels.data(), byteCount);
			if (result != VK_SUCCESS) { return result; }

			device = owningDevice;
			extent = textureExtent;
			result = createImage(physicalDevice);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = uploadFrom(stagingBuffer.buffer, graphicsQueue, commandPool);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = createViewAndSampler();
			if (result != VK_SUCCESS) { cleanup(); return result; }
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned sampler, view, image, memory, and clears the borrowed device handle.
			*/
		void cleanup() {
			if (sampler != VK_NULL_HANDLE) { vkDestroySampler(device, sampler, nullptr); }
			if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); }
			if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); }
			if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); }
			sampler = VK_NULL_HANDLE;
			view = VK_NULL_HANDLE;
			image = VK_NULL_HANDLE;
			memory = VK_NULL_HANDLE;
			extent = {};
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned texture image resources on scope exit.
			*/
		~TextureImage() { cleanup(); }

	private:
		/**
			* @brief Creates and binds the device-local sampled image allocation.
			*
			* @param physicalDevice Physical device used to query image memory requirements.
			* @return VK_SUCCESS when the image and memory binding are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createImage(VkPhysicalDevice physicalDevice) {
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.extent = {.width = extent.width, .height = extent.height, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = 1U,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};

			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
			if (result != VK_SUCCESS) { return result; }

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, image, &requirements);
			const std::optional<std::uint32_t> memoryType = VulkanDepthImage::findMemoryType(
				physicalDevice,
				requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);
			if (!memoryType.has_value()) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			const VkMemoryAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = requirements.size,
				.memoryTypeIndex = *memoryType,
			};

			result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
			if (result != VK_SUCCESS) { return result; }
			return vkBindImageMemory(device, image, memory, 0U);
		}

		/**
			* @brief Uploads staging-buffer pixels with one command buffer and leaves the image shader-readable.
			*
			* @param stagingBuffer Borrowed transfer-source buffer containing RGBA pixel bytes.
			* @param graphicsQueue Queue receiving the one-time upload command buffer.
			* @param commandPool Command pool used to allocate the temporary primary command buffer.
			* @return VK_SUCCESS when the copy has completed on the queue, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult uploadFrom(VkBuffer stagingBuffer, VkQueue graphicsQueue, VkCommandPool commandPool) {
			VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
			const VkCommandBufferAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1U,
			};
			VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			VkFence fence{VK_NULL_HANDLE};
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
			if (result != VK_SUCCESS) { vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer); return result; }

			result = recordUpload(commandBuffer, stagingBuffer);
			if (result == VK_SUCCESS) {
				const VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1U,
					.pCommandBuffers = &commandBuffer,
				};
				result = vkQueueSubmit(graphicsQueue, 1U, &submitInfo, fence);
			}
			if (result == VK_SUCCESS) { result = vkWaitForFences(device, 1U, &fence, VK_TRUE, UINT64_MAX); }

			vkDestroyFence(device, fence, nullptr);
			vkFreeCommandBuffers(device, commandPool, 1U, &commandBuffer);
			return result;
		}

		/**
			* @brief Records the texture layout transitions and buffer-to-image copy.
			*
			* @param commandBuffer Temporary primary command buffer receiving upload commands.
			* @param stagingBuffer Borrowed transfer-source buffer containing tightly packed RGBA pixels.
			* @return VK_SUCCESS when recording completed, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult recordUpload(VkCommandBuffer commandBuffer, VkBuffer stagingBuffer) {
			const VkCommandBufferBeginInfo beginInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			VulkanReadback::transitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			const VkBufferImageCopy copyRegion{
				.bufferOffset = 0U,
				.bufferRowLength = 0U,
				.bufferImageHeight = 0U,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
				.imageOffset = {.x = 0, .y = 0, .z = 0},
				.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
			};
			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1U, &copyRegion);
			VulkanReadback::transitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			return vkEndCommandBuffer(commandBuffer);
		}

		/**
			* @brief Creates the sampled RGBA image view and linear repeat sampler.
			*
			* @return VK_SUCCESS when both sampling handles are ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult createViewAndSampler() {
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = 0U,
					.layerCount = 1U,
				},
			};

			VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &view);
			if (result != VK_SUCCESS) { return result; }

			const VkSamplerCreateInfo samplerInfo{
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
				.minLod = 0.0F,
				.maxLod = 1.0F,
			};

			return vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
		}
	};

	/**
		* @brief Encodes VulkanReadback color bytes as an opaque 8-bit RGBA PNG for deterministic analysis.
		*
		* @param pixels CPU-side image bytes in the exact Vulkan source format order.
		* @param extent Width and height of the readback image in pixels.
		* @param sourceFormat Vulkan source format; only 8-bit RGBA and BGRA UNORM/SRGB are supported.
		* @param outputPath Filesystem path passed to stb_image_write.
		* @return True when validation, conversion, and stbi_write_png succeeded.
		*/
	[[nodiscard]] inline bool writeReadbackPng(
		std::span<const std::byte> pixels,
		VkExtent2D extent,
		VkFormat sourceFormat,
		std::string_view outputPath
	) {
		const bool rgbaFormat = sourceFormat == VK_FORMAT_R8G8B8A8_UNORM || sourceFormat == VK_FORMAT_R8G8B8A8_SRGB;
		const bool bgraFormat = sourceFormat == VK_FORMAT_B8G8R8A8_UNORM || sourceFormat == VK_FORMAT_B8G8R8A8_SRGB;
		if ((!rgbaFormat && !bgraFormat) || extent.width == 0U || extent.height == 0U || outputPath.empty()) { return false; }

		constexpr std::size_t channels{4U}; // PNG output is always 8-bit RGBA.
		const std::size_t width = static_cast<std::size_t>(extent.width);
		const std::size_t height = static_cast<std::size_t>(extent.height);
		if (height > std::numeric_limits<std::size_t>::max() / width / channels) { return false; }
		const std::size_t byteCount = width * height * channels;
		if (pixels.size() != byteCount || width > static_cast<std::size_t>(std::numeric_limits<int>::max() / channels)) { return false; }

		auto rgbaPixels = std::vector<unsigned char>(byteCount); // Owns contiguous bytes required by stb_image_write.
		for (std::size_t offset{}; offset < byteCount; offset += channels) {
			const unsigned char first = std::to_integer<unsigned char>(pixels[offset + 0U]);
			const unsigned char second = std::to_integer<unsigned char>(pixels[offset + 1U]);
			const unsigned char third = std::to_integer<unsigned char>(pixels[offset + 2U]);
			rgbaPixels[offset + 0U] = bgraFormat ? third : first; // Convert B to R only for BGRA sources.
			rgbaPixels[offset + 1U] = second;
			rgbaPixels[offset + 2U] = bgraFormat ? first : third; // Convert R to B only for BGRA sources.
			rgbaPixels[offset + 3U] = 255U;                      // Analysis PNGs ignore source alpha.
		}

		const auto path = std::string{outputPath}; // stb requires a null-terminated path.
		const int stride = static_cast<int>(width * channels);
		return stbi_write_png(path.c_str(), static_cast<int>(extent.width), static_cast<int>(extent.height), 4, rgbaPixels.data(), stride) != 0;
	}

	/// @brief Minimal Vulkan descriptor-pool owner for uniform-buffer, shadow-map, and object-texture descriptor sets.
	struct VulkanDescriptorPool {
		VkDevice device{VK_NULL_HANDLE};                       ///< Borrowed device used to destroy the pool.
		VkDescriptorPool descriptorPool{VK_NULL_HANDLE};        ///< Owned descriptor pool.

		VulkanDescriptorPool() = default;
		VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
		VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;

		/**
			* @brief Creates a descriptor pool for per-frame uniform-buffer, shadow-map, and object-texture descriptor sets.
			*
			* @param owningDevice Logical device that owns the descriptor pool.
			* @param maxSets Maximum descriptor-set count and descriptor count per binding.
			* @return VK_SUCCESS when the descriptor pool is available, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, std::uint32_t maxSets) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const std::array poolSizes{
				VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = maxSets},
				VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxSets * 2U},
			};
			const VkDescriptorPoolCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = maxSets,
				.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
				.pPoolSizes = poolSizes.data(),
			};

			device = owningDevice;
			const VkResult result = vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool);
			if (result != VK_SUCCESS) {
				descriptorPool = VK_NULL_HANDLE;
				device = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Destroys the owned descriptor pool and clears the borrowed device handle.
			*/
		void cleanup() {
			if (descriptorPool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, descriptorPool, nullptr); }
			descriptorPool = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned descriptor pool on scope exit.
			*/
		~VulkanDescriptorPool() { cleanup(); }
	};

	/// @brief Minimal Vulkan descriptor-set owner for per-frame uniform-buffer, shadow-map, and object-texture bindings.
	struct VulkanDescriptorSets {
		VkDevice device{VK_NULL_HANDLE};                         ///< Borrowed device used for allocation and updates.
		VkDescriptorPool descriptorPool{VK_NULL_HANDLE};          ///< Borrowed pool that owns the allocations; sets are freed implicitly with the pool.
		std::vector<VkDescriptorSet> descriptorSets{};            ///< Owned descriptor sets allocated one per frame.

		VulkanDescriptorSets() = default;
		VulkanDescriptorSets(const VulkanDescriptorSets &) = delete;
		VulkanDescriptorSets &operator=(const VulkanDescriptorSets &) = delete;

		/**
			* @brief Allocates one descriptor set per frame from a borrowed pool and layout.
			*
			* @param owningDevice Logical device that owns the descriptor pool.
			* @param pool Descriptor pool used for descriptor-set allocation.
			* @param setLayout Descriptor-set layout repeated for every frame set.
			* @param count Number of per-frame descriptor sets to allocate.
			* @return VK_SUCCESS when all descriptor sets are allocated, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkDevice owningDevice, VkDescriptorPool pool, VkDescriptorSetLayout setLayout, std::uint32_t count) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || setLayout == VK_NULL_HANDLE) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			device = owningDevice;
			descriptorPool = pool;
			auto layouts = std::vector<VkDescriptorSetLayout>(count, setLayout);
			const VkDescriptorSetAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = pool,
				.descriptorSetCount = count,
				.pSetLayouts = layouts.data(),
			};

			descriptorSets.resize(count);
			const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data());
			if (result != VK_SUCCESS) {
				descriptorSets.clear();
				device = VK_NULL_HANDLE;
				descriptorPool = VK_NULL_HANDLE;
			}
			return result;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-0 uniform buffer.
			*
			* @param frameIndex Frame set index to update.
			* @param uniformBuffer Uniform buffer bound to descriptor binding 0.
			* @param range Byte range exposed through the uniform-buffer descriptor.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeUniformBuffer(std::uint32_t frameIndex, VkBuffer uniformBuffer, VkDeviceSize range) {
			if (frameIndex >= descriptorSets.size() || uniformBuffer == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffer,
				.offset = 0U,
				.range = range,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = 0U,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its binding-1 sampled shadow map.
			*
			* @param frameIndex Frame set index to update.
			* @param imageView Shadow-map image view bound to descriptor binding 1.
			* @param sampler Shadow-map sampler bound to descriptor binding 1.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
			*/
		[[nodiscard]] VkResult writeShadowMap(std::uint32_t frameIndex, VkImageView imageView, VkSampler sampler) {
			if (frameIndex >= descriptorSets.size() || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = 1U,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Writes one frame descriptor set with its reserved binding-2 sampled object texture.
			*
			* @param frameIndex Frame set index to update.
			* @param texture Object base-color texture bound to descriptor binding 2.
			* @return VK_SUCCESS after updating the descriptor set, otherwise VK_ERROR_INITIALIZATION_FAILED.
		*/
		[[nodiscard]] VkResult writeObjectTexture(std::uint32_t frameIndex, const TextureImage &texture) {
			if (frameIndex >= descriptorSets.size() || texture.view == VK_NULL_HANDLE || texture.sampler == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkDescriptorImageInfo imageInfo{
				.sampler = texture.sampler,
				.imageView = texture.view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets[frameIndex],
				.dstBinding = 2U,
				.dstArrayElement = 0U,
				.descriptorCount = 1U,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo,
			};

			vkUpdateDescriptorSets(device, 1U, &write, 0U, nullptr);
			return VK_SUCCESS;
		}

		/**
			* @brief Clears descriptor-set handles while leaving implicit pool-owned allocation lifetime intact.
			*/
		void cleanup() {
			descriptorSets.clear();
			device = VK_NULL_HANDLE;
			descriptorPool = VK_NULL_HANDLE;
		}

		/**
			* @brief Clears borrowed descriptor-set state on scope exit.
			*/
		~VulkanDescriptorSets() { cleanup(); }
	};

	/// @brief Minimal Vulkan mesh owner for one uploaded CPU mesh.
	struct VulkanMesh {
		VulkanBuffer vertexBuffer{};       ///< owned device-local-style host-visible vertex buffer.
		VulkanBuffer indexBuffer{};        ///< owned host-visible index buffer.
		std::uint32_t indexCount{0U};       ///< number of indices recorded for indexed draws.

		VulkanMesh() = default;
		VulkanMesh(const VulkanMesh &) = delete;
		VulkanMesh &operator=(const VulkanMesh &) = delete;

		/**
			* @brief Transfers uploaded mesh buffers and leaves the source empty.
			*/
		VulkanMesh(VulkanMesh &&other) noexcept
			: vertexBuffer{std::move(other.vertexBuffer)},
				indexBuffer{std::move(other.indexBuffer)},
				indexCount{std::exchange(other.indexCount, 0U)} {}

		/**
			* @brief Replaces this mesh with another uploaded mesh.
			*
			* @return This mesh after taking ownership from the source.
			*/
		VulkanMesh &operator=(VulkanMesh &&other) noexcept {
			if (this != &other) {
				cleanup();
				vertexBuffer = std::move(other.vertexBuffer);
				indexBuffer = std::move(other.indexBuffer);
				indexCount = std::exchange(other.indexCount, 0U);
			}
			return *this;
		}

		/**
			* @brief Creates vertex and index buffers and uploads one CPU mesh into host-visible memory.
			*
			* @param physicalDevice Physical device used to select host-visible memory types.
			* @param device Logical device that owns the created buffers.
			* @param mesh CPU mesh whose vertices and indices are uploaded.
			* @return VK_SUCCESS when both buffers are uploaded, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, VkDevice device, const vve::simple::Mesh &mesh) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || mesh.vertices.empty() || mesh.indices.empty()) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			const VkDeviceSize vertexSize = sizeof(vve::simple::Vertex) * mesh.vertices.size();
			const VkMemoryPropertyFlags hostVisibleMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			VkResult result = vertexBuffer.create(
				physicalDevice,
				device,
				vertexSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				hostVisibleMemory
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vertexBuffer.upload(mesh.vertices.data(), vertexSize);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			const VkDeviceSize indexSize = sizeof(std::uint32_t) * mesh.indices.size();
			result = indexBuffer.create(
				physicalDevice,
				device,
				indexSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				hostVisibleMemory
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = indexBuffer.upload(mesh.indices.data(), indexSize);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			indexCount = static_cast<std::uint32_t>(mesh.indices.size());
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned buffers and clears the draw count.
			*/
		void cleanup() {
			vertexBuffer.cleanup();
			indexBuffer.cleanup();
			indexCount = 0U;
		}

		/**
			* @brief Destroys the owned mesh buffers on scope exit.
			*/
		~VulkanMesh() { cleanup(); }
	};

	/// @brief Plain per-frame uniform data matching the Slang set 0 binding 0 block layout.
	struct FrameUniforms {
		Mat4 view{};        ///< shared camera view matrix
		Mat4 projection{};  ///< shared camera projection matrix
		Mat4 lightViewProj{}; ///< light view-projection for the upcoming shadow pass
	};

	/// @brief Minimal per-frame uniform-buffer owner for shared view and projection data.
	struct VulkanUniformBuffers {
		std::vector<VulkanBuffer> buffers{}; ///< owned per-frame host-visible uniform buffers.

		VulkanUniformBuffers() = default;
		VulkanUniformBuffers(const VulkanUniformBuffers &) = delete;
		VulkanUniformBuffers &operator=(const VulkanUniformBuffers &) = delete;

		/**
			* @brief Creates one host-visible FrameUniforms buffer for each frame slot.
			*
			* @param physicalDevice Physical device used to select host-visible memory.
			* @param device Logical device that owns the uniform buffers.
			* @param framesInFlight Number of per-frame uniform buffers to create.
			* @return VK_SUCCESS when all buffers are created, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, VkDevice device, std::uint32_t framesInFlight) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE || framesInFlight == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			auto createdBuffers = std::vector<VulkanBuffer>(framesInFlight);
			buffers.swap(createdBuffers);
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				const VkResult result = buffers[frame].create(
					physicalDevice,
					device,
					sizeof(FrameUniforms),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
				);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}
			return VK_SUCCESS;
		}

		/**
			* @brief Uploads shared frame matrices into one frame slot.
			*
			* @param frameIndex Frame slot whose uniform buffer receives the data.
			* @param uniforms Source view and projection matrices copied into the buffer.
			* @return VK_SUCCESS after upload, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult update(std::uint32_t frameIndex, const FrameUniforms &uniforms) {
			if (frameIndex >= buffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			return buffers[frameIndex].upload(&uniforms, sizeof(FrameUniforms));
		}

		/**
			* @brief Clears the owned buffer list and lets each VulkanBuffer release its handles.
			*/
		void cleanup() {
			buffers.clear();
		}

		/**
			* @brief Clears the owned uniform buffers on scope exit.
			*/
		~VulkanUniformBuffers() { cleanup(); }
	};

} // namespace vve::simple
