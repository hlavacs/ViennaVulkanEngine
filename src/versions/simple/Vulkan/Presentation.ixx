module;
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Presentation;
import :Device;
import std;

/**
	* @file
	* @brief Vulkan presentation objects for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanSwapchain owns only VkSwapchainKHR creation and swapchain image retrieval.
	* - VulkanImageViews owns only VkImageView creation and teardown for borrowed swapchain images.
	* - VulkanDepthImage owns one depth VkImage, its device memory, and its VkImageView.
	* - VulkanRenderPass owns only VkRenderPass creation and teardown for a single forward color subpass.
	* - VulkanFramebuffers owns only VkFramebuffer creation and teardown for borrowed swapchain image views.
	*/
export namespace vve::simple {
	/// @brief Minimal Vulkan swapchain owner; no image views, render pass, commands, or sync are created here.
	struct VulkanSwapchain {
		/// @brief Presentation mode request used during swapchain creation.
		enum class PresentModePreference {
			mailbox, ///< Prefer low-latency mailbox presentation with FIFO fallback.
			fifo,    ///< Request guaranteed FIFO presentation.
		};

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

} // namespace vve::simple
