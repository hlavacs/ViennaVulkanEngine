#pragma once

namespace vve
{
    class RendererForward11 : public Renderer {

		friend class RendererForward;

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    	vh::Pipeline m_graphicsPipeline;
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

		static const int size_pos = sizeof(glm::vec3);
		static const int size_nor = sizeof(glm::vec3);
		static const int size_tex = sizeof(glm::vec2);
		static const int size_col = sizeof(glm::vec4);
		static const int size_tan = sizeof(glm::vec3);

		void getBindingDescription( std::string type, std::string C, int &binding, int stride, auto& bdesc );
		auto getBindingDescriptions(std::string type) -> std::vector<VkVertexInputBindingDescription>;
		void addAttributeDescription( std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd );
        auto getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription>;

		PipelinePerType* getPipelinePerType(std::string type);
		std::string getPipelineType(ObjectHandle handle, vh::VertexData &vertexData);

		//parameters per frame
		vh::UniformBuffers m_uniformBuffersPerFrame;
		vh::UniformBuffers m_uniformBuffersLights;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{0};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
	    vh::Pipeline m_graphicsPipeline;

		std::map<int, PipelinePerType> m_pipelinesPerType;

	    VkRenderPass m_renderPass;
	    VkDescriptorPool m_descriptorPool;    
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;

		size_t m_maxNumberLights{10};
		std::vector<vh::Light> m_lights;
		glm::ivec3 m_numberLightsPerType{0,0,0};
    };

};   // namespace vve

