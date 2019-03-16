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
	class VESubrenderC1 : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderC1() { m_type = VE_SUBRENDERER_TYPE_COLOR1; };
		///Destructor
		virtual ~VESubrenderC1() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}


