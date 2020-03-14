#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


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
#include <iomanip>
#include <typeinfo>
#include <functional>


namespace vve {

	typedef uint32_t VeIndex;
	constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();
	using VeIndexPair = std::pair<VeIndex, VeIndex>;
	using VeIndexTriple = std::tuple<VeIndex, VeIndex, VeIndex>;

	typedef uint64_t VeHandle;
	constexpr VeHandle VE_NULL_HANDLE = std::numeric_limits<VeHandle>::max();
	using VeHandlePair = std::pair<VeHandle, VeHandle>;
	using VeHandleTriple = std::tuple<VeHandle, VeHandle, VeHandle>;

	using VeHandleIndexPair = std::pair<const VeHandle, VeIndex>;
	using VeStringIndexPair = std::pair<const std::string, VeIndex>;


	template<typename S, typename T>
	struct std::hash<std::pair<S, T>>
	{
		inline size_t operator()(const std::pair<S, T>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(val.first) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(val.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	template<typename S, typename T, typename U>
	struct std::hash<std::tuple<S, T, U>>
	{
		inline size_t operator()(const std::tuple<S, T, U>& val) const
		{
			size_t seed = 0;
			seed ^= std::hash<S>()(std::get<0>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<1>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= std::hash<T>()(std::get<2>(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};


}


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
#include <irrKlang.h>

#ifndef VE_PUBLIC_INTERFACE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#endif

