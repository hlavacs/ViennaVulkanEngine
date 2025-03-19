/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/
#ifndef VE_SUBRENDER_DF_SHADOW_H
#define VE_SUBRENDER_DF_SHADOW_H

namespace ve
{
	/**
		* \brief Subrenderer that manages draws the shadow pass
		*/
	class VESubrenderDF_Shadow : public VESubrenderDF
	{
	protected:
	public:
		///Constructor
		VESubrenderDF_Shadow(VERendererDeferred &renderer)
			: VESubrenderDF(renderer) {};

		///Destructor
		virtual ~VESubrenderDF_Shadow() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_SHADOW;
		};
		///\returns the type of the subrenderer

		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_SHADOW;
		};

		virtual void initSubrenderer();

		virtual void addEntity(VEEntity *pEntity);

		void bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) override;

		virtual void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);

		virtual void bindDescriptorSetsPerFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);
	};
} // namespace ve

#endif