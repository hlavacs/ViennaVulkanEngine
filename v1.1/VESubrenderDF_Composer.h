/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#ifndef VE_SUBRENDER_DF_COMPOSER_H
#define VE_SUBRENDER_DF_COMPOSER_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
		*/
	class VESubrenderDF_Composer : public VESubrenderDF
	{
	public:
		///Constructor
		VESubrenderDF_Composer(VERendererDeferred &renderer)
			: VESubrenderDF(renderer) {};

		///Destructor
		virtual ~VESubrenderDF_Composer() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_COMPOSER;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_NONE;
		};

		virtual void initSubrenderer() override;

		virtual void addEntity(VEEntity *pEntity) override;

		void setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) override;

		virtual void bindDescriptorSetsPerFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);

		void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);

		void bindOffscreenDescriptorSet(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	};
} // namespace ve

#endif