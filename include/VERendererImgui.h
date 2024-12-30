#pragma once


namespace vve {

    class RendererImgui : public Renderer
    {
    public:
        RendererImgui(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererImgui();

    private:
        void OnAnnounce(Message message);
        void OnInit(Message message);
        void OnPrepareNextFrame(Message message);
        void OnRecordNextFrame(Message message);
        void OnSDL(Message message);
        void OnQuit(Message message);

		RendererVulkan* m_vulkan{nullptr};
	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
