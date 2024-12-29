#pragma once

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE> {
		
		using System<ATYPE>::m_engine;
		using System<ATYPE>::m_registry;
		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgAnnounce;
		using Renderer<ATYPE>::m_window;

    public:
        RendererForward(std::string systemName, Engine<ATYPE>& engine, std::string windowName);
        virtual ~RendererForward();

    private:
        void OnAnnounce(Message message);
        void OnInit(Message message);
        void OnRecordNextFrame(Message message);
        void OnQuit(Message message);

		RendererVulkan<ATYPE>* m_vulkan{nullptr};
	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

