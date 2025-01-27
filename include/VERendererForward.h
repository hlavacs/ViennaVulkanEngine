#pragma once

namespace vve
{
    class RendererForward : public Renderer {

	    struct UniformBufferObject {
	        alignas(16) glm::mat4 model;
	        alignas(16) glm::mat4 modelInverseTranspose;
			alignas(16) vh::Color color{}; //can be used as parameter for shader		
	    };
	
	    struct UniformBufferFrame {
	        alignas(16) glm::mat4 view;
	        alignas(16) glm::mat4 proj;
	    };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    	vh::Pipeline m_graphicsPipeline;
		};

    public:
        RendererForward(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward();

    private:
        bool OnInit(Message message);
        bool OnRecordNextFrame(Message message);
		bool OnObjectCreate( Message message );
        bool OnQuit(Message message);

		PipelinePerType& getPipelinePerType(std::string type, vh::VertexData &vertexData);
		std::string getPipelineType(ObjectHandle handle, vh::VertexData &vertexData);

		vh::UniformBuffers m_uniformBuffersPerFrame;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{0};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    vh::Pipeline m_graphicsPipeline;

		std::unordered_map<std::string, PipelinePerType> m_pipelinesPerType;

	    VkRenderPass m_renderPass;
	    VkDescriptorPool m_descriptorPool;    
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
    };

};   // namespace vve

