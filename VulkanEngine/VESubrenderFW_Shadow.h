/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

namespace ve {

	/**
	* \brief Subrenderer that manages draws the shadow pass
	*/
	class VESubrenderFW_Shadow : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_Shadow() { m_type = VE_SUBRENDERER_TYPE_SHADOW; };
		///Destructor
		virtual ~VESubrenderFW_Shadow() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
		void bindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity);
		void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	};
}

