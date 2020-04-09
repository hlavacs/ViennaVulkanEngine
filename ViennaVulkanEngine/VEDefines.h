#pragma once

/**
*
* \file
* \brief All includes of external sources are funneled through this include file.
*
* This is the basic include file of all engine parts. It includes external include files, 
* defines all basic data types, and defines operators for output and hashing for them.
*
*/

///include STL 
#include <type_traits>
#include <iostream>
#include <vector>
#include <limits>
#include <array>
#include <map>
#include <unordered_map>
#include <set>
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

	//----------------------------------------------------------------------------------
	//basic data types

	enum class VeCount : uint64_t { };										///<for counting objects
	inline VeCount operator+(VeCount count, VeCount rcount) { return VeCount((uint64_t)count + (uint64_t)rcount); };
	inline VeCount operator-(VeCount count, VeCount rcount) { return VeCount((uint64_t)count - (uint64_t)rcount); };
	inline VeCount operator++(VeCount count) { count = VeCount((uint64_t)count + 1); return count; };
	inline VeCount operator--(VeCount count) { count = VeCount((uint64_t)count - 1); return count; };

	typedef uint32_t VeIndex;												///<for indexing in large data collections
	constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();	///<a null index pointing nowhere
	using VeIndexPair = std::pair<VeIndex, VeIndex>;						///<a pair of indices
	using VeIndexTriple = std::tuple<VeIndex, VeIndex, VeIndex>;			///<a triplet of indices

	enum class VeHandle : uint64_t {};										///<64 bit handle for uniquely identifying objects in tables
	constexpr VeHandle VE_NULL_HANDLE = VeHandle(std::numeric_limits<uint64_t>::max());				///<an empty handle pointing to no objects
	using VeHandlePair = std::pair<VeHandle, VeHandle>;						///<a pair of handles
	using VeHandleTriple = std::tuple<VeHandle, VeHandle, VeHandle>;		///<a tripuint64_t let of hanVeHandle l, int r dle return l + r; s;
	inline uint64_t operator+(VeHandle l, int r) { return (uint64_t)l + r; };
	inline VeHandle operator""_Hd(uint64_t val) { return VeHandle(val); };
	inline std::ostream& operator<<(std::ostream& stream, VeHandle& handle) {
		stream << handle + 0;
		return stream;
	};


	//----------------------------------------------------------------------------------
	//operators for console output

	/**
	*
	*	\brief Operator to output handle pairs
	*
	*	\param[in] stream The stream that is output into.
	*	\param[in] pair The handle pair to be output
	*	\returns altered output stream.
	*
	*/
	inline std::ostream& operator<<(std::ostream& stream, VeHandlePair& pair) {
		stream << pair.first+0;
		stream << std::skipws;
		stream << pair.second+0;
		return stream;
	};

	/**
	*
	*	\brief Operator to output index pairs
	*
	*	\param[in] stream The stream that is output into.
	*	\param[in] pair The index pair to be output
	*	\returns altered output stream.
	*
	*/
	inline std::ostream& operator<<(std::ostream& stream, VeIndexPair& pair) {
		stream << pair.first;
		stream << std::skipws;
		stream << pair.second;
		return stream;
	};

	/**
	*
	*	\brief Operator to output handle triplets
	*
	*	\param[in] stream The stream that is output into.
	*	\param[in] pair The handle triple to be output
	*	\returns altered output stream.
	*
	*/
	inline std::ostream& operator<<(std::ostream& stream, VeHandleTriple& triple) {
		stream << std::get<0>(triple)+0;
		stream << std::skipws;
		stream << std::get<1>(triple)+0;
		stream << std::skipws;
		stream << std::get<2>(triple)+0;
		return stream;
	};

	/**
	*
	*	\brief Operator to output index triplets
	*
	*	\param[in] stream The stream that is output into.
	*	\param[in] pair The index triple to be output
	*	\returns altered output stream.
	*
	*/
	inline std::ostream& operator<<(std::ostream& stream, VeIndexTriple& triple) {
		stream << std::get<0>(triple);
		stream << std::skipws;
		stream << std::get<1>(triple);
		stream << std::skipws;
		stream << std::get<2>(triple);
		return stream;
	};


	//----------------------------------------------------------------------------------
	//hashing specialization, mainly for the hashed map

	/**
	*
	*	\brief Computes a hash over a pair of data items.
	*
	*	\param[in] val A pair of objects that the hash is computer over
	*	\returns the computed hash.
	*
	*/
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

	/**
	*
	*	\brief Computes a hash over a triple of data items.
	*
	*	\param[in] val A triple of objects that the hash is computer over
	*	\returns the computed hash.
	*
	*/
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
};



#include "VEDoxygen.h"
#include "VEUtilClock.h"
#include "VEMemHeap.h"
#include "VEMemVector.h"
#include "VEMemMap.h"

///if this is defined then the engine runs in multithreaded mode. If not, in single thread mode
#define VE_ENABLE_MULTITHREADING
#include "VEGameJobSystem.h"

#include "VEMemTable.h"

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

#include "vulkan/vulkan.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#endif

//prevent modules to access globals in other modules with extern - by defining it away
//do not include any extern include file after here, or disable this statement
#define extern

