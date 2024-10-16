#pragma once

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>


void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);

//int imgui_SDL2( VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue
//        , uint32_t queueFamily, VkSurfaceKHR surface, VkDescriptorPool pool
//        , VkAllocationCallbacks* allocator, SDL_Window* window, ImGui_ImplVulkanH_Window* mainWindowData
//        , ImGuiIO* io);

void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);

void FramePresent(ImGui_ImplVulkanH_Window* wd);
