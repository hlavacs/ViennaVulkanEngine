#pragma once


namespace vve {

    /**
     * @brief ImGui renderer implementation
     */
    class RendererImgui : public Renderer  {

    public:
        /**
         * @brief Constructor for ImGui Renderer
         * @param systemName Name of the system
         * @param engine Reference to the engine
         * @param windowName Name of the associated window
         */
        RendererImgui(std::string systemName, Engine& engine, std::string windowName);
        /**
         * @brief Destructor for ImGui Renderer
         */
        virtual ~RendererImgui();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnSDL(Message message);
        bool OnQuit(Message message);

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
	    vvh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkDescriptorPool m_descriptorPool;    
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
