/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEINCLUDE_H
#define VEINCLUDE_H


#include "VHHelper.h"

#include "VENamedClass.h"
#include "VEEventListener.h"
#include "VEEventListenerGLFW.h"
//#include "VEEventListenerNuklear.h"
//#include "VEEventListenerNuklearDebug.h"
#include "VEWindow.h"
#include "VEWindowGLFW.h"
#include "VEEngine.h"
#include "VEMaterial.h"
#include "VEEntity.h"
#include "VESceneManager.h"
#include "VESubrender.h"
#include "VESubrenderFW.h"
#include "VESubrenderFW_C1.h"
#include "VESubrenderFW_Skyplane.h"
#include "VESubrenderFW_D.h"
//#include "VESubrenderFW_DN.h"
//#include "VESubrenderFW_Nuklear.h"
#include "VESubrenderFW_Shadow.h"
#include "VERenderer.h"
#include "VERendererForward.h"


//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VECHECKRESULT(x) { \
		VkResult retval = (x); \
		assert (retval == VK_SUCCESS); \
	}


//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VECHECKPOINTER(x) { \
		void* pointer = (x); \
		assert (pointer!=nullptr); \
	}



#endif
