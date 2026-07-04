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

export module VEEngine.Simple.Vulkan:Commands;
import :Device;
import std;

/**
	* @file
	* @brief Vulkan command and buffer ownership objects for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanCommandPool owns only VkCommandPool creation for the graphics queue family and teardown.
	* - VulkanCommandBuffers owns primary command-buffer allocation from a borrowed command pool and teardown.
	* - VulkanFrameSync owns per-frame semaphores and fences for the simple forward renderer.
	* - VulkanBuffer owns one VkBuffer and its backing VkDeviceMemory allocation.
	*/
export namespace vve::simple {
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
	};

} // namespace vve::simple
