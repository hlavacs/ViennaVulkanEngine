#pragma once


namespace vh {

   	void RenCreateRenderPassClear(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass);

    void RenCreateRenderPass(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass);

    void RenCreateDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout& descriptorSetLayouts);

	  void RenCreateDescriptorPool(VkDevice device, uint32_t sizes, VkDescriptorPool& descriptorPool);

    void RenCreateDescriptorSet(VkDevice device, VkDescriptorSetLayout& descriptorSetLayouts, VkDescriptorPool descriptorPool, DescriptorSet& descriptorSet);

    void RenUpdateDescriptorSet(VkDevice device, Buffer& uniformBuffers, size_t binding, VkDescriptorType type, size_t size, DescriptorSet& descriptorSet);

    void RenUpdateDescriptorSetTexture(VkDevice device, Map& texture, size_t binding, DescriptorSet& descriptorSet);

    VkShaderModule RenCreateShaderModule(VkDevice device, const std::vector<char>& code);

    void RenCreateGraphicsPipeline(VkDevice device, VkRenderPass renderPass, 
  		std::string vertShaderPath, std::string fragShaderPath,
  		std::vector<VkVertexInputBindingDescription> bindingDescription, 
  		std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
  		std::vector<VkDescriptorSetLayout> descriptorSetLayout,
  		std::vector<int32_t> specializationConstants,
      std::vector<VkPushConstantRange> pushConstantRanges,
      std::vector<VkPipelineColorBlendAttachmentState> blendAttachments,
  		Pipeline& graphicsPipeline);

    void RenCreateFramebuffers(VkDevice device, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);

    void RenCreateDepthResources(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);

    VkFormat RenFindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkFormat RenFindDepthFormat(VkPhysicalDevice physicalDevice);

    bool RenHasStencilComponent(VkFormat format);


} // namespace vh

