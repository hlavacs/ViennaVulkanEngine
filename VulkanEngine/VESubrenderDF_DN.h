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
	class VESubrenderDF_DN : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderDF_DN() {};
		///Destructor
		virtual ~VESubrenderDF_DN() {};

        ///\returns the class of the subrenderer
        virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_OBJECT; };
        ///\returns the type of the subrenderer

        virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP; };

		virtual void initSubrenderer();
        virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);
		virtual void addEntity(VEEntity *pEntity);
	};
}

