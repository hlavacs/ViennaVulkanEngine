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
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan:Commands;
import :Device;
import :OwnedHandle;
import std;

/**
	* @file
	* @brief Vulkan command and buffer ownership objects for the simple forward renderer.
	*
	* Functional objects:
	* - VulkanCommandPool owns only command-pool creation for the graphics queue family and RAII teardown.
	* - VulkanCommandBuffers owns primary command-buffer allocation through RAII and exposes raw handles for recording.
	* - VulkanFrameSync owns per-frame semaphores and fences for the simple forward renderer.
	*/
export namespace vve::simple {
	/// @brief Minimal Vulkan command-pool owner for resettable graphics command buffers.
	struct VulkanCommandPool {
		VulkanOwnedHandle<vk::raii::CommandPool, VkCommandPool> commandPool{}; ///< Owned command pool for the graphics queue family.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::uint32_t graphicsQueueFamily) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandPoolCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = graphicsQueueFamily,
			};

			VkCommandPool rawCommandPool{};
			const VkResult result = vkCreateCommandPool(owningDevice, &createInfo, nullptr, &rawCommandPool);
			if (result == VK_SUCCESS) { commandPool.handle = vk::raii::CommandPool{owningDevice.handle, rawCommandPool}; }
			return result;
		}

		/**
			* @brief Releases the owned command pool through its RAII wrapper.
			*/
		void cleanup() { commandPool.reset(); }
	};

	/// @brief Minimal Vulkan command-buffer owner for primary command buffers allocated from the command pool.
	struct VulkanCommandBuffers {
		std::vector<VulkanOwnedHandle<vk::raii::CommandBuffer, VkCommandBuffer>> ownedCommandBuffers{}; ///< RAII owners for primary command buffers.
		std::vector<VkCommandBuffer> commandBuffers{}; ///< Raw command-buffer view kept for existing recording code.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, const VulkanOwnedHandle<vk::raii::CommandPool, VkCommandPool> &owningPool, std::uint32_t count) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || owningPool == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			commandBuffers.resize(count);
			const VkCommandBufferAllocateInfo allocateInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = owningPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = count,
			};

			const VkResult result = vkAllocateCommandBuffers(owningDevice, &allocateInfo, commandBuffers.data());
			if (result != VK_SUCCESS) {
				commandBuffers.clear();
				return result;
			}

			ownedCommandBuffers.reserve(commandBuffers.size());
			for (VkCommandBuffer commandBuffer : commandBuffers) {
				ownedCommandBuffers.emplace_back().handle = vk::raii::CommandBuffer{owningDevice.handle, commandBuffer, owningPool};
			}
			return result;
		}

		/**
			* @brief Releases the owned command buffers through their RAII wrappers.
			*/
		void cleanup() {
			for (auto &commandBuffer : ownedCommandBuffers) { commandBuffer.reset(); }
			ownedCommandBuffers.clear();
			commandBuffers.clear();
		}

		/**
			* @brief Frees the owned command buffers on scope exit.
			*/
		~VulkanCommandBuffers() { cleanup(); }
	};

	/// @brief Minimal Vulkan frame-synchronization owner for per-frame rendering primitives.
	struct VulkanFrameSync {
		std::vector<VulkanOwnedHandle<vk::raii::Semaphore, VkSemaphore>> imageAvailableSemaphores{}; ///< Owned semaphores signaled when swapchain images are ready.
		std::vector<VulkanOwnedHandle<vk::raii::Semaphore, VkSemaphore>> renderFinishedSemaphores{}; ///< Owned semaphores signaled when rendering has finished.
		std::vector<VulkanOwnedHandle<vk::raii::Fence, VkFence>> inFlightFences{};                   ///< Owned fences tracking submitted work for each frame slot.

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
		[[nodiscard]] VkResult create(const VulkanOwnedHandle<vk::raii::Device, VkDevice> &owningDevice, std::uint32_t framesInFlight, std::uint32_t swapchainImageCount) {
			cleanup();
			if (owningDevice == VK_NULL_HANDLE || framesInFlight == 0U || swapchainImageCount == 0U) { return VK_ERROR_INITIALIZATION_FAILED; }

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
				VkResult result = vkCreateSemaphore(owningDevice, &semaphoreInfo, nullptr, &imageAvailable);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				imageAvailableSemaphores.emplace_back().handle = vk::raii::Semaphore{owningDevice.handle, imageAvailable};

				VkFence inFlight{VK_NULL_HANDLE};
				result = vkCreateFence(owningDevice, &fenceInfo, nullptr, &inFlight);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				inFlightFences.emplace_back().handle = vk::raii::Fence{owningDevice.handle, inFlight};
			}

			// Present waits can outlive a frame slot, so each swapchain image owns its signal semaphore.
			for (std::uint32_t image{}; image < swapchainImageCount; ++image) {
				const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
				VkSemaphore renderFinished{VK_NULL_HANDLE};
				const VkResult result = vkCreateSemaphore(owningDevice, &semaphoreInfo, nullptr, &renderFinished);
				if (result != VK_SUCCESS) { cleanup(); return result; }
				renderFinishedSemaphores.emplace_back().handle = vk::raii::Semaphore{owningDevice.handle, renderFinished};
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Releases the owned frame synchronization primitives through their RAII wrappers.
			*/
		void cleanup() {
			for (auto &semaphore : imageAvailableSemaphores) { semaphore.reset(); }
			for (auto &semaphore : renderFinishedSemaphores) { semaphore.reset(); }
			for (auto &fence : inFlightFences) { fence.reset(); }
			imageAvailableSemaphores.clear();
			renderFinishedSemaphores.clear();
			inFlightFences.clear();
		}

		/**
			* @brief Destroys the owned frame synchronization primitives on scope exit.
			*/
		~VulkanFrameSync() { cleanup(); }
	};

} // namespace vve::simple
