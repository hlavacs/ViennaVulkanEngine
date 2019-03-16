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
	class VESubrenderD : public VESubrender {
	protected:

	public:
		///Constructor
		VESubrenderD() { m_type = VE_SUBRENDERER_TYPE_DIFFUSEMAP; };
		///Destructor
		virtual ~VESubrenderD() {};

		virtual void initSubrenderer();
		virtual void addEntity(VEEntity *pEntity);
	};
}

