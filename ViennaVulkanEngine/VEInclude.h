/**
*
* \file
* \brief
*
* Details
*
*/


///if this is defined then the engine runs in multithreaded mode. If not, in single thread mode
#define VE_ENABLE_MULTITHREADING
#include "VEGameJobSystem.h"


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/hash.hpp"
#include "glm/gtx/transform.hpp"

///Irrklang audio library
#include <irrKlang.h>


#ifndef VE_PUBLIC_INTERFACE
#include "stb_image.h"
#include "stb_image_write.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan/vulkan.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#endif




