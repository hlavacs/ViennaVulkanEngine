#pragma once

namespace vve
{

	static const uint32_t MAX_NUMBER_LIGHTS = 128;

    class RendererForward11 : public Renderer {

		friend class RendererForward;

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    	vvh::Pipeline m_graphicsPipeline;
		};

    public:
        RendererForward11(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererForward11();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
		bool OnObjectCreate( Message message );
		bool OnObjectDestroy( Message message );
        bool OnQuit(Message message);
		void CreatePipelines();

		PipelinePerType* getPipelinePerType(std::string type);
		std::string getPipelineType(ObjectHandle handle, vvh::VertexData &vertexData);

		//parameters per frame
		vvh::Buffer m_uniformBuffersPerFrame;
		vvh::Buffer m_storageBuffersLights;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vvh::DescriptorSet m_descriptorSetPerFrame{0};
		std::map<int, PipelinePerType> m_pipelinesPerType;
	    VkRenderPass m_renderPassClear;
	    VkRenderPass m_renderPass;
	    VkDescriptorPool m_descriptorPool;    
		std::vector<VkCommandPool> m_commandPools;

		int m_pass;
		glm::ivec3 m_numberLightsPerType{0,0,0};
    };

};   // namespace vve

