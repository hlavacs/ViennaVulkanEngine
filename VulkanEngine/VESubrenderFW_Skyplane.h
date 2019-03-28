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
	class VESubrenderFW_Skyplane : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_Skyplane() { m_type = VE_SUBRENDERER_TYPE_SKYPLANE; };
		///Destructor
		virtual ~VESubrenderFW_Skyplane() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}



