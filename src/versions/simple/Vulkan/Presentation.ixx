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

export module VEEngine.Simple.Vulkan:Presentation;
import :Device;
import :OwnedHandle;
import std;

/**
	* @file
	* @brief Vulkan presentation objects for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanSwapchain owns only VkSwapchainKHR creation and swapchain image retrieval.
	* - VulkanImageViews owns only VkImageView creation and teardown for borrowed swapchain images.
	* - VulkanDepthImage owns one depth VkImage, its device memory, and its VkImageView.
	*/
export namespace vve::simple {
	/// @brief Minimal Vulkan swapchain owner; no image views, render pass, commands, or sync are created here.
	struct VulkanSwapchain {
		/// @brief Presentation mode request used during swapchain creation.
		enum class PresentModePreference {
			mailbox, ///< Prefer low-latency mailbox presentation with FIFO fallback.
			fifo,    ///< Request guaranteed FIFO presentation.
		};

		VulkanOwnedHandle<vk::raii::SwapchainKHR, VkSwapchainKHR> swapchain{}; ///< Owned Vulkan swapchain handle.
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
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
			VkSurfaceKHR surface,
			std::uint32_t graphicsQueueFamily,
			std::uint32_t presentQueueFamily,
			std::uint32_t width,
			std::uint32_t height,
			PresentModePreference presentModePreference = PresentModePreference::mailbox
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
			const VkPresentModeKHR presentMode = choosePresentMode(presentModes, presentModePreference);
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
			VkSwapchainKHR rawSwapchain{VK_NULL_HANDLE};
			result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &rawSwapchain);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			swapchain.handle = vk::raii::SwapchainKHR{owningDevice.handle, rawSwapchain};

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
			swapchain.reset();
			imageFormat = VK_FORMAT_UNDEFINED;
			extent = {};
			images.clear();
			device = VK_NULL_HANDLE;
		}

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
			* @brief Selects the requested presentation behavior with FIFO as the guaranteed fallback.
			*
			* @param presentModes Surface present modes reported by Vulkan.
			* @param preference User-requested present mode preference.
			* @return Requested or preferred present mode, with guaranteed FIFO fallback.
			*/
		[[nodiscard]] static VkPresentModeKHR choosePresentMode(
			const std::vector<VkPresentModeKHR> &presentModes,
			PresentModePreference preference
		) {
			if (preference == PresentModePreference::fifo) { return VK_PRESENT_MODE_FIFO_KHR; }
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
		std::vector<VulkanOwnedHandle<vk::raii::ImageView, VkImageView>> ownedViews{}; ///< Owned image views created for borrowed swapchain images.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, const std::vector<VkImage> &images, VkFormat imageFormat) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			ownedViews.reserve(images.size());
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
				const VkResult result = vkCreateImageView(owningDevice, &createInfo, nullptr, &view);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				ownedViews.emplace_back().handle = vk::raii::ImageView{owningDevice.handle, view};
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned image views and clears the borrowed device handle.
			*/
		void cleanup() {
			for (auto &view : ownedViews) { view.reset(); }
			ownedViews.clear();
		}

		/**
			* @brief Destroys the owned image views on scope exit.
			*/
		~VulkanImageViews() { cleanup(); }
	};

	/// @brief Minimal Vulkan depth-attachment owner; no render pass, framebuffers, commands, or sync are created here.
	struct VulkanDepthImage {
		VulkanOwnedHandle<vk::raii::Image, VkImage> image{};                         ///< Owned depth image handle.
		VulkanOwnedHandle<vk::raii::DeviceMemory, VkDeviceMemory> memory{};          ///< Owned device-local memory backing the depth image.
		VulkanOwnedHandle<vk::raii::ImageView, VkImageView> imageView{};             ///< Owned depth image view used as a framebuffer attachment.

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
		[[nodiscard]] VkResult create(VkPhysicalDevice physicalDevice, const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, VkExtent2D extent) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || extent.width == 0U || extent.height == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

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

			VkImage rawImage{VK_NULL_HANDLE};
			VkResult result = vkCreateImage(owningDevice, &imageInfo, nullptr, &rawImage);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			image.handle = vk::raii::Image{owningDevice.handle, rawImage};

			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(owningDevice, image, &requirements);
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

			VkDeviceMemory rawMemory{VK_NULL_HANDLE};
			result = vkAllocateMemory(owningDevice, &allocateInfo, nullptr, &rawMemory);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			memory.handle = vk::raii::DeviceMemory{owningDevice.handle, rawMemory};

			result = vkBindImageMemory(owningDevice, image, memory, 0U);
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

			VkImageView rawView{VK_NULL_HANDLE};
			result = vkCreateImageView(owningDevice, &viewInfo, nullptr, &rawView);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			imageView.handle = vk::raii::ImageView{owningDevice.handle, rawView};
			return VK_SUCCESS;
		}

		/**
			* @brief Destroys the owned depth image view, image, and memory allocation.
			*/
		void cleanup() {
			imageView.reset();
			image.reset();
			memory.reset();
		}

		/**
			* @brief Destroys the owned depth resources on scope exit.
		*/
		~VulkanDepthImage() { cleanup(); }
	};

} // namespace vve::simple
