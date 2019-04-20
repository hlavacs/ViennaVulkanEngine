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


	};
}


