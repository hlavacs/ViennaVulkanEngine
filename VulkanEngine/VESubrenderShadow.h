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
	class VESubrenderShadow : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderShadow() { m_type = VE_SUBRENDERER_TYPE_SHADOW; };
		///Destructor
		virtual ~VESubrenderShadow() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
		void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	};
}

