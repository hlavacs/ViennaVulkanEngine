/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

class VERenderer;

namespace ve {


	/**
	*
	* \brief A VESubrender instance manages and draws its entities
	*
	* Subrenderers understand the data that is attached to entities and create and manage the according Vulkan resources.
	* The also create PSOs and use them to draw entities. Subrenderers maintain a list of entities associated with them.
	*
	*/
	class VESubrender {

	public:

		///Data that is updated for each object
		struct veUBOPerObject {
			glm::mat4 model;			///<Object model matrix
			glm::mat4 modelInvTrans;	///<Inverse transpose
			glm::vec4 color;			///<Uniform color if needed by shader
		};

		/**
		* \brief enums the subrenderers that are registered
		*/
		enum veSubrenderType {
			VE_SUBRENDERER_TYPE_NONE,						///<No specific type
			VE_SUBRENDERER_TYPE_COLOR1,						///<Only one color per object
			VE_SUBRENDERER_TYPE_DIFFUSEMAP,					///<Use a diffuse texture
			VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP,		///<Use a diffuse texture and normal map
			VE_SUBRENDERER_TYPE_CUBEMAP,					///<Use a cubemap to create a sky box
			VE_SUBRENDERER_TYPE_TERRAIN_WITH_HEIGHTMAP,		///<A tesselated terrain using a height map
			VE_SUBRENDERER_TYPE_SHADOW						///<Draw entities for the shadow pass
		};

	protected:
		veSubrenderType m_type = VE_SUBRENDERER_TYPE_NONE;	///<Type of the subrenderer

		VkDescriptorSetLayout m_descriptorSetLayoutUBO = VK_NULL_HANDLE;		///<Descriptor set 1 : per object UBO
		VkDescriptorSetLayout m_descriptorSetLayoutResources = VK_NULL_HANDLE;	///<Descriptor set 3 : per object additional resources
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;				///<Pipeline layout
		VkPipeline m_pipeline = VK_NULL_HANDLE;				///<Pipeline for light pass

		std::vector<VEEntity *> m_entities;					///<List of associated entities

	public:
		///Constructor
		VESubrender() {};
		///Destructor
		virtual ~VESubrender() {};

		///\returns the type of the subrenderer
		veSubrenderType getType() { return m_type; };

		///Create descriptor set layout, pipeline layout and PSO
		virtual void	initSubrenderer() {};
		virtual void	closeSubrenderer();
		virtual void	recreateResources();

		virtual void	draw( VkCommandBuffer commandBuffer, uint32_t imageIndex );
		virtual void	drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);
		virtual void	bindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);
		virtual void	addEntity( VEEntity *pEntity );
		virtual void	removeEntity(VEEntity *pEntity);
	};


}
