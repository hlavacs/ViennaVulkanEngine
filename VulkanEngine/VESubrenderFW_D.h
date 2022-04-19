/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDERFWD_H
#define VESUBRENDERFWD_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have one diffuse texture for coloring
		*/
	class VESubrenderFW_D : public VESubrenderFW
	{
	public:
		///Constructor
		VESubrenderFW_D(VERendererForward &renderer)
			: VESubrenderFW(renderer) {};

		///Destructor
		virtual ~VESubrenderFW_D() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_OBJECT;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_DIFFUSEMAP;
		};

		virtual void initSubrenderer();

		virtual void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass);

		virtual void addEntity(VEEntity *pEntity);
	};
} // namespace ve

#endif
