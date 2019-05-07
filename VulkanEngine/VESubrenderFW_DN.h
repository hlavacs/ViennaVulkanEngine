/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

namespace ve {

	/**
	* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
	*/
	class VESubrenderFW_DN : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_DN() { };
		///Destructor
		virtual ~VESubrenderFW_DN() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_OBJECT; };
		///\returns the type of the subrenderer
		virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP; };

		virtual void initSubrenderer();
		//virtual void bindPipeline(VkCommandBuffer commandBuffer);
		//virtual void bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);
		virtual void addEntity(VEEntity *pEntity);
	};
}

