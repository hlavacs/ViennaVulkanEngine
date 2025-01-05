#pragma once


namespace vve {

    class RendererImgui : public Renderer  {
		
    public:
        RendererImgui(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererImgui();
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnSDL(Message message);
        bool OnQuit(Message message);

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
