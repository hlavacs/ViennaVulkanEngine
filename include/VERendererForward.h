#pragma once

namespace vve
{
    class RendererForward : public Renderer {
		
    public:
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward();

    private:
        void OnAnnounce(Message message);
        void OnInit(Message message);
        void OnRecordNextFrame(Message message);
		void OnObjectCreate( Message message );
        void OnQuit(Message message);

		vh::DescriptorSetLayout m_descriptorSetLayoutsPerFrameInFlight;
		vh::DescriptorSet m_descriptorSetsPerFrameInFlight;
		vh::DescriptorSetLayout m_descriptorSetLayoutsPerObject;
		vh::DescriptorSet m_descriptorSetsPerObject;

		vh::DescriptorSetLayout m_descriptorSetLayouts;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

