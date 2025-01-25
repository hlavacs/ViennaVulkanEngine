#pragma once

namespace vh {

    void createCommandPool(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool& commandPool);

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

    void endSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
    
	void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers);

    void startRecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , bool clear, glm::vec4 clearColor, uint32_t currentFrame);

    void bindPipeline(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , bool clear, glm::vec4 clearColor, uint32_t currentFrame);

    void endRecordCommandBuffer(VkCommandBuffer commandBuffer);

    void recordObject(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			const std::vector<DescriptorSet>&& descriptorSets, Mesh& geometry, uint32_t currentFrame);

    void recordObject2(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			const std::vector<DescriptorSet>&& descriptorSets, Mesh& mesh, uint32_t currentFrame);


	void submitCommandBuffers(VkDevice device, VkQueue graphicsQueue, std::vector<VkCommandBuffer>& commandBuffers,
		std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores, VkSemaphore& signalSemaphore,
		std::vector<VkFence>& fences, uint32_t currentFrame);

	VkResult presentImage(VkQueue presentQueue, SwapChain swapChain, uint32_t imageIndex, VkSemaphore signalSemaphore);

} // namespace vh


