#pragma once

#include <iostream>
#include <iomanip>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


#if (defined(VVE_SINGLE_PRECISION) && defined(VVE_DOUBLE_PRECISION))
	#error "Both VVE_SINGLE_PRECISION and VVE_DOUBLE_PRECISION are defined!"
#endif

#if !(defined(VVE_SINGLE_PRECISION) || defined(VVE_DOUBLE_PRECISION))
	#define VVE_SINGLE_PRECISION
#endif

namespace vve {
	#ifdef VVE_SINGLE_PRECISION
		using real_t = float;
		#define vec2_t glm::vec2
		#define vec3_t glm::vec3
		#define vec4_t glm::vec4
		#define quat_t glm::quat
		#define mat3_t glm::mat3
		#define mat4_t glm::mat4
		#define mat43_t glm::mat4x3
	#else //VVE_DOUBLE_PRECISION
		using real_t = double;
		#define vec2_t glm::dvec2
		#define vec3_t glm::dvec3
		#define vec4_t glm::dvec4
		#define quat_t glm::dquat
		#define mat3_t glm::dmat3
		#define mat4_t glm::dmat4
		#define mat43_t glm::dmat4x3
	#endif

	inline std::ostream& operator<<(std::ostream& os, const vec3_t& vec) {
    	return os << std::setw(6) << vec[0] << " " << vec[1] << " " << vec[2] << " " << std::endl; 
	}

	inline std::ostream& operator<<(std::ostream& os, const mat3_t& mat) {
    	return os << mat[0] << mat[1]<< mat[2]<< std::endl; 
	}

	inline std::ostream& operator<<(std::ostream& os, const vec4_t& vec) {
    	return os << std::setw(6) << vec[0] << " " << vec[1] << " " << vec[2] << " " << vec[3] << std::endl; 
	}

	inline std::ostream& operator<<(std::ostream& os, const mat4_t& mat) {
    	return os << mat[0]<< mat[1]<< mat[2]<< mat[3] << std::endl; 
	}
}

#include <stb_image.h>

#include "Volk/volk.h"

#include "vma/vk_mem_alloc.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "VHVulkan.h"
#include "VHBuffer.h"
#include "VHDevice.h"
#include "VHCommand.h"
#include "VHRender.h"
#include "VHSync.h"
