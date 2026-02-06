#pragma once

/**
 * @file pipline_rasterized.h
 * @brief Rasterization pipeline setup and command recording.
 */

namespace vve {
    /** Rasterization pipeline system for drawing geometry to render targets. */
    class PiplineRasterized: public System {

    private:
        VkPipeline graphicsPipeline{};
        VkPipelineLayout pipelineLayout{};
        VkRenderPass renderPass{};
        VkDescriptorSetLayout descriptorSetLayout;
        std::vector<VkDescriptorSet> descriptorSets;
        std::vector<VkFramebuffer> framebuffers{};

        std::vector<RenderTarget*> renderTargets;
        RenderTarget* depthTarget;

        VkDevice device;
        VkExtent2D extent;
        CommandManager* commandManager;

        DeviceBuffer<Vertex>* vertexBuffer;
        DeviceBuffer<uint32_t>* indexBuffer;
        std::vector<HostBuffer<vvh::Instance>*> instanceBuffers;


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

        // we assume that all render targets of a rasterized pipline are non persistent aka: colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // we assume that all render targets of a rasterized pipline will only be read from afterwards aka: colorAttachment.finalLayout = VK_IMAGE_LAYOUT_READ_ONLY;
        void createRenderPass();

        void createFramebuffers();

    public:

        /**
         * @param systemName System identifier.
         * @param engine Engine reference for messaging.
         * @param device Logical device.
         * @param extent Render area extent.
         * @param commandManager Command manager for command buffers.
         * @param vertexBuffer Geometry vertex buffer.
         * @param indexBuffer Geometry index buffer.
         * @param instanceBuffers Per-frame instance buffers.
         * @param descriptorSetLayout Descriptor set layout for uniforms/textures.
         */
        PiplineRasterized(std::string systemName, Engine& engine, VkDevice device, VkExtent2D extent, CommandManager* commandManager, DeviceBuffer<Vertex>* vertexBuffer, DeviceBuffer<uint32_t>* indexBuffer, std::vector<HostBuffer<vvh::Instance>*> instanceBuffers, VkDescriptorSetLayout& descriptorSetLayout);

        /**
         * Assign descriptor sets used by the pipeline.
         * @param descriptorSets Descriptor sets to bind.
         */
        void setDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets);


        /** Release owned Vulkan resources. */
        ~PiplineRasterized();

        /** Explicitly free Vulkan resources (destructor-safe). */
        void freeResources();

        /**
         * Bind a color render target.
         * @param target Render target to add.
         */
        void bindRenderTarget(RenderTarget* target);

        /**
         * Bind a depth render target.
         * @param target Depth render target.
         */
        void bindDepthRenderTarget(RenderTarget* target);

        /**
         * Recreate framebuffers for a new extent.
         * @param extent New render extent.
         */
        void recreateFrameBuffers(VkExtent2D extent);

        /** Create the graphics pipeline. */
        void initGraphicsPipeline();

        /**
         * Record draw commands for the current frame.
         * @param currentFrame Frame index.
         */
        void recordCommandBuffer(uint32_t currentFrame);
    };

}
