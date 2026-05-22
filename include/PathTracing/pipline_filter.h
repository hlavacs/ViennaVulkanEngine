namespace vve {

    /** Ray tracing pipeline wrapper for shader binding tables and dispatch. */
    class PipelineFilter {
    private:

        VkPipeline computePipeline{};
        VkPipelineLayout pipelineLayout{};
        VkDevice device;
        CommandManager* commandManager;
        VkPhysicalDevice physicalDevice;

        DescriptorManager* targetsDescriptors;

        VkExtent2D extent;
        VkExtent2D workgroupSize;

        std::vector<RenderTarget*> renderTargets;

        std::string shaderFile;

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
         * @param extent Render area extent.
         */
        PipelineFilter(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager* commandManager,
            DescriptorManager* targetsDescriptors, VkExtent2D extent, VkExtent2D workgroupSize, std::string shaderFile);

        /**
         * Update the render extent.
         * @param extent New render extent.
         */
        void setExtent(VkExtent2D extent);


        /**
         * Bind a render target for ray tracing output.
         * @param target Render target to add.
         */
        void bindRenderTarget(RenderTarget* target);

        /** Create the ray tracing pipeline and SBT. */
        void initComputePipeline();

        /** Release owned Vulkan resources. */
        void freeResources();

        /**
         * Record ray tracing commands for the current frame.
         * @param currentFrame Frame index.
         */
        void recordCommandBuffer(int currentFrame);

    };
}