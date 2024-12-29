#pragma once

namespace vve
{
   	template<ArchitectureType ATYPE>
    class RendererForward : public Renderer<ATYPE> {
		
		using System<ATYPE>::m_engine;
		using System<ATYPE>::m_registry;
		using typename System<ATYPE>::Message;
		using Renderer<ATYPE>::m_window;

    public:
        RendererForward(std::string systemName, Engine<ATYPE>& engine);
        virtual ~RendererForward();

    private:
        void OnInit(Message message);
        void OnInit2(Message message);
        void OnRecordNextFrame(Message message);
        void OnQuit(Message message);

		RendererVulkan<ATYPE>* m_vulkan{nullptr};
	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

