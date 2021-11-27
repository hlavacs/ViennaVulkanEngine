/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VE_SUBRENDER_DF_DN_H
#define VE_SUBRENDER_DF_DN_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
		*/
	class VESubrenderDF_DN : public VESubrenderDF
	{
	protected:
	public:
		///Constructor
		VESubrenderDF_DN(VERendererDeferred &renderer)
			: VESubrenderDF(renderer) {};

		///Destructor
		virtual ~VESubrenderDF_DN() {};

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