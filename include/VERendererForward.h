#pragma once

namespace vve
{
    class RendererForward : public Renderer {
		
    public:
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward();
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }

    private:
        void OnInit(Message message);
        void OnRecordNextFrame(Message message);
		void OnObjectCreate( Message message );
        void OnQuit(Message message);

		VkDescriptorSetLayout m_descriptorSetLayoutsPerFrameInFlight;
		vh::DescriptorSet m_descriptorSetsPerFrameInFlight;
		VkDescriptorSetLayout m_descriptorSetLayoutsPerObject;
		vh::DescriptorSet m_descriptorSetsPerObject;

		VkDescriptorSetLayout m_descriptorSetLayoutBuffer;
		VkDescriptorSetLayout m_descriptorSetLayoutBufferTexture;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

