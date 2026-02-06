#pragma once

/**
 * @file command_manager.h
 * @brief Command buffer, pool, and synchronization utilities.
 */

namespace vve {
	/** Queue family indices required by the renderer. */
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		/** @return True when all required queue families are available. */
		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};



	/** Manages command buffers, command pools, and frame sync objects. */
	class CommandManager {

	private:
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkSurfaceKHR surface;
		VkQueue graphicsQueue;
		VkCommandPool commandPool{};
		std::vector<VkCommandBuffer> commandBuffers{};
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
	
		/** Create a command pool for the graphics queue family. */
		void createCommandPool();

		/** Allocate per-frame command buffers. */
		void createCommandBuffer();

		/** Create per-frame semaphores and fences. */
		void createSyncObjects();

		/**
		 * Query queue families for graphics/present support.
		 * @param device Physical device to query.
		 * @param surface Surface used for present support checks.
		 * @return Populated queue family indices.
		 */
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR& surface);

	public:
		/**
		 * @param device Logical device.
		 * @param physicalDevice Physical device for queue queries.
		 * @param surface Surface used for present queue selection.
		 * @param graphicsQueue Graphics queue handle.
		 */
		CommandManager(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue& graphicsQueue);

		/** Release owned Vulkan resources. */
		~CommandManager();

		/** Explicitly free Vulkan resources (destructor-safe). */
		void freeResources();

		/**
		 * @param currentFrame Frame index.
		 * @return Command buffer for the given frame index.
		 */
		VkCommandBuffer getCommandBuffer(int currentFrame);
		/**
		 * @param currentFrame Frame index.
		 * @return Pointer to command buffer for the given frame index.
		 */
		VkCommandBuffer* getCommandBufferPtr(int currentFrame);

		/**
		 * Begin a single-time command buffer.
		 * @return Recording command buffer.
		 */
		VkCommandBuffer beginSingleTimeCommand();

		/**
		 * End and submit a single-time command buffer.
		 * @param commandBuffer Command buffer to end and submit.
		 */
		void endSingleTimeCommand(VkCommandBuffer& commandBuffer);

		/**
		 * Begin recording the frame command buffer.
		 * @param currentFrame Frame index.
		 */
		void beginCommand(int currentFrame);
		/**
		 * Submit the frame command buffer.
		 * @param currentFrame Frame index.
		 * @param ImageAvailableSemaphore Semaphore to wait on before rendering.
		 */
		void executeCommand(int currentFrame, VkSemaphore ImageAvailableSemaphore);
		/**
		 * @param currentFrame Frame index.
		 * @return Render-finished semaphore for the frame.
		 */
		VkSemaphore getRenderFinishedSemaphores(int currentFrame);
		/**
		 * Wait for the in-flight fence of the frame.
		 * @param currentFrame Frame index.
		 */
		void waitForFence(int currentFrame);

	};

}
