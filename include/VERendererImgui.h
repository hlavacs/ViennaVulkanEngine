#pragma once


namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererImgui : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:
        RendererImgui(std::string systemName, Engine<ATYPE>& engine);
        virtual ~RendererImgui();

    private:
        virtual void OnInit(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnSDL(Message message);
        virtual void OnQuit(Message message);

		RendererVulkan<ATYPE>* m_vulkan{nullptr};
	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve
