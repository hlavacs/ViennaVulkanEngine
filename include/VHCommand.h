#pragma once

namespace vh {

    void ComCreateCommandPool(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool& commandPool);

    VkCommandBuffer ComBeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

    VkResult ComEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
    
	void ComCreateCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers);

    void ComStartRecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, bool clear, glm::vec4 clearColor, uint32_t currentFrame);

    void ComBindPipeline(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , std::vector<VkViewport> viewPorts, std::vector<VkRect2D> scissors
        , glm::vec4 blendConstants, std::vector<PushConstants> pushConstants
        , uint32_t currentFrame);

    void ComEndRecordCommandBuffer(VkCommandBuffer commandBuffer);

    void ComRecordObject(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			const std::vector<DescriptorSet>&& descriptorSets, std::string type, Mesh& mesh, uint32_t currentFrame);

	void ComSubmitCommandBuffers(VkDevice device, VkQueue graphicsQueue, std::vector<VkCommandBuffer>& commandBuffers,
		std::vector<VkSemaphore>& imageAvailableSemaphores, 
		std::vector<VkSemaphore>& renderFinishedSemaphores, 
        std::vector<Semaphores>& intermediateSemaphores, 
		std::vector<VkFence>& fences, uint32_t currentFrame);

    void ComSubmitCommandBuffers2(VkDevice device, VkQueue graphicsQueue, std::vector<VkCommandBuffer>& commandBuffers,
        std::vector<VkSemaphore>& imageAvailableSemaphores, std::vector<Semaphores>& semaphores, VkSemaphore& signalSemaphore,
        std::vector<VkFence>& fences, uint32_t currentFrame);
    
	VkResult ComPresentImage(VkQueue presentQueue, SwapChain swapChain, uint32_t& imageIndex, VkSemaphore& signalSemaphore);

} // namespace vh


