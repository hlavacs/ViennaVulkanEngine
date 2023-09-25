#ifndef VEENUMS_H
#define VEENUMS_H

namespace ve
{
	enum veRendererType : int
	{
		VE_RENDERER_TYPE_FORWARD = 0,
		VE_RENDERER_TYPE_DEFERRED = 1,
		VE_RENDERER_TYPE_RAYTRACING_NV = 2,
		VE_RENDERER_TYPE_RAYTRACING_KHR = 3,
		VE_RENDERER_TYPE_HYBRID = 4,
	};

	/**
		* \brief enums the subrenderers that are registered
		*/
	enum veSubrenderClass
	{
		VE_SUBRENDERER_CLASS_BACKGROUND, ///<Background, draw only once
		VE_SUBRENDERER_CLASS_OBJECT, ///<Object, draw once for each light
		VE_SUBRENDERER_CLASS_SHADOW, ///<Shadow renderer
		VE_SUBRENDERER_CLASS_OVERLAY, ///<GUI overlay
		VE_SUBRENDERER_CLASS_RT, ///<RT Subrenderer
		VE_SUBRENDERER_CLASS_COMPOSER ///<Composer Subrenderer for deferred rendering
	};

	/**
		* \brief enums the subrenderers that are registered
		*/
	enum veSubrenderType
	{
		VE_SUBRENDERER_TYPE_NONE, ///<No specific type
		VE_SUBRENDERER_TYPE_COLOR1, ///<Only one color per object
		VE_SUBRENDERER_TYPE_DIFFUSEMAP, ///<Use a diffuse texture
		VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP, ///<Use a diffuse texture and normal map
		VE_SUBRENDERER_TYPE_CUBEMAP, ///<Use a cubemap to create a sky box
		VE_SUBRENDERER_TYPE_CUBEMAP2, ///<Use a cubemap to create a sky box
		VE_SUBRENDERER_TYPE_SKYPLANE, ///<Use a skyplane to create a sky box
		VE_SUBRENDERER_TYPE_TERRAIN_WITH_HEIGHTMAP, ///<A tesselated terrain using a height map
		VE_SUBRENDERER_TYPE_NUKLEAR, ///<A Nuklear based GUI
		VE_SUBRENDERER_TYPE_SHADOW, ///<Draw entities for the shadow pass

		//-------------------------------Cloth-Simulation-Stuff---------------------------------
		// by Felix Neumann
		VE_SUBRENDERER_TYPE_CLOTH	///<Use a Diffuse texture to draw a cloth (two sided)
	};

} // namespace ve
#endif