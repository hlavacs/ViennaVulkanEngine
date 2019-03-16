/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

namespace ve {

	/**
	* \brief Subrenderer that manages entities that have a diffuse texture and a normal map
	*/
	class VESubrenderDN : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderDN() { m_type = VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP; };
		///Destructor
		virtual ~VESubrenderDN() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}

