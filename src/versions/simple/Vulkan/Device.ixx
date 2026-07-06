module;
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

export module VEEngine.Simple.Vulkan:Device;
import :OwnedHandle;
import std;

/**
	* @file
	* @brief Vulkan device bootstrap objects for the simple forward renderer.
	*
	* Functional objects:
	* - findMemoryType selects a compatible physical-device memory type for Vulkan resources.
	* - VulkanInstance owns only VkInstance creation and teardown.
	* - VulkanSurface owns only VkSurfaceKHR creation and teardown.
	* - VulkanPhysicalDevice selects a physical device and queue family indices.
	* - VulkanDevice owns only VkDevice creation and queue retrieval.
	*/
namespace vve::simple {

	/**
		* @brief Finds the first physical-device memory type satisfying a type mask and required properties.
		*
		* @param physicalDevice Physical device whose memory properties are queried.
		* @param typeFilter Bit mask of memory types compatible with the resource.
		* @param requiredProperties Required Vulkan memory-property flags.
		* @return The first compatible memory type index, or std::nullopt when none match.
		*/
	[[nodiscard]] std::optional<std::uint32_t> findMemoryType(
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

} // namespace vve::simple
export namespace vve::simple {

	/// @brief Minimal Vulkan root object; no device, surface, swapchain, commands, or sync are created here.
	struct VulkanInstance {
		vk::raii::Context context{};                 ///< Vulkan-Hpp context that owns the global dispatch loader.
		VulkanOwnedHandle<vk::raii::Instance, VkInstance> instance{}; ///< Owned Vulkan instance handle.
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

			VkInstance rawInstance{};
			const VkResult result = vkCreateInstance(&createInfo, nullptr, &rawInstance);
			if (result == VK_SUCCESS) { instance.handle = vk::raii::Instance{context, rawInstance}; }
			return result;
		}

		/// @brief Releases the owned Vulkan instance through its RAII wrapper.
		void cleanup() { instance.reset(); }

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
			* @brief Selects the first physical device that supports graphics, presentation, and swapchains.
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

			for (const VkPhysicalDevice candidate : devices) {
				result = selectIfSuitable(instance, candidate, surface);
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
	};

	/// @brief Minimal Vulkan logical-device owner; no swapchain, render pass, commands, or sync are created here.
	struct VulkanDevice {
		VulkanOwnedHandle<vk::raii::Device, VkDevice> device{}; ///< Owned Vulkan logical device handle.
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
			const auto extensions = std::array<char const *, 1U>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
			const VkDeviceCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &dynamicRenderingFeatures,
				.queueCreateInfoCount = static_cast<std::uint32_t>(queueInfos.size()),
				.pQueueCreateInfos = queueInfos.data(),
				.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
				.ppEnabledExtensionNames = extensions.data(),
			};

			VkDevice rawDevice{};
			const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &rawDevice);
			if (result != VK_SUCCESS) {
				graphicsQueue = VK_NULL_HANDLE;
				presentQueue = VK_NULL_HANDLE;
				return result;
			}

			device.handle = vk::raii::Device{physicalDevice.handle, rawDevice};
			vkGetDeviceQueue(device, graphicsQueueFamily, 0U, &graphicsQueue);
			vkGetDeviceQueue(device, presentQueueFamily, 0U, &presentQueue);
			return VK_SUCCESS;
		}

		/// @brief Releases the owned logical device through its RAII wrapper and clears borrowed queues.
		void cleanup() {
			device.reset();
			graphicsQueue = VK_NULL_HANDLE;
			presentQueue = VK_NULL_HANDLE;
		}
	};

} // namespace vve::simple
