#pragma once


namespace vve {
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

        VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice& device);

        // we assume that all render targets of a rasterized pipline are non persistent aka: colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // we assume that all render targets of a rasterized pipline will only be read from afterwards aka: colorAttachment.finalLayout = VK_IMAGE_LAYOUT_READ_ONLY;
        void createRenderPass();

        void createFramebuffers();

    public:

        PiplineRasterized(std::string systemName, Engine& engine, VkDevice device, VkExtent2D extent, CommandManager* commandManager, DeviceBuffer<Vertex>* vertexBuffer, DeviceBuffer<uint32_t>* indexBuffer, std::vector<HostBuffer<vvh::Instance>*> instanceBuffers, VkDescriptorSetLayout& descriptorSetLayout);

        void setDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets);


        ~PiplineRasterized();

        void freeResources();

        void bindRenderTarget(RenderTarget* target);

        void bindDepthRenderTarget(RenderTarget* target);

        void recreateFrameBuffers(VkExtent2D extent);

        void initGraphicsPipeline();

        void recordCommandBuffer(uint32_t currentFrame);
    };

}