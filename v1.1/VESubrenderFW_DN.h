/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDERFWDN_H
#define VESUBRENDERFWDN_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
		*/
	class VESubrenderFW_DN : public VESubrenderFW
	{
	public:
		///Constructor for class VESubrenderFW_DN
		VESubrenderFW_DN(VERendererForward &renderer)
			: VESubrenderFW(renderer) {};

		///Destructor for class VESubrenderFW_DN
		virtual ~VESubrenderFW_DN() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_OBJECT;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP;
		};

		virtual void initSubrenderer();

		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);

		virtual void addEntity(VEEntity *pEntity);
	};
} // namespace ve

#endif
