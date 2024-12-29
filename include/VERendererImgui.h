#pragma once


namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgAnnounce;
		using typename System<ATYPE>::MsgSDL;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(std::string systemName, Engine<ATYPE>& engine);
        virtual ~RendererImgui();

    private:
        void OnAnnounce(Message message);
        void OnInit(Message message);
        void OnPrepareNextFrame(Message message);
        void OnRecordNextFrame(Message message);
        void OnSDL(Message message);
        void OnQuit(Message message);

		RendererVulkan<ATYPE>* m_vulkan{nullptr};
	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
