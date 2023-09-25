/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VEINCLUDE_H
#define VEINCLUDE_H

#include "VEEnums.h"

#include "VHHelper.h"

#include "VENamedClass.h"
#include "VEEventListener.h"
#include "VEEventListenerGLFW.h"
#include "VEEventListenerNuklear.h"
#include "VEEventListenerNuklearDebug.h"
#include "VEWindow.h"
#include "VEWindowGLFW.h"
#include "VEEngine.h"
#include "VEMaterial.h"
#include "VEEntity.h"
#include "VESceneManager.h"
#include "VERenderer.h"
#include "VERendererForward.h"
#include "VERendererDeferred.h"
#include "VERendererRayTracingNV.h"
#include "VERendererRayTracingKHR.h"
#include "VESubrender.h"
#include "VESubrenderFW.h"
#include "VESubrenderDF.h"
#include "VESubrenderRayTracingNV.h"
#include "VESubrenderRayTracingKHR.h"
#include "VESubrender_Nuklear.h"
#include "VESubrenderFW_C1.h"
#include "VESubrenderFW_Skyplane.h"
#include "VESubrenderFW_Cloth.h"
#include "VESubrenderFW_D.h"
#include "VESubrenderFW_DN.h"
#include "VESubrenderFW_Shadow.h"
#include "VESubrenderDF_C1.h"
#include "VESubrenderDF_D.h"
#include "VESubrenderDF_DN.h"
#include "VESubrenderDF_Shadow.h"
#include "VESubrenderDF_Skyplane.h"
#include "VESubrenderDF_Composer.h"
#include "VESubrenderRayTracingNV_DN.h"
#include "VESubrenderRayTracingKHR_DN.h"

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
