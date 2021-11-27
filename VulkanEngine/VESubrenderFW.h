/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VHSUBRENDERFW_H
#define VHSUBRENDERFW_H

#include "VESubrender.h"

namespace ve
{
	class VERendererForward;

	/**
		*
		* \brief A VESubrender instance manages and draws its entities
		*
		* Subrenderers understand the data that is attached to entities and create and manage the according Vulkan resources.
		* The also create PSOs and use them to draw entities. Subrenderers maintain a list of entities associated with them.
		*
		*/
	class VESubrenderFW : public VESubrender
	{
	public:
	protected:
		VERendererForward &m_renderer;
		uint32_t m_resourceArrayLength = 16; ///<Length of resource array in shader
		VkDescriptorSetLayout m_descriptorSetLayoutResources = VK_NULL_HANDLE; ///<Descriptor set layout for per object resources (like images)
		std::vector<VkDescriptorSet> m_descriptorSetsResources; ///<a list of resource descriptor set arrays, maps are condensed into these arrays of size K
		std::vector<std::vector<VkDescriptorImageInfo>> m_maps; ///<descriptor write info for the  maps, m_maps[0] may contain all diffuse maps, m_maps[1] all normal maps etc
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE; ///<Pipeline layout
		std::vector<VkPipeline> m_pipelines; ///<Pipelines for light pass(es)
		uint32_t m_idxLastRecorded = 0; ///<Used for incremental command buffer recording, idx of last recorded entity

	public:
		///Constructor of subrender fw class
		VESubrenderFW(VERendererForward &renderer)
			: m_renderer(renderer) {};

		///Destructor of subrender fw class
		virtual ~VESubrenderFW() {};

		///\returns the class of the subrenderer, a very general description
		virtual veSubrenderClass getClass() = 0;

		///\returns the type of the subrenderer, specific type inside a class
		virtual veSubrenderType getType() = 0;

		//------------------------------------------------------------------------------------------------------------------
		virtual void initSubrenderer();

		virtual void closeSubrenderer();

		virtual void recreateResources();

		//------------------------------------------------------------------------------------------------------------------
		virtual void bindPipeline(VkCommandBuffer commandBuffer);

		virtual void bindDescriptorSetsPerFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);

		virtual void bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		///Set the dynamic state of the pipeline - does nothing for the base class
		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) {};

		//------------------------------------------------------------------------------------------------------------------
		///Prepare to perform draw operation, e.g. for an overlay
		virtual void prepareDraw() {};

		virtual void afterDrawFinished(); //after all draw calls have been recorded

		//Draw all entities that are managed by this subrenderer
		virtual void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);

		///Perform an arbitrary draw operation
		///\returns a semaphore signalling when this draw operations has finished
		virtual VkSemaphore draw(uint32_t imageIndex, VkSemaphore wait_semaphore)
		{
			return VK_NULL_HANDLE;
		};

		virtual void drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		//------------------------------------------------------------------------------------------------------------------
		virtual void addEntity(VEEntity *pEntity)
		{
			VESubrender::addEntity(pEntity);
		};

		virtual void addMaps(VEEntity *pEntity, std::vector<VkDescriptorImageInfo> &newMaps);

		virtual void removeEntity(VEEntity *pEntity);

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
