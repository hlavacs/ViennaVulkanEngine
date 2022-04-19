/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VE_SUBRENDER_DF_C1_H
#define VE_SUBRENDER_DF_C1_H

namespace ve
{
	/**
		* \brief Subrenderer that manages entities that have only one color
		*/
	class VESubrenderDF_C1 : public VESubrenderDF
	{
	protected:
	public:
		///Constructor
		VESubrenderDF_C1(VERendererDeferred &renderer)
			: VESubrenderDF(renderer) {};

		///Destructor
		virtual ~VESubrenderDF_C1() {};

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
