#pragma once

/**
 * @file pipline_raytraced.h
 * @brief Ray tracing pipeline setup and command recording.
 */

namespace vve {
    /** Ray tracing pipeline wrapper for shader binding tables and dispatch. */
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

        /** Create a shader module from SPIR-V bytecode. */
        VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice& device);

        void createShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo);

        void loadRayTracingFunctions();

    public:

        /**
         * @param device Logical device.
         * @param physicalDevice Physical device for RT properties.
         * @param commandManager Command manager for command buffers.
         * @param m_rtProperties Ray tracing pipeline properties.
         * @param descriptorSetLayoutGeneral Layout for general descriptors.
         * @param descriptorSetLayoutRT Layout for RT descriptors.
         * @param descriptorSetLayoutTargets Layout for target descriptors.
         * @param descriptorSetsTargets Descriptor sets for targets.
         * @param extent Render area extent.
         */
        PiplineRaytraced(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager* commandManager, VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties,
            VkDescriptorSetLayout descriptorSetLayoutGeneral, VkDescriptorSetLayout descriptorSetLayoutRT,
            VkDescriptorSetLayout descriptorSetLayoutTargets,
            std::vector<VkDescriptorSet> descriptorSetsTargets, VkExtent2D extent);

        /** Assign descriptor sets used by the pipeline. */
        void setDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsGeneral, std::vector<VkDescriptorSet> descriptorSetsRT);

        /** Assign descriptor sets for render targets. */
        void setRenderTargetsDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsTargets);

        /** Update the render extent. */
        void setExtent(VkExtent2D extent);


        /** Bind a render target for ray tracing output. */
        void bindRenderTarget(RenderTarget* target);

        /** Create the ray tracing pipeline and SBT. */
        void initRayTracingPipeline();

        /** Release owned Vulkan resources. */
        void freeResources();

        /** Record ray tracing commands for the current frame. */
        void recordCommandBuffer(int currentFrame);

    };

}
