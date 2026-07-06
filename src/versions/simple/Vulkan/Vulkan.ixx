module;
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.Simple.Vulkan;
export import :OwnedHandle;
export import :Device;
export import :Commands;
export import :Presentation;
export import :Pipeline;
export import :Shadow;
export import :Readback;
export import :Resources;
import std;

/**
	* @file
	* @brief Primary module interface for VEEngine.Simple.Vulkan; this file only re-exports the Vulkan partitions.
	*
	* Functional objects:
	* - Handle.ixx owns the transitional vk::raii-to-raw Vulkan handle adapter shared by partitions.
	* - Device.ixx owns Vulkan instance, surface, physical-device selection, logical-device creation, and memory-type selection helpers.
	* - Commands.ixx owns Vulkan command pools, command buffers, frame synchronization, and generic buffer allocation.
	* - Presentation.ixx owns swapchain presentation images, image views, depth attachments, render pass, and framebuffers.
	* - Pipeline.ixx owns descriptor-set layout, vertex input description, pipeline layout, shader modules, graphics pipeline, and push constants.
	* - Shadow.ixx owns shadow-map depth images, array views, samplers, framebuffers, and the reserved shadow pipeline state.
	* - Readback.ixx owns color/depth image readback buffers and deterministic PNG encoding for captured frames.
	* - Resources.ixx owns texture images, uploaded meshes, frame uniforms, uniform buffers, descriptor pool, and descriptor sets.
	*/
