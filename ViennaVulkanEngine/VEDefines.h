#pragma once

#include <type_traits>
#include <iostream>
#include <vector>
#include <limits>
#include <array>
#include <map>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <atomic>
#include <random>
#include <assert.h>
#include <chrono>


namespace vve {
	typedef uint32_t VeIndex;
	constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();

	typedef uint64_t VeHandle;
	constexpr VeHandle VE_NULL_HANDLE = std::numeric_limits<VeHandle>::max();

	typedef uint32_t VeSize;
	constexpr VeSize VE_NULL_SIZE = std::numeric_limits<VeSize>::max();

}

#ifndef VE_PUBLIC_INTERFACE

#define VE_ENABLE_MULTITHREADING
#include "VEGameJobSystem.h"
#include "VEVector.h"
#include "VETable.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/hash.hpp"
#include "glm/gtx/transform.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

#include "stb_image.h"
#include "stb_image_write.h"
//#include <gli/gli.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <irrKlang.h>


#endif
