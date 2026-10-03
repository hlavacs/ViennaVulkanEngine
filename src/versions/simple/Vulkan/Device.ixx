module;
#include <compare>
#include <cstdio>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VVEngine.Simple.Vulkan:Device;
import :OwnedHandle;
import std;

/**
	* @file
	* @brief Vulkan device bootstrap objects for the simple forward renderer.
	*
	* Functional objects:
	* - detail::appendLayerFilter preserves existing comma-separated loader filters without duplicate entries.
	* - DeviceCapabilities and evaluateDeviceRequirements check the Vulkan 1.3 device contract without a GPU.
	* - VulkanInstance owns VkInstance and Debug validation diagnostics, including synchronization checks.
	* - VulkanSurface owns only VkSurfaceKHR creation and teardown.
	* - VulkanPhysicalDevice selects a physical device and queue family indices.
	* - VulkanDevice owns only VkDevice creation and queue retrieval.
	*/
export namespace vve::simple {

	/// @brief Queried device capabilities needed by the forward renderer, independent of Vulkan object lifetime.
	struct DeviceCapabilities {
		std::uint32_t apiVersion{};					///< Physical device's supported Vulkan API version.
		bool swapchainExtension{};					///< VK_KHR_swapchain is advertised by the device.
		bool dynamicRendering{};						///< Dynamic rendering feature is supported.
		bool shaderSampledImageArrayDynamicIndexing{};	///< Material texture arrays support dynamic indexing.
	};

	[[nodiscard]] inline VkResult evaluateDeviceRequirements(const DeviceCapabilities &capabilities);

	/// @brief Minimal Vulkan root object; no device, surface, swapchain, commands, or sync are created here.
	struct VulkanInstance {
		vk::raii::Context context{};                 ///< Vulkan-Hpp context that owns the global dispatch loader.
		VulkanOwnedHandle<vk::raii::Instance, VkInstance> instance{}; ///< Owned Vulkan instance handle.
		std::vector<char const *> extensions{};      ///< SDL-required instance extensions used for creation.
		std::vector<char const *> layers{};          ///< Optional validation layers enabled when available.

		VulkanInstance() = default;
		~VulkanInstance();														///< Releases the validation messenger before the owned instance.
		VulkanInstance(const VulkanInstance &) = delete;
		VulkanInstance &operator=(const VulkanInstance &) = delete;
		[[nodiscard]] bool validationActive() const;							///< Reports whether the Debug validation messenger is active.
		[[nodiscard]] std::uint64_t validationErrorCount() const;				///< Returns ERROR callbacks since create(), retained through cleanup.

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
			const void *instanceNext{};
#ifndef NDEBUG
			validationErrors_.store(0U, std::memory_order_relaxed);
			const bool validationEnabled = validationLayerAvailable();
			if (validationEnabled) { layers.push_back(validationLayerName); }
			const bool debugUtils = validationEnabled && extensionAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			const VkDebugUtilsMessengerCreateInfoEXT debugInfo{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = validationCallback,
				.pUserData = this,
			};
			// The instance callback covers creation and destruction; the persistent messenger covers its lifetime.
			if (debugUtils) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				instanceNext = &debugInfo;
			}
#ifdef VK_EXT_layer_settings
			const VkBool32 validateSync{VK_TRUE};
			const VkLayerSettingEXT syncSetting{validationLayerName, "validate_sync", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1U, &validateSync};
			const VkLayerSettingsCreateInfoEXT settingsInfo{
				.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT, .pNext = instanceNext,
				.settingCount = 1U, .pSettings = &syncSetting};
#endif
			const VkValidationFeatureEnableEXT syncFeature{VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
			const VkValidationFeaturesEXT featuresInfo{
				.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, .pNext = instanceNext,
				.enabledValidationFeatureCount = 1U, .pEnabledValidationFeatures = &syncFeature};
			// Older headers or layers use validation_features instead of layer_settings.
			if (validationEnabled) {
#ifdef VK_EXT_layer_settings
				if (extensionAvailable(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
					extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
					instanceNext = &settingsInfo;
				} else
#endif
				if (extensionAvailable(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME)) {
					extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
					instanceNext = &featuresInfo;
				} else {
					std::fprintf(stderr, "[vve::simple] Sync validation unavailable: VK_EXT_layer_settings and VK_EXT_validation_features missing\n");
				}
			}
#endif

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
				.pNext = instanceNext,
				.pApplicationInfo = &appInfo,
				.enabledLayerCount = static_cast<std::uint32_t>(layers.size()),
				.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data(),
				.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data(),
			};

			VkInstance rawInstance{};
			const VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
			if (result != VK_SUCCESS) { return result; }
			instance.handle = vk::raii::Instance{context, rawInstance};
#ifndef NDEBUG
			if (debugUtils) {
				const auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
					vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
				if (createMessenger == nullptr || vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT") == nullptr) {
					cleanup();
					return VK_ERROR_EXTENSION_NOT_PRESENT;
				}
				const VkResult messengerResult = createMessenger(instance, &debugInfo, nullptr, &debugMessenger_);
				if (messengerResult != VK_SUCCESS) { cleanup(); return messengerResult; }
			}
#endif
			return VK_SUCCESS;
		}

		void cleanup();															///< Releases the validation messenger and instance, preserving error counts.

	private:
#ifndef NDEBUG
		VkDebugUtilsMessengerEXT debugMessenger_{VK_NULL_HANDLE}; ///< Owned messenger, destroyed before the instance.
		std::atomic<std::uint64_t> validationErrors_{};            ///< ERROR callbacks since create(), retained through cleanup.
		static constexpr char const *validationLayerName{"VK_LAYER_KHRONOS_validation"}; ///< Standard Vulkan validation layer.
		[[nodiscard]] static bool extensionAvailable(std::string_view name);	///< Checks loader and validation-layer extension availability.
		static VKAPI_ATTR VkBool32 VKAPI_CALL validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT *message, void *userData);	///< Prints validation diagnostics and counts ERROR callbacks.

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
#endif
	};

	/// @brief Minimal Vulkan window surface object; no device, swapchain, commands, or sync are created here.
	struct VulkanSurface {
		VulkanOwnedHandle<vk::raii::SurfaceKHR, VkSurfaceKHR> surface{}; ///< Owned Vulkan surface handle.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Instance, VkInstance> &owningInstance, SDL_Window *window) {
			cleanup();
			if (owningInstance == VK_NULL_HANDLE || window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }

			VkSurfaceKHR rawSurface{};
			if (!SDL_Vulkan_CreateSurface(window, owningInstance, nullptr, &rawSurface)) { return VK_ERROR_INITIALIZATION_FAILED; }
			surface.handle = vk::raii::SurfaceKHR{owningInstance.handle, rawSurface};
			return VK_SUCCESS;
		}

		/// @brief Releases the owned Vulkan surface through its RAII wrapper.
		void cleanup() { surface.reset(); }
	};

	/// @brief Non-owning Vulkan physical-device selector for graphics and presentation support.
	struct VulkanPhysicalDevice {
		VulkanOwnedHandle<vk::raii::PhysicalDevice, VkPhysicalDevice> physicalDevice{}; ///< Selected physical device handle.
		std::optional<std::uint32_t> graphicsQueueFamily{};          ///< Queue family index supporting graphics commands.
		std::optional<std::uint32_t> presentQueueFamily{};           ///< Queue family index supporting presentation to the surface.

		/**
			* @brief Selects the first Vulkan 1.3 device supporting graphics with compute, presentation, swapchains, and required features.
			*
			* @param instance Vulkan instance that owns the physical-device list.
			* @param surface Vulkan surface used to test presentation support.
			* @return VK_SUCCESS when a device was selected, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult select(const VulkanOwnedHandle<vk::raii::Instance, VkInstance> &instance, VkSurfaceKHR surface) {
			reset();
			if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			std::uint32_t deviceCount{};
			VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			if (deviceCount == 0U) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			auto devices = std::vector<VkPhysicalDevice>(deviceCount);
			result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
			if (result != VK_SUCCESS) { return result; }

			// Keep the first suitable device and report rejected devices only when none qualifies.
			for (const VkPhysicalDevice candidate : devices) {
				result = selectIfSuitable(instance, candidate, surface);
				if (result == VK_SUCCESS && selected()) { return VK_SUCCESS; }
				if (result != VK_SUCCESS && result != VK_ERROR_FEATURE_NOT_PRESENT) { reset(); return result; }
			}

			std::string rejectedDevices{};
			// Include each device's actual version so older drivers have an actionable diagnostic.
			for (const VkPhysicalDevice candidate : devices) {
				VkPhysicalDeviceProperties properties{};
				vkGetPhysicalDeviceProperties(candidate, &properties);
				rejectedDevices += std::format("{}{} (Vulkan {}.{}.{})", rejectedDevices.empty() ? "" : ", ",
					properties.deviceName, VK_API_VERSION_MAJOR(properties.apiVersion),
					VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
			}
			std::fprintf(stderr, "[vve::simple] No suitable device: %s; requires Vulkan 1.3, VK_KHR_swapchain, "
				"dynamicRendering, shaderSampledImageArrayDynamicIndexing and graphics/compute/present queues\n", rejectedDevices.c_str());
			reset();
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		/**
			* @brief Reports whether a device and both required queue family indices are available.
			*
			* @return True after successful selection.
			*/
		[[nodiscard]] bool selected() const {
			return physicalDevice.valid() && graphicsQueueFamily.has_value() && presentQueueFamily.has_value();
		}

	private:
		static constexpr char const *swapchainExtensionName{VK_KHR_SWAPCHAIN_EXTENSION_NAME}; ///< Required presentation extension.

		/**
			* @brief Clears the borrowed physical-device handle and discovered queue family indices.
			*/
		void reset() {
			physicalDevice.reset();
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
		[[nodiscard]] VkResult selectIfSuitable(const VulkanOwnedHandle<vk::raii::Instance, VkInstance> &instance, VkPhysicalDevice candidate, VkSurfaceKHR surface) {
			std::optional<std::uint32_t> graphicsFamily{};
			std::optional<std::uint32_t> presentationFamily{};

			std::uint32_t queueFamilyCount{};
			vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
			auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
			if (queueFamilyCount != 0U) {
				vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());
			}

			// The graphics queue also runs the post-processing compute shaders, so it needs both capabilities.
			// A family that can also present is preferred, which avoids concurrent swapchain sharing.
			constexpr VkQueueFlags graphicsAndCompute{VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT};
			for (std::uint32_t index{}; index < queueFamilyCount; ++index) {
				VkBool32 presentSupported{VK_FALSE};
				const VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &presentSupported);
				if (result != VK_SUCCESS) { return result; }
				const bool graphics = (queueFamilies[index].queueFlags & graphicsAndCompute) == graphicsAndCompute;

				if (graphics && presentSupported == VK_TRUE) {
					graphicsFamily = index;
					presentationFamily = index;
					break;
				}
				if (graphics && !graphicsFamily.has_value()) { graphicsFamily = index; }
				if (presentSupported == VK_TRUE && !presentationFamily.has_value()) { presentationFamily = index; }
			}

			const VkResult swapchainResult = supportsSwapchain(candidate);
			if (swapchainResult != VK_SUCCESS && swapchainResult != VK_ERROR_FEATURE_NOT_PRESENT) { return swapchainResult; }
			const VkResult result = supportsRequiredFeatures(candidate, swapchainResult == VK_SUCCESS);
			if (result != VK_SUCCESS) { return result; }
			if (!graphicsFamily.has_value() || !presentationFamily.has_value()) { return VK_ERROR_FEATURE_NOT_PRESENT; }

			physicalDevice.handle = vk::raii::PhysicalDevice{instance.handle, candidate};
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

		/**
			* @brief Queries the Vulkan version and features and evaluates the renderer's device requirements.
			*
			* The renderer records dynamic rendering passes, and the fragment shader indexes the texture array
			* with a per-draw material index, which needs shaderSampledImageArrayDynamicIndexing.
			* @param candidate Physical device whose features are queried.
			* @param swapchainSupported Whether the device advertises VK_KHR_swapchain.
			* @return VK_SUCCESS when all requirements are met, otherwise VK_ERROR_FEATURE_NOT_PRESENT.
			*/
		[[nodiscard]] static VkResult supportsRequiredFeatures(VkPhysicalDevice candidate, bool swapchainSupported) {
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(candidate, &properties);
			VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			};
			VkPhysicalDeviceFeatures2 features{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &dynamicRendering,
			};
			vkGetPhysicalDeviceFeatures2(candidate, &features);
			return evaluateDeviceRequirements(DeviceCapabilities{
				.apiVersion = properties.apiVersion,
				.swapchainExtension = swapchainSupported,
				.dynamicRendering = dynamicRendering.dynamicRendering == VK_TRUE,
				.shaderSampledImageArrayDynamicIndexing = features.features.shaderSampledImageArrayDynamicIndexing == VK_TRUE,
			});
		}
	};

	/// @brief Minimal Vulkan logical-device owner; no swapchain, commands, or sync are created here.
	struct VulkanDevice {
		VulkanOwnedHandle<vk::raii::Device, VkDevice> device{}; ///< Owned Vulkan logical device handle.
		VkQueue graphicsQueue{VK_NULL_HANDLE};        ///< Borrowed graphics queue retrieved from the device.
		VkQueue presentQueue{VK_NULL_HANDLE};         ///< Borrowed presentation queue retrieved from the device.
		bool swapchainMutableFormat{false};           ///< Whether the optional GUI UNORM swapchain views are enabled.
		bool samplerAnisotropy{false};                ///< Whether the optional material sampler feature is enabled.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::PhysicalDevice, VkPhysicalDevice> &physicalDevice, std::uint32_t graphicsQueueFamily, std::uint32_t presentQueueFamily) {
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

			const VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
				.dynamicRendering = VK_TRUE,
			};
			VkPhysicalDeviceFeatures supportedFeatures{};
			vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
			const VkPhysicalDeviceFeatures enabledFeatures{
				.samplerAnisotropy = supportedFeatures.samplerAnisotropy, ///< Enable anisotropic material sampling only when supported.
				.shaderSampledImageArrayDynamicIndexing = VK_TRUE, ///< Fragment shader indexes the texture array by material; checked in device selection.
			};
			// Image-format lists are core in Vulkan 1.2; only the swapchain extension remains optional.
			std::uint32_t extensionCount{};
			VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
			if (result != VK_SUCCESS) { return result; }
			auto availableExtensions = std::vector<VkExtensionProperties>(extensionCount);
			result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
			if (result != VK_SUCCESS) { return result; }
			const bool mutableFormat = std::ranges::any_of(availableExtensions, [](const VkExtensionProperties &extension) {
				return std::string_view{extension.extensionName} == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME;
			});
			auto extensions = std::vector<char const *>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
			if (mutableFormat) { extensions.push_back(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME); }
			const VkDeviceCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &dynamicRenderingFeatures,
				.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
				.pQueueCreateInfos = queueInfos.data(),
				.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.data(),
				.pEnabledFeatures = &enabledFeatures,
			};

			VkDevice rawDevice{};
			result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &rawDevice);
			if (result != VK_SUCCESS) {
				graphicsQueue = VK_NULL_HANDLE;
				presentQueue = VK_NULL_HANDLE;
				return result;
			}

			device.handle = vk::raii::Device{physicalDevice.handle, rawDevice};
			swapchainMutableFormat = mutableFormat;
			samplerAnisotropy = enabledFeatures.samplerAnisotropy == VK_TRUE;
			vkGetDeviceQueue(device, graphicsQueueFamily, 0U, &graphicsQueue);
			vkGetDeviceQueue(device, presentQueueFamily, 0U, &presentQueue);
			return VK_SUCCESS;
		}

		/// @brief Releases the owned logical device through its RAII wrapper and clears borrowed queues.
		void cleanup() {
			device.reset();
			swapchainMutableFormat = false;
			samplerAnisotropy = false;
			graphicsQueue = VK_NULL_HANDLE;
			presentQueue = VK_NULL_HANDLE;
		}
	};

	namespace detail {
		/// @brief Appends one exact layer name to a comma-separated loader filter; existing entries are preserved.
		[[nodiscard]] inline std::string appendLayerFilter(std::string current, std::string_view layer) {
			// Match complete entries so a longer layer name does not hide the requested filter.
			for (const auto entry : std::views::split(current, ',')) {
				if (std::ranges::equal(entry, layer)) { return current; }
			}
			if (!current.empty()) { current += ','; }
			current += layer;
			return current;
		}
	}

	/// @brief Checks the renderer's core API and feature contract without calling Vulkan; missing requirements reject the device.
	[[nodiscard]] inline VkResult evaluateDeviceRequirements(const DeviceCapabilities &capabilities) {
		return capabilities.apiVersion >= VK_API_VERSION_1_3 && capabilities.swapchainExtension &&
			capabilities.dynamicRendering && capabilities.shaderSampledImageArrayDynamicIndexing
			? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
	}

	/// @brief Tears down the messenger while its callback state is still alive.
	inline VulkanInstance::~VulkanInstance() { cleanup(); }

	/// @brief Releases the Debug messenger before its instance; error counts remain available for inspection.
	inline void VulkanInstance::cleanup() {
#ifndef NDEBUG
		if (debugMessenger_ != VK_NULL_HANDLE) {
			const auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
			destroyMessenger(instance, debugMessenger_, nullptr);
			debugMessenger_ = VK_NULL_HANDLE;
		}
#endif
		instance.reset();
	}

	/// @brief Reports whether Debug validation errors can currently reach the counter.
	inline bool VulkanInstance::validationActive() const {
#ifndef NDEBUG
		return debugMessenger_ != VK_NULL_HANDLE;
#else
		return false;
#endif
	}

	/// @brief Returns all ERROR callbacks since the latest create(), or zero in Release.
	inline std::uint64_t VulkanInstance::validationErrorCount() const {
#ifndef NDEBUG
		return validationErrors_.load(std::memory_order_relaxed);
#else
		return 0U;
#endif
	}

#ifndef NDEBUG
	/// @brief Checks loader and validation-layer extensions, including layer-only configuration extensions.
	inline bool VulkanInstance::extensionAvailable(std::string_view name) {
		// Explicit layer extensions are not included in the loader's global list.
		for (const char *layer : {static_cast<const char *>(nullptr), validationLayerName}) {
			std::uint32_t count{};
			if (vkEnumerateInstanceExtensionProperties(layer, &count, nullptr) != VK_SUCCESS) { continue; }
			auto available = std::vector<VkExtensionProperties>(count);
			if (vkEnumerateInstanceExtensionProperties(layer, &count, available.data()) != VK_SUCCESS) { continue; }
			if (std::ranges::any_of(available, [name](const auto &extension) { return name == extension.extensionName; })) { return true; }
		}
		return false;
	}

	/// @brief Prints validation diagnostics and counts ERROR severity, including synchronization hazards.
	inline VKAPI_ATTR VkBool32 VKAPI_CALL VulkanInstance::validationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *message, void *userData) {
		if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
			static_cast<VulkanInstance *>(userData)->validationErrors_.fetch_add(1U, std::memory_order_relaxed);
		}
		std::fprintf(stderr, "[vve::simple validation] %s: %s\n",
			message->pMessageIdName != nullptr ? message->pMessageIdName : "unnamed",
			message->pMessage != nullptr ? message->pMessage : "");
		return VK_FALSE;
	}
#endif

} // namespace vve::simple
