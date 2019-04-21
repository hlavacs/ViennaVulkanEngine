/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

namespace ve {

	/**
	* \brief Subrenderer that manages entities that have only one color
	*/
	class VESubrenderFW_Nuklear : public VESubrender {
	protected:
		struct nk_context *m_ctx;

	public:
		///Constructor of class VESubrenderFW_Nuklear
		VESubrenderFW_Nuklear() {};
		///Destructor of class VESubrenderFW_Nuklear
		virtual ~VESubrenderFW_Nuklear() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_OVERLAY; };
		///\returns the type of the subrenderer
		virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_NUKLEAR; };

		virtual void initSubrenderer();
		virtual void closeSubrenderer();
		virtual void draw(	VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass,
							VECamera *pCamera, VELight *pLight,
							std::vector<VkDescriptorSet> descriptorSetsShadow) {};
		virtual VkSemaphore draw(uint32_t imageIndex, VkSemaphore wait_semaphore);

		///\returns the Nuklear context
		virtual struct nk_context *getContext() { return m_ctx; };

	};
}


