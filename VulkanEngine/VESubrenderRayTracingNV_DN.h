/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#ifndef VESUBRENDER_RAYTRACING_NV_DN_H
#define VESUBRENDER_RAYTRACING_NV_DN_H

#include "DescriptorSetGenerator.h"
#include "ShaderBindingTableGenerator.h"

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
		*/
	class VESubrenderRayTracingNV_DN : public VESubrenderRayTracingNV
	{
		friend VERendererRayTracingNV;

	public:
	protected:
		struct RTPushConstants
		{
			VkBool32 enableShadows = false;
			VkBool32 enableReflections = false;
		};

		VkDescriptorSetLayout m_descriptorSetLayoutOutput = VK_NULL_HANDLE; ///<Descriptor set layout for for output image
		VkDescriptorSetLayout m_descriptorSetLayoutAS = VK_NULL_HANDLE; ///<Descriptor set layout for acceleration structure
		VkDescriptorSetLayout m_descriptorSetLayoutGeometry = VK_NULL_HANDLE; ///<Descriptor set layout for vertices and indices
		VkDescriptorSetLayout m_descriptorSetLayoutObjectUBOs = VK_NULL_HANDLE; ///<Descriptor set layout for per object resources (UBOPerEntity)
		std::vector<VkDescriptorSet> m_descriptorSetsOutput; ///<a list of resource descriptor sets of Output Images. One set for each SwapChain Image
		std::vector<VkDescriptorSet> m_descriptorSetsAS; ///<a list of resource descriptor sets of Acceleration Structures. One set for all SwapChain Images
		std::vector<VkDescriptorSet> m_descriptorSetsGeometry; ///<a list of resource descriptor sets of Vertices and Indices. One set for all SwapChain Images
		std::vector<VkDescriptorSet> m_descriptorSetsUBOs; ///<a list of resource descriptor sets of UBOPerEntity. One set for each SwapChain Image
		RTPushConstants m_pushConstants;
		uint32_t m_rayGenIndex;
		uint32_t m_hitGroupIndex;
		uint32_t m_shadowHitGroupIndex;
		uint32_t m_missIndex;
		uint32_t m_shadowMissIndex;
		VkBuffer m_shaderBindingTableBuffer;
		VkDeviceMemory m_shaderBindingTableMem;
		nv_helpers_vk::DescriptorSetGenerator m_rtDSG;
		nv_helpers_vk::ShaderBindingTableGenerator m_sbtGen;

	public:
		///Constructor of subrender fw class
		VESubrenderRayTracingNV_DN(VERendererRayTracingNV &renderer);

		///Destructor of subrender fw class
		virtual ~VESubrenderRayTracingNV_DN() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_RT;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP;
		};

		//------------------------------------------------------------------------------------------------------------------
		virtual void initSubrenderer();

		virtual void closeSubrenderer();

		virtual void recreateResources();

		void createRTGraphicsPipeline();

		void createShaderBindingTable();

		void createRaytracingDescriptorSets();

		void updateRTDescriptorSets() override;

		//------------------------------------------------------------------------------------------------------------------
		virtual void bindPipeline(VkCommandBuffer commandBuffer);

		virtual void bindDescriptorSetsPerFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex, VECamera *pCamera, VELight *pLight);

		virtual void bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		///Set the dynamic state of the pipeline - does nothing for the base class
		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);

		//------------------------------------------------------------------------------------------------------------------
		///Prepare to perform draw operation, e.g. for an overlay
		virtual void prepareDraw() {};

		virtual void afterDrawFinished(); //after all draw calls have been recorded

		//Draw all entities that are managed by this subrenderer
		virtual void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow = {});

		///Perform an arbitrary draw operation
		///\returns a semaphore signalling when this draw operations has finished
		virtual VkSemaphore draw(uint32_t imageIndex, VkSemaphore wait_semaphore)
		{
			return VK_NULL_HANDLE;
		};

		virtual void drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex);

		//------------------------------------------------------------------------------------------------------------------
		virtual void addEntity(VEEntity *pEntity);

		///\returns the number of entities that this sub renderer manages
		uint32_t getNumberEntities()
		{
			return (uint32_t)m_entities.size();
		};

		///return the layout of the local pipeline
		VkPipelineLayout getPipelineLayout()
		{
			return m_pipelineLayout;
		};
	};
} // namespace ve

#endif
