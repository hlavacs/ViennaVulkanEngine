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
	class VESubrenderDF_C1 : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderDF_C1() {};
		///Destructor
		virtual ~VESubrenderDF_C1() {};

        ///\returns the class of the subrenderer
        virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_OBJECT; };
        ///\returns the type of the subrenderer
        virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_COLOR1; };

		virtual void initSubrenderer();
	};
}


