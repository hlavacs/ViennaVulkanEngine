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
	class VESubrenderCubemap : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderCubemap() { m_type = VE_SUBRENDERER_TYPE_CUBEMAP; };
		///Destructor
		virtual ~VESubrenderCubemap() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}
