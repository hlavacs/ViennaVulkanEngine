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

		/**
		* \brief enums the subrenderers that are registered
		*/
		enum veSubrenderClass {
			VE_SUBRENDERER_CLASS_BACKGROUND,					///<Background, draw only once
			VE_SUBRENDERER_CLASS_OBJECT,						///<Object, draw once for each light
			VE_SUBRENDERER_CLASS_SHADOW,						///<Shadow renderer
			VE_SUBRENDERER_CLASS_OVERLAY						///<GUI overlay
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
			VE_SUBRENDERER_TYPE_CUBEMAP2,					///<Use a cubemap to create a sky box
			VE_SUBRENDERER_TYPE_SKYPLANE,					///<Use a cubemap to create a sky box
			VE_SUBRENDERER_TYPE_TERRAIN_WITH_HEIGHTMAP,		///<A tesselated terrain using a height map
			VE_SUBRENDERER_TYPE_SHADOW						///<Draw entities for the shadow pass
		};

	protected:
		VkDescriptorSetLayout	m_descriptorSetLayoutResources = VK_NULL_HANDLE;	///<Descriptor set 3 : per object additional resources
		VkPipelineLayout		m_pipelineLayout = VK_NULL_HANDLE;					///<Pipeline layout
		VkPipeline				m_pipeline = VK_NULL_HANDLE;						///<Pipeline for light pass

		std::vector<VEEntity *> m_entities;											///<List of associated entities

	public:
		///Constructor of subrender class
		VESubrender() {};
		///Destructor of subrender class
		virtual ~VESubrender() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()=0;
		///\returns the type of the subrenderer
		virtual veSubrenderType getType()=0;

		///Create descriptor set layout, pipeline layout and PSO
		virtual void	initSubrenderer() {};
		virtual void	closeSubrenderer();
		virtual void	recreateResources();

		virtual void	bindPipeline( VkCommandBuffer commandBuffer ); 
		virtual void	bindDescriptorSetsPerFrame(	VkCommandBuffer commandBuffer, uint32_t imageIndex, 
													VECamera *pCamera, VELight *pLight, 
													std::vector<VkDescriptorSet> descriptorSetsShadow );
		virtual void	bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		///Set the dynamic state of the pipeline - does nothing for the base class
		virtual void	setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) {};

		virtual void	draw(	VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass,
								VECamera *pCamera, VELight *pLight,
								std::vector<VkDescriptorSet> descriptorSetsShadow );

		virtual void	drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);
		
		virtual void	addEntity( VEEntity *pEntity );
		virtual void	removeEntity(VEEntity *pEntity);
		///\returns the number of entities that this sub renderer manages
		uint32_t		getNumberEntities() { return (uint32_t)m_entities.size(); };
		
		///return the layout of the local pipeline
		VkPipelineLayout getPipelineLayout() { return m_pipelineLayout; };
	};


}
