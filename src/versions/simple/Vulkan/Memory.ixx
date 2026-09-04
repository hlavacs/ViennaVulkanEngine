module;
#include <compare>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

export module VEEngine.Simple.Vulkan:Memory;
import std;

/**
	* @file
	* @brief VMA-backed memory ownership and one-time command helpers shared by every Vulkan partition.
	*
	* Functional objects:
	* - VulkanAllocator owns the VmaAllocator created for one logical device.
	* - VulkanBuffer owns one VkBuffer plus its VMA allocation; host-visible buffers stay persistently mapped.
	* - VulkanImage owns one VkImage plus its VMA allocation, a whole-image view, and optional per-layer views.
	* - submitOnce records, submits, and waits for one temporary command buffer.
	* - transitionImage records a conservative layout barrier for a range of array layers.
	*/
export namespace vve::simple {

	/// @brief Owner of the VMA allocator used for every buffer and image of one device.
	struct VulkanAllocator {
		VmaAllocator allocator{VK_NULL_HANDLE}; ///< Owned VMA allocator.

		VulkanAllocator() = default;
		VulkanAllocator(const VulkanAllocator &) = delete;
		VulkanAllocator &operator=(const VulkanAllocator &) = delete;
		~VulkanAllocator() { cleanup(); }

		[[nodiscard]] operator VmaAllocator() const { return allocator; }

		/// @brief Creates the allocator; apiVersion must not exceed what instance and device support.
		[[nodiscard]] VkResult create(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, std::uint32_t apiVersion) {
			cleanup();
			if (instance == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VmaAllocatorCreateInfo createInfo{
				.physicalDevice = physicalDevice,
				.device = device,
				.instance = instance,
				.vulkanApiVersion = apiVersion,
			};
			return vmaCreateAllocator(&createInfo, &allocator);
		}

		void cleanup() {
			if (allocator != VK_NULL_HANDLE) { vmaDestroyAllocator(allocator); }
			allocator = VK_NULL_HANDLE;
		}
	};

	/// @brief Owned buffer with its allocation; host-visible buffers are persistently mapped for upload and readback.
	struct VulkanBuffer {
		VmaAllocator allocator{VK_NULL_HANDLE}; ///< Borrowed allocator that owns the allocation.
		VkBuffer buffer{VK_NULL_HANDLE};        ///< Owned buffer handle.
		VmaAllocation allocation{VK_NULL_HANDLE}; ///< Owned VMA allocation.
		void *mapped{nullptr};                  ///< Persistent host mapping, or null for device-local buffers.
		VkDeviceSize size{0};                   ///< Requested buffer size in bytes.

		VulkanBuffer() = default;
		VulkanBuffer(const VulkanBuffer &) = delete;
		VulkanBuffer &operator=(const VulkanBuffer &) = delete;
		VulkanBuffer(VulkanBuffer &&other) noexcept { *this = std::move(other); }
		VulkanBuffer &operator=(VulkanBuffer &&other) noexcept {
			if (this != &other) {
				cleanup();
				allocator = std::exchange(other.allocator, VK_NULL_HANDLE);
				buffer = std::exchange(other.buffer, VK_NULL_HANDLE);
				allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
				mapped = std::exchange(other.mapped, nullptr);
				size = std::exchange(other.size, 0U);
			}
			return *this;
		}
		~VulkanBuffer() { cleanup(); }

		/**
			* @brief Creates a buffer; hostVisible selects mapped host memory (uploads, readbacks) instead of device-local memory.
			*/
		[[nodiscard]] VkResult create(VmaAllocator owningAllocator, VkDeviceSize bufferSize, VkBufferUsageFlags usage, bool hostVisible) {
			cleanup();
			if (owningAllocator == VK_NULL_HANDLE || bufferSize == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }
			const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bufferSize, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
			const VmaAllocationCreateInfo allocationInfo{
				.flags = hostVisible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT : 0U,
				.usage = VMA_MEMORY_USAGE_AUTO,
				.requiredFlags = hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0U,
			};
			VmaAllocationInfo info{};
			const VkResult result = vmaCreateBuffer(owningAllocator, &bufferInfo, &allocationInfo, &buffer, &allocation, &info);
			if (result != VK_SUCCESS) { buffer = VK_NULL_HANDLE; allocation = VK_NULL_HANDLE; return result; }
			allocator = owningAllocator;
			mapped = info.pMappedData;
			size = bufferSize;
			return VK_SUCCESS;
		}

		/// @brief Copies bytes into the persistent mapping of a host-visible buffer.
		[[nodiscard]] VkResult upload(const void *data, VkDeviceSize byteCount) {
			if (mapped == nullptr || data == nullptr || byteCount > size) { return VK_ERROR_INITIALIZATION_FAILED; }
			std::memcpy(mapped, data, static_cast<std::size_t>(byteCount));
			return VK_SUCCESS;
		}

		/// @brief Returns the persistent mapping of a host-visible buffer as bytes (empty for device-local buffers).
		[[nodiscard]] std::span<const std::byte> bytes() const {
			return mapped == nullptr ? std::span<const std::byte>{} : std::span{static_cast<const std::byte *>(mapped), static_cast<std::size_t>(size)};
		}

		void cleanup() {
			if (buffer != VK_NULL_HANDLE) { vmaDestroyBuffer(allocator, buffer, allocation); }
			allocator = VK_NULL_HANDLE;
			buffer = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
			mapped = nullptr;
			size = 0U;
		}
	};

	/// @brief Owned 2D image or 2D array with its allocation, a whole-image view, and optional per-layer views.
	struct VulkanImage {
		VmaAllocator allocator{VK_NULL_HANDLE};  ///< Borrowed allocator that owns the allocation.
		VkDevice device{VK_NULL_HANDLE};         ///< Borrowed device used for the views.
		VkImage image{VK_NULL_HANDLE};           ///< Owned image handle.
		VmaAllocation allocation{VK_NULL_HANDLE}; ///< Owned VMA allocation.
		VkImageView imageView{VK_NULL_HANDLE};   ///< Owned view over all layers (2D or 2D array).
		std::vector<VkImageView> layerViews{};   ///< Owned one-layer 2D views, present when requested at creation.
		VkExtent2D extent{};                     ///< Image size in pixels.
		VkFormat format{VK_FORMAT_UNDEFINED};    ///< Image format shared by every view.
		VkImageAspectFlags aspect{};             ///< Aspect used by the views and layout transitions.
		std::uint32_t layerCount{0U};            ///< Number of array layers.

		VulkanImage() = default;
		VulkanImage(const VulkanImage &) = delete;
		VulkanImage &operator=(const VulkanImage &) = delete;
		~VulkanImage() { cleanup(); }

		/**
			* @brief Creates a device-local optimal-tiling image with one view over all layers and, optionally, one view per layer.
			*/
		[[nodiscard]] VkResult create(VmaAllocator owningAllocator, VkDevice owningDevice, VkExtent2D imageExtent, VkFormat imageFormat, VkImageUsageFlags usage,
											VkImageAspectFlags imageAspect, std::uint32_t layers = 1U, bool perLayerViews = false) {
			cleanup();
			if (owningAllocator == VK_NULL_HANDLE || owningDevice == VK_NULL_HANDLE || imageExtent.width == 0U || imageExtent.height == 0U || layers == 0U) {
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = imageFormat,
				.extent = {.width = imageExtent.width, .height = imageExtent.height, .depth = 1U},
				.mipLevels = 1U,
				.arrayLayers = layers,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			const VmaAllocationCreateInfo allocationInfo{.usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
			VkResult result = vmaCreateImage(owningAllocator, &imageInfo, &allocationInfo, &image, &allocation, nullptr);
			if (result != VK_SUCCESS) { image = VK_NULL_HANDLE; allocation = VK_NULL_HANDLE; return result; }
			allocator = owningAllocator;
			device = owningDevice;
			extent = imageExtent;
			format = imageFormat;
			aspect = imageAspect;
			layerCount = layers;

			result = createView(0U, layers, imageView);
			if (result != VK_SUCCESS) { cleanup(); return result; }
			if (perLayerViews) {
				layerViews.assign(layers, VK_NULL_HANDLE);
				for (std::uint32_t layer{}; layer < layers; ++layer) {
					result = createView(layer, 1U, layerViews[layer]);
					if (result != VK_SUCCESS) { cleanup(); return result; }
				}
			}
			return VK_SUCCESS;
		}

		void cleanup() {
			for (VkImageView view : layerViews) { if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); } }
			layerViews.clear();
			if (imageView != VK_NULL_HANDLE) { vkDestroyImageView(device, imageView, nullptr); }
			if (image != VK_NULL_HANDLE) { vmaDestroyImage(allocator, image, allocation); }
			imageView = VK_NULL_HANDLE;
			image = VK_NULL_HANDLE;
			allocation = VK_NULL_HANDLE;
			allocator = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
			extent = {};
			format = VK_FORMAT_UNDEFINED;
			aspect = 0U;
			layerCount = 0U;
		}

	private:
		/// @brief Creates one view over the given layer range; several layers produce a 2D-array view.
		[[nodiscard]] VkResult createView(std::uint32_t baseLayer, std::uint32_t layers, VkImageView &view) const {
			const VkImageViewCreateInfo viewInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = layers == 1U ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.format = format,
				.subresourceRange = {.aspectMask = aspect, .baseMipLevel = 0U, .levelCount = 1U, .baseArrayLayer = baseLayer, .layerCount = layers},
			};
			return vkCreateImageView(device, &viewInfo, nullptr, &view);
		}
	};

	/// @brief Returns the access mask implied by an image layout for the conservative barriers used outside frame recording.
	[[nodiscard]] inline VkAccessFlags layoutAccess(VkImageLayout layout) {
		switch (layout) {
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		default: return 0U;
		}
	}

	/// @brief Records a layout transition for a range of layers using all-commands stages; meant for one-time uploads and readbacks.
	inline void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspect, std::uint32_t baseLayer, std::uint32_t layers,
										  VkImageLayout oldLayout, VkImageLayout newLayout) {
		if (oldLayout == newLayout) { return; }
		const VkImageMemoryBarrier barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = layoutAccess(oldLayout),
			.dstAccessMask = layoutAccess(newLayout),
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {.aspectMask = aspect, .baseMipLevel = 0U, .levelCount = 1U, .baseArrayLayer = baseLayer, .layerCount = layers},
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &barrier);
	}

	/// @brief Allocates a temporary command buffer, records it through `record`, submits it, and waits for completion.
	template <typename Record>
	[[nodiscard]] VkResult submitOnce(VkDevice device, VkQueue queue, VkCommandPool pool, Record &&record) {
		if (device == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
		VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
		const VkCommandBufferAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1U};
		VkResult result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
		if (result != VK_SUCCESS) { return result; }

		VkFence fence{VK_NULL_HANDLE};
		const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		if (result == VK_SUCCESS) {
			const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
			result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		}
		if (result == VK_SUCCESS) {
			record(commandBuffer);
			result = vkEndCommandBuffer(commandBuffer);
		}
		if (result == VK_SUCCESS) {
			const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1U, .pCommandBuffers = &commandBuffer};
			result = vkQueueSubmit(queue, 1U, &submitInfo, fence);
		}
		if (result == VK_SUCCESS) { result = vkWaitForFences(device, 1U, &fence, VK_TRUE, UINT64_MAX); }

		if (fence != VK_NULL_HANDLE) { vkDestroyFence(device, fence, nullptr); }
		vkFreeCommandBuffers(device, pool, 1U, &commandBuffer);
		return result;
	}

} // namespace vve::simple
