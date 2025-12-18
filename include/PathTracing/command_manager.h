#pragma once

namespace vve {
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};



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
	
		void createCommandPool();

		void createCommandBuffer();

		void createSyncObjects();

		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR& surface);

	public:
		CommandManager(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue& graphicsQueue);

		~CommandManager();

		VkCommandBuffer getCommandBuffer(int currentFrame);
		VkCommandBuffer* getCommandBufferPtr(int currentFrame);

		VkCommandBuffer beginSingleTimeCommand();

		void endSingleTimeCommand(VkCommandBuffer& commandBuffer);

		void beginCommand(int currentFrame);
		void executeCommand(int currentFrame, VkSemaphore ImageAvailableSemaphore);
		VkSemaphore getRenderFinishedSemaphores(int currentFrame);
		void waitForFence(int currentFrame);

	};

}