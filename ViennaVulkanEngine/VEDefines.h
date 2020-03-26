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

	typedef uint32_t VeCount;

	typedef uint32_t VeIndex;
	constexpr VeIndex VE_NULL_INDEX = std::numeric_limits<VeIndex>::max();
	constexpr VeIndex VE_MAX_INDEX = VE_NULL_INDEX - 1;
	using VeIndexPair = std::pair<VeIndex, VeIndex>;
	using VeIndexTriple = std::tuple<VeIndex, VeIndex, VeIndex>;

	typedef uint64_t VeHandle;
	constexpr VeHandle VE_NULL_HANDLE = std::numeric_limits<VeHandle>::max();
	constexpr VeHandle VE_MAX_HANDLE = VE_NULL_HANDLE - 1;
	using VeHandlePair = std::pair<VeHandle, VeHandle>;
	using VeHandleTriple = std::tuple<VeHandle, VeHandle, VeHandle>;

	inline std::ostream& operator<<(std::ostream& stream, VeHandlePair& pair) {
		stream << pair.first;
		stream << std::skipws;
		stream << pair.second;
		return stream;
	};

	inline std::ostream& operator<<(std::ostream& stream, VeIndexPair& pair) {
		stream << pair.first;
		stream << std::skipws;
		stream << pair.second;
		return stream;
	};

	inline std::ostream& operator<<(std::ostream& stream, VeHandleTriple& triple) {
		stream << std::get<0>(triple);
		stream << std::skipws;
		stream << std::get<1>(triple);
		stream << std::skipws;
		stream << std::get<2>(triple);
		return stream;
	};

	inline std::ostream& operator<<(std::ostream& stream, VeIndexTriple& triple) {
		stream << std::get<0>(triple);
		stream << std::skipws;
		stream << std::get<1>(triple);
		stream << std::skipws;
		stream << std::get<2>(triple);
		return stream;
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
				f = f - (f - 0.9)/100.0;
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
				f = f - (f - 0.9) / 100.0;
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


#include "VEDoxygen.h"
#include "VEMemHeap.h"
#include "VEMemVector.h"
#include "VEMemMap.h"

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

#endif

