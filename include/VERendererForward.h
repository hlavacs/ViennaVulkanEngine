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
        bool OnInit(Message message);
        bool OnRecordNextFrame(Message message);
		bool OnObjectLoad( Message message );
        bool OnQuit(Message message);

		vh::UniformBuffers m_uniformBuffersPerFrame;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{0};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    vh::Pipeline m_graphicsPipeline;

	    VkRenderPass m_renderPass;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

