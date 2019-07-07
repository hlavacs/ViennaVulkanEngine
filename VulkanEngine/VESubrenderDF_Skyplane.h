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
	class VESubrenderDF_Skyplane : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderDF_Skyplane() {};
		///Destructor
		virtual ~VESubrenderDF_Skyplane() {};

        ///\returns the class of the subrenderer
        virtual veSubrenderClass getClass() { return VE_SUBRENDERER_CLASS_BACKGROUND; };
        ///\returns the type of the subrenderer
        virtual veSubrenderType getType() { return VE_SUBRENDERER_TYPE_SKYPLANE; };
		
        virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}



