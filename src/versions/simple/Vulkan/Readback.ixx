module;
#include <compare>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_vulkan.h>
#include <stb_image_write.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Readback;
import :OwnedHandle;
import :Device;
import :Commands;
import std;

/**
	* @file
	* @brief Vulkan readback utilities for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanReadback copies one finished color VkImage into a host-visible VulkanBuffer and exposes CPU pixel bytes.
	* - VulkanDepthReadback copies one D32 shadow-array layer into a host-visible VulkanBuffer and exposes CPU depth values.
	* - writeReadbackPng encodes VulkanReadback bytes as deterministic opaque RGBA8 PNG files.
	*/
export namespace vve::simple {

	/// @brief Minimal Vulkan color-image readback owner for one host-visible transfer destination buffer.
	struct VulkanReadback {
		const VulkanOwnedHandle<vk::raii::Device, VkDevice> *ownedDevice{}; ///< Borrowed RAII device for scoped transfer helpers.
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
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
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
			ownedDevice = &owningDevice;
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
			vk::raii::CommandBuffer ownedCommandBuffer{(*ownedDevice).handle, commandBuffer, commandPool};
			commandBuffer = static_cast<VkCommandBuffer>(*ownedCommandBuffer);

			VkFence fence{VK_NULL_HANDLE};
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
			if (result != VK_SUCCESS) { return result; }
			vk::raii::Fence ownedFence{(*ownedDevice).handle, fence};
			fence = static_cast<VkFence>(*ownedFence);

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
			ownedDevice = nullptr;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned readback buffer on scope exit.
		*/
		~VulkanReadback() { cleanup(); }

	private:
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

	public:
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

	private:
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

	/// @brief Minimal Vulkan D32 depth-array readback owner for one captured shadow-map layer.
	struct VulkanDepthReadback {
		const VulkanOwnedHandle<vk::raii::Device, VkDevice> *ownedDevice{}; ///< Borrowed RAII device for scoped transfer helpers.
		VkDevice device{VK_NULL_HANDLE};              ///< Borrowed Vulkan logical device used for commands, fences, and mapping.
		VkQueue graphicsQueue{VK_NULL_HANDLE};        ///< Borrowed graphics queue used to submit the one-time copy command buffer.
		VkCommandPool commandPool{VK_NULL_HANDLE};    ///< Borrowed command pool used to allocate the temporary command buffer.
		VkExtent2D extent{};                          ///< Captured depth layer size in texels.
		std::uint32_t capturedLayer{};                ///< Last array layer copied into the readback buffer.
		VulkanBuffer buffer{};                        ///< Owned host-visible transfer destination storing tight float depths.
		bool valid{false};                            ///< True after a successful layer capture has populated the buffer.

		VulkanDepthReadback() = default;
		VulkanDepthReadback(const VulkanDepthReadback &) = delete;
		VulkanDepthReadback &operator=(const VulkanDepthReadback &) = delete;

		/**
			* @brief Creates a host-visible transfer destination buffer sized for one D32 depth layer.
			*
			* @param physicalDevice Physical device used to query memory types.
			* @param owningDevice Logical device that owns the readback buffer and temporary synchronization objects.
			* @param queue Graphics queue used later for the one-time copy submission.
			* @param pool Command pool used later for one temporary primary command buffer.
			* @param layerExtent Width and height of the source D32 shadow-map layer.
			* @return VK_SUCCESS when the readback buffer is ready, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult create(
			VkPhysicalDevice physicalDevice,
			const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice,
			VkQueue queue,
			VkCommandPool pool,
			VkExtent2D layerExtent
		) {
			cleanup();
			if (physicalDevice == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || layerExtent.width == 0U || layerExtent.height == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			const VkDeviceSize byteCount = static_cast<VkDeviceSize>(layerExtent.width) * layerExtent.height * sizeof(float);
			VkResult result = buffer.create(
				physicalDevice,
				owningDevice,
				byteCount,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			device = owningDevice;
			ownedDevice = &owningDevice;
			graphicsQueue = queue;
			commandPool = pool;
			extent = layerExtent;
			return VK_SUCCESS;
		}

		/**
			* @brief Copies one shader-readable D32 image-array layer into the host-visible depth buffer.
			*
			* @param sourceImage Borrowed VK_FORMAT_D32_SFLOAT image array to copy from.
			* @param layer Array layer selected with baseArrayLayer for the depth copy.
			* @return VK_SUCCESS when the requested layer is available through depthAt, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult capture(VkImage sourceImage, std::uint32_t layer) {
			valid = false;
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
			vk::raii::CommandBuffer ownedCommandBuffer{(*ownedDevice).handle, commandBuffer, commandPool};
			commandBuffer = static_cast<VkCommandBuffer>(*ownedCommandBuffer);

			VkFence fence{VK_NULL_HANDLE};
			const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
			if (result != VK_SUCCESS) { return result; }
			vk::raii::Fence ownedFence{(*ownedDevice).handle, fence};
			fence = static_cast<VkFence>(*ownedFence);

			result = recordCopy(commandBuffer, sourceImage, layer);
			if (result == VK_SUCCESS) {
				const VkSubmitInfo submitInfo{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 1U,
					.pCommandBuffers = &commandBuffer,
				};
				result = vkQueueSubmit(graphicsQueue, 1U, &submitInfo, fence);
			}
			if (result == VK_SUCCESS) { result = vkWaitForFences(device, 1U, &fence, VK_TRUE, UINT64_MAX); }

			if (result == VK_SUCCESS) { capturedLayer = layer; valid = true; }
			return result;
		}

		/**
			* @brief Reports whether a previous capture populated the readback buffer.
			*
			* @return True when depthAt may read texels from the last captured layer.
			*/
		[[nodiscard]] bool hasData() const { return valid; }

		/**
			* @brief Reads one float depth texel from the last captured D32 layer.
			*
			* @param x Horizontal texel coordinate inside the captured layer.
			* @param y Vertical texel coordinate inside the captured layer.
			* @return The captured depth value, or std::nullopt when no valid texel is available.
			*/
		[[nodiscard]] std::optional<float> depthAt(std::uint32_t x, std::uint32_t y) const {
			if (!valid || x >= extent.width || y >= extent.height || buffer.memory == VK_NULL_HANDLE) { return std::nullopt; }

			void *mapped{};
			const VkDeviceSize byteOffset = (static_cast<VkDeviceSize>(y) * extent.width + x) * sizeof(float);
			const VkResult result = vkMapMemory(device, buffer.memory, 0U, buffer.size, 0U, &mapped);
			if (result != VK_SUCCESS) { return std::nullopt; }

			float depth{};
			std::memcpy(&depth, static_cast<const std::byte *>(mapped) + byteOffset, sizeof(float));
			vkUnmapMemory(device, buffer.memory);
			return depth;
		}

		/**
			* @brief Releases the owned readback buffer and clears borrowed handles.
			*/
		void cleanup() {
			buffer.cleanup();
			extent = {};
			capturedLayer = 0U;
			valid = false;
			graphicsQueue = VK_NULL_HANDLE;
			commandPool = VK_NULL_HANDLE;
			ownedDevice = nullptr;
			device = VK_NULL_HANDLE;
		}

		/**
			* @brief Destroys the owned readback buffer on scope exit.
			*/
		~VulkanDepthReadback() { cleanup(); }

	private:
		/**
			* @brief Records the depth-image layout transitions and single-layer image-to-buffer copy.
			*
			* @param commandBuffer Temporary primary command buffer to record.
			* @param sourceImage Borrowed D32 image array copied into the readback buffer.
			* @param layer Array layer copied with VK_IMAGE_ASPECT_DEPTH_BIT.
			* @return VK_SUCCESS when recording completed, otherwise a Vulkan error code.
			*/
		[[nodiscard]] VkResult recordCopy(VkCommandBuffer commandBuffer, VkImage sourceImage, std::uint32_t layer) {
			const VkCommandBufferBeginInfo beginInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			transitionDepthLayer(commandBuffer, sourceImage, layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			const VkBufferImageCopy copyRegion{
				.bufferOffset = 0U,
				.bufferRowLength = 0U,
				.bufferImageHeight = 0U,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.mipLevel = 0U,
					.baseArrayLayer = layer,
					.layerCount = 1U,
				},
				.imageOffset = {.x = 0, .y = 0, .z = 0},
				.imageExtent = {.width = extent.width, .height = extent.height, .depth = 1U},
			};
			vkCmdCopyImageToBuffer(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.buffer, 1U, &copyRegion);
			transitionDepthLayer(commandBuffer, sourceImage, layer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			return vkEndCommandBuffer(commandBuffer);
		}

		/**
			* @brief Records a depth-image layout transition for one mip level and one array layer.
			*
			* @param commandBuffer Command buffer receiving the barrier.
			* @param image Depth image being transitioned.
			* @param layer Array layer affected by the barrier.
			* @param oldLayout Layout before the barrier.
			* @param newLayout Layout after the barrier.
			*/
		static void transitionDepthLayer(
			VkCommandBuffer commandBuffer,
			VkImage image,
			std::uint32_t layer,
			VkImageLayout oldLayout,
			VkImageLayout newLayout
		) {
			const VkImageMemoryBarrier barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = accessMask(oldLayout),
				.dstAccessMask = accessMask(newLayout),
				.oldLayout = oldLayout,
				.newLayout = newLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0U,
					.levelCount = 1U,
					.baseArrayLayer = layer,
					.layerCount = 1U,
				},
			};
			vkCmdPipelineBarrier(commandBuffer, stageMask(oldLayout), stageMask(newLayout), 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);
		}

		/**
			* @brief Selects the pipeline stage used for supported depth readback layouts.
			*
			* @param layout Vulkan image layout being synchronized.
			* @return Pipeline stage compatible with shader reads and transfer-source copies.
			*/
		[[nodiscard]] static VkPipelineStageFlags stageMask(VkImageLayout layout) {
			if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { return VK_PIPELINE_STAGE_TRANSFER_BIT; }
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}

		/**
			* @brief Selects the memory access mask used for supported depth readback layouts.
			*
			* @param layout Vulkan image layout being synchronized.
			* @return Access mask compatible with shader reads and transfer-source copies.
			*/
		[[nodiscard]] static VkAccessFlags accessMask(VkImageLayout layout) {
			if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { return VK_ACCESS_SHADER_READ_BIT; }
			if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { return VK_ACCESS_TRANSFER_READ_BIT; }
			return 0U;
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

} // namespace vve::simple
