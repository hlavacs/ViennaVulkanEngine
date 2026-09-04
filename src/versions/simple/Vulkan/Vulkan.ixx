module;
#include <compare>
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
export import :Memory;
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
	* - Device.ixx owns Vulkan instance, surface, physical-device selection, and logical-device creation.
	* - Memory.ixx owns the VMA allocator, buffers, images, one-time submits, and layout transitions.
	* - Commands.ixx owns Vulkan command pools, command buffers, and frame synchronization.
	* - Presentation.ixx owns the swapchain, its image views, and the shared depth format.
	* - Pipeline.ixx owns descriptor-set layout, vertex input description, pipeline layout, shader modules, graphics pipeline, and push constants.
	* - Shadow.ixx owns shadow-map depth arrays, samplers, and shadow pipelines.
	* - Readback.ixx owns the color/depth image readback and deterministic PNG encoding for captured frames.
	* - Resources.ixx owns texture images, uploaded meshes, frame uniforms, uniform buffers, descriptor pool, and descriptor sets.
	*/
