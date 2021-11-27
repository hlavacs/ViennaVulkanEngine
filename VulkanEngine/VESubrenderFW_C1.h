/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VESUBRENDERFWC1_H
#define VESUBRENDERFWC1_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have only one color
		*/
	class VESubrenderFW_C1 : public VESubrenderFW
	{
	protected:
	public:
		///Constructor
		VESubrenderFW_C1(VERendererForward &renderer)
			: VESubrenderFW(renderer) {};

		///Destructor
		virtual ~VESubrenderFW_C1() {};

		///\returns the class of the subrenderer
		virtual veSubrenderClass getClass()
		{
			return VE_SUBRENDERER_CLASS_OBJECT;
		};

		///\returns the type of the subrenderer
		virtual veSubrenderType getType()
		{
			return VE_SUBRENDERER_TYPE_COLOR1;
		};

		virtual void initSubrenderer();
	};
} // namespace ve

#endif
