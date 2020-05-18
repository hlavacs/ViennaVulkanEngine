#ifndef VEINCLUDE_H
#define VEINCLUDE_H


/**
*
* \file
* \brief
*
* Details
*
*/


///type traits
#include <type_traits>
#include <typeinfo>

///IO
#include <iostream>
#include <fstream>
#include <iomanip>

///STL 
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
#include <string>
#include <tuple>

///C++ stuff
#include <functional>
#include <atomic>
#include <random>
#include <assert.h>
#include <chrono>
#include <limits>
#include <utility>
#define _USE_MATH_DEFINES
#include <math.h>

//C++20
#include <memory_resource>
#include <experimental/coroutine>
#include <filesystem>
#include <experimental/generator>

///VVE engine includes 
#include "VETypes.h"
#include "VEDoxygen.h"
#include "VEUtilClock.h"

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





#endif

