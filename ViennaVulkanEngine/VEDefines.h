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


	class VeClock {
		std::chrono::high_resolution_clock::time_point m_last;
		uint32_t m_num_ticks = 0;
		double m_sum_time = 0;
		double m_avg_time = 0;
		double m_stat = 0;
		double f = 1.0;
		std::string m_name;

	public:
		VeClock( std::string name, double stat_time = 1.0 ) : m_name(name), m_stat(stat_time) {
			m_last = std::chrono::high_resolution_clock::now();
		};

		void start() {
			m_last = std::chrono::high_resolution_clock::now();
		};

		void stop() {
			auto now = std::chrono::high_resolution_clock::now();
			auto time_span = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_last);
			m_sum_time += time_span.count();
			++m_num_ticks;
			if ( m_num_ticks >= m_stat ) {
				double avg = m_sum_time / (double)m_num_ticks;
				m_avg_time = (1.0 - f) * m_avg_time + f * avg;
				f = 0.9;
				m_sum_time = 0;
				m_num_ticks = 0;
				print();
			}
		};
		
		void tick() {
			auto now = std::chrono::high_resolution_clock::now();
			auto time_span = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_last);
			m_last = now;
			m_sum_time += time_span.count();
			++m_num_ticks;
			if (m_sum_time > m_stat * std::exp(9.0 * std::log(10.0)) ) {
				double avg = m_sum_time / m_num_ticks;
				m_avg_time = (1.0 - f) * m_avg_time + f * avg;
				f = 0.9;
				m_sum_time = 0;
				m_num_ticks = 0;
				print();
			}
		};

		void print() {
			std::cout << m_name << " avg " << std::setw(7) <<  m_avg_time / 1000000.0 << " ms" << std::endl;
		};

	};


}


#include "VEVector.h"

#define VE_ENABLE_MULTITHREADING
#include "VEGameJobSystem.h"
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

