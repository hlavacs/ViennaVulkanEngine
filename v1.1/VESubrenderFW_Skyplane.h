/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDERFWSKYPLANE_H
#define VESUBRENDERFWSKYPLANE_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that are cubemap based sky boxes
		*/
	class VESubrenderFW_Skyplane : public VESubrenderFW
	{
	protected:
	public:
		///Constructor
		VESubrenderFW_Skyplane(VERendererForward &renderer)
			: VESubrenderFW(renderer) {};

		///Destructor
		virtual ~VESubrenderFW_Skyplane() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_BACKGROUND;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_SKYPLANE;
		};

		virtual void initSubrenderer();

		virtual void addEntity(VEEntity *pEntity);
	};
} // namespace ve

#endif
