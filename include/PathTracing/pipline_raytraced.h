#pragma once


namespace vve {
    class PiplineRaytraced {
    private:

        VkPipeline graphicsPipeline{};
        VkPipelineLayout pipelineLayout{};
        VkDevice device;
        CommandManager* commandManager;
        VkPhysicalDevice physicalDevice;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties;

        RawDeviceBuffer* shaderBindingTableBuffer;
        std::vector<uint8_t> m_shaderHandles;


        VkStridedDeviceAddressRegionKHR m_raygenRegion{};    // Ray generation shader region
        VkStridedDeviceAddressRegionKHR m_missRegion{};      // Miss shader region
        VkStridedDeviceAddressRegionKHR m_hitRegion{};       // Hit shader region
        VkStridedDeviceAddressRegionKHR m_callableRegion{};  // Callable shader region

        VkDescriptorSetLayout descriptorSetLayoutGeneral;
        std::vector<VkDescriptorSet> descriptorSetsGeneral;
        VkDescriptorSetLayout descriptorSetLayoutRT;
        std::vector<VkDescriptorSet> descriptorSetsRT;
        VkDescriptorSetLayout descriptorSetLayoutTargets;
        std::vector<VkDescriptorSet> descriptorSetsTargets;


        VkExtent2D extent;

        std::vector<RenderTarget*> renderTargets;


        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;


        static std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!: " + filename);
            }

            size_t fileSize = (size_t)file.tellg();
            std::vector<char> buffer(fileSize);

            file.seekg(0);
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }

        VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice& device);

        void createShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

        void loadRayTracingFunctions();

    public:

        PiplineRaytraced(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager* commandManager, VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties,
            VkDescriptorSetLayout descriptorSetLayoutGeneral, VkDescriptorSetLayout descriptorSetLayoutRT,
            VkDescriptorSetLayout descriptorSetLayoutTargets,
            std::vector<VkDescriptorSet> descriptorSetsTargets, VkExtent2D extent);

        void setDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsGeneral, std::vector<VkDescriptorSet> descriptorSetsRT);

        void setRenderTargetsDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsTargets);

        void setExtent(VkExtent2D extent);


        void bindRenderTarget(RenderTarget* target);

        void initRayTracingPipeline();

        void freeResources();

        void recordCommandBuffer(int currentFrame);

    };

}