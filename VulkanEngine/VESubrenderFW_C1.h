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
	class VESubrenderFW_C1 : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_C1() { m_type = VE_SUBRENDERER_TYPE_COLOR1; };
		///Destructor
		virtual ~VESubrenderFW_C1() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}


