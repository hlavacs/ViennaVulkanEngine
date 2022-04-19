/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDERFWSHADOW_H
#define VESUBRENDERFWSHADOW_H

namespace ve
{
	/**
		* \brief Subrenderer that manages draws the shadow pass
		*/
	class VESubrenderFW_Shadow : public VESubrenderFW
	{
	protected:
	public:
		///Constructor
		VESubrenderFW_Shadow(VERendererForward &renderer)
			: VESubrenderFW(renderer) {};

		///Destructor
		virtual ~VESubrenderFW_Shadow() {};

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

		void bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);

		virtual void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass, VECamera *pCamera, VELight *pLight, std::vector<VkDescriptorSet> descriptorSetsShadow);
	};
} // namespace ve

#endif
