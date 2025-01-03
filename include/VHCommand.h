#pragma once

namespace vh {

    void createCommandPool(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool& commandPool);

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

    void endSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
    
	void createCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers);

    void startRecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , bool clear, glm::vec4 clearColor, uint32_t currentFrame);

    void endRecordCommandBuffer(VkCommandBuffer commandBuffer);

    void recordObject(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			std::vector<VkDescriptorSet>& descriptorSets, Geometry& geometry, uint32_t currentFrame);

    void recordObject2(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			const std::vector<DescriptorSet>&& descriptorSets, Geometry& geometry, uint32_t currentFrame);


} // namespace vh


