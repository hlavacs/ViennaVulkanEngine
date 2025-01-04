#pragma once


namespace vh {

   	void createRenderPassClear(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass);

    void createRenderPass(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass);

    void createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>&& bindings, VkDescriptorSetLayout& descriptorSetLayouts);

	void createDescriptorPool(VkDevice device, uint32_t sizes, VkDescriptorPool& descriptorPool);

    void createDescriptorSet(VkDevice device, VkDescriptorSetLayout& descriptorSetLayouts, VkDescriptorPool descriptorPool, DescriptorSet& descriptorSet);

    void updateDescriptorSetUBO(VkDevice device, UniformBuffers& uniformBuffers, size_t binding, size_t size, DescriptorSet& descriptorSet);
    void updateDescriptorSetTexture(VkDevice device, Texture& texture, size_t binding, DescriptorSet& descriptorSet);

    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);

    void createGraphicsPipeline(VkDevice device, VkRenderPass renderPass, std::vector<VkDescriptorSetLayout> descriptorSetLayout, Pipeline& graphicsPipeline);

    void createFramebuffers(VkDevice device, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);

    void createDepthResources(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);

    VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

    bool hasStencilComponent(VkFormat format);


} // namespace vh

