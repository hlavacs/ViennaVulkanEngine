/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

namespace ve {

	/**
	* \brief Subrenderer that manages entities that are cubemap based sky boxes
	*/
	class VESubrenderFW_Cubemap : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_Cubemap() { m_type = VE_SUBRENDERER_TYPE_CUBEMAP; };
		///Destructor
		virtual ~VESubrenderFW_Cubemap() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}
