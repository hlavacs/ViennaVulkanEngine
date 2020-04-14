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
#include <fstream>
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
#include <string>


#define VECTOR std::vector<T>
#define VECTORPAR  //memcopy,align,capacity
#define VECTORPAR2 //memcopy

namespace vve {


	//----------------------------------------------------------------------------------
	//basic data types

	//typedef uint64_t VeCount;
	//inline VeCount operator""_Cnt(uint64_t val) { return VeCount(val); };
	enum class VeCount : uint64_t { };										///<for counting objects
	inline VeCount operator+(VeCount count, VeCount rcount) { return VeCount((uint64_t)count + (uint64_t)rcount); };
	inline VeCount operator-(VeCount count, VeCount rcount) { return VeCount((uint64_t)count - (uint64_t)rcount); };
	inline VeCount operator++(VeCount &count) { count = VeCount((uint64_t)count + 1); return count; };
	inline VeCount operator--(VeCount &count) { count = VeCount((uint64_t)count - 1); return count; };
	inline VeCount operator""_Cnt(uint64_t val) { return VeCount(val); };
	inline uint64_t operator+(VeCount l, int r) { return (uint64_t)l + r; };
	inline uint64_t operator-(VeCount l, int r) { return (uint64_t)l - r; };

	typedef uint64_t VeIndex;												///<for indexing in large data collections
	constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();	///<a null index pointing nowhere
	using VeIndexPair = std::pair<VeIndex, VeIndex>;						///<a pair of indices
	using VeIndexTriple = std::tuple<VeIndex, VeIndex, VeIndex>;			///<a triplet of indices
	inline std::ostream& operator<<(std::ostream& stream, VeIndexPair& pair) {
		stream << pair.first << " " << std::skipws << pair.second;
		return stream;
	};
	inline std::ostream& operator<<(std::ostream& stream, VeIndexTriple& triple) {
		stream << std::get<0>(triple) << " " << std::skipws << std::get<1>(triple) << " " << std::skipws << std::get<2>(triple);
		return stream;
	};

	enum class VeHandle : uint64_t {};										///<64 bit handle for uniquely identifying objects in tables
	constexpr VeHandle VE_NULL_HANDLE = VeHandle(std::numeric_limits<uint64_t>::max());				///<an empty handle pointing to no objects
	using VeHandlePair = std::pair<VeHandle, VeHandle>;						///<a pair of handles
	using VeHandleTriple = std::tuple<VeHandle, VeHandle, VeHandle>;		///<a tripuint64_t let of hanVeHandle l, int r dle return l + r; s;
	inline uint64_t operator+(VeHandle l, uint64_t r) { return (uint64_t)l + r; };
	inline bool operator<(VeHandle l, VeHandle r) { return (uint64_t)l < (uint64_t)r; };
	inline bool operator<=(VeHandle l, VeHandle r) { return (uint64_t)l <= (uint64_t)r; };
	inline VeHandle operator""_Hd(uint64_t val) { return VeHandle(val); };
	inline std::ostream& operator<<(std::ostream& stream, VeHandle& handle) {
		stream << handle + 0;
		return stream;
	};
	inline std::ostream& operator<<(std::ostream& stream, VeHandlePair& pair) {
		stream << pair.first + 0 << " " << std::skipws << pair.second + 0;
		return stream;
	};
	inline std::ostream& operator<<(std::ostream& stream, VeHandleTriple& triple) {
		stream << std::get<0>(triple) + 0 << " " << std::skipws << std::get<1>(triple) + 0 << " " << std::skipws << std::get<2>(triple) + 0;
		return stream;
	};

	enum class VeKey : uint64_t {};														///<64 bit handle for uniquely identifying objects in tables
	constexpr VeKey VE_NULL_KEY = VeKey(std::numeric_limits<uint64_t>::max());		///<an empty handle pointing to no objects
	using VeKeyPair = std::pair<VeKey, VeKey>;						///<a pair of handles
	using VeKeyTriple = std::tuple<VeKey, VeKey, VeKey>;		///<a tripuint64_t let of hanVeHandle l, int r dle return l + r; s;
	inline uint64_t operator+(VeKey l, uint64_t r) { return (uint64_t)l + r; };
	inline bool operator<(VeKey l, VeKey r) { return (uint64_t)l < (uint64_t)r; };
	inline bool operator<=(VeKey l, VeKey r) { return (uint64_t)l <= (uint64_t)r; };
	inline VeKey operator""_Ke(uint64_t val) { return VeKey(val); };
	inline std::ostream& operator<<(std::ostream& stream, VeKey& key) {
		stream << key + 0;
		return stream;
	};
	inline std::ostream& operator<<(std::ostream& stream, VeKeyPair& pair) {
		stream << pair.first + 0 << " " << std::skipws << pair.second + 0;
		return stream;
	};
	inline std::ostream& operator<<(std::ostream& stream, VeKeyTriple& triple) {
		stream << std::get<0>(triple) + 0 << " " << std::skipws << std::get<1>(triple) + 0 << " " << std::skipws << std::get<2>(triple) + 0;
		return stream;
	};

	enum class VeValue : uint64_t {};														///<64 bit handle for uniquely identifying objects in tables
	constexpr VeValue VE_NULL_VALUE = VeValue(std::numeric_limits<uint64_t>::max());		///<an empty handle pointing to no objects
	inline uint64_t operator+(VeValue l, uint64_t r) { return (uint64_t)l + r; };
	inline bool operator<(VeValue l, VeValue r) { return (uint64_t)l < (uint64_t)r; };
	inline bool operator<=(VeValue l, VeValue r) { return (uint64_t)l <= (uint64_t)r; };
	inline VeValue operator""_Va(uint64_t val) { return VeValue(val); };
	inline std::ostream& operator<<(std::ostream& stream, VeValue& value) {
		stream << value + 0;
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

#include "VELabels.h"
#include "VEMemTable.h"
#include "VESystem.h"

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


