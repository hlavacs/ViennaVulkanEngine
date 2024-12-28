#pragma once

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
}

#include <stb_image.h>
#include <tiny_obj_loader.h>

#include "Volk/volk.h"

#include "vma/vk_mem_alloc.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

#include "VHVulkan.h"
#include "VHBuffer.h"
#include "VHDevice.h"
