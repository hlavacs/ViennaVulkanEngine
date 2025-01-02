#pragma once


namespace vve {

    class RendererImgui : public Renderer
    {
    public:
        RendererImgui(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererImgui();
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }

    private:
        void OnInit(Message message);
        void OnPrepareNextFrame(Message message);
        void OnRecordNextFrame(Message message);
        void OnSDL(Message message);
        void OnQuit(Message message);

		VkDescriptorSetLayout m_descriptorSetLayouts;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
