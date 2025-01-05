#pragma once

namespace vve
{
    class RendererForward : public Renderer {

	    struct UniformBufferObject {
	        alignas(16) glm::mat4 model;
	    };
	
	    struct UniformBufferFrame {
	        alignas(16) glm::mat4 view;
	        alignas(16) glm::mat4 proj;
	    };

    public:
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward();
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }

    private:
        void OnInit(Message message);
        void OnRecordNextFrame(Message message);
		void OnObjectCreate( Message message );
        void OnQuit(Message message);

		vh::UniformBuffers m_uniformBuffersPerFrame;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{1};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

