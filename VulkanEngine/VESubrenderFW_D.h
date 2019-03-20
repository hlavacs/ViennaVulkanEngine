/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once


namespace ve {

	/**
	* \brief Subrenderer that manages entities that have one diffuse texture for coloring
	*/
	class VESubrenderFW_D : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderFW_D() { m_type = VE_SUBRENDERER_TYPE_DIFFUSEMAP; };
		///Destructor
		virtual ~VESubrenderFW_D() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}

