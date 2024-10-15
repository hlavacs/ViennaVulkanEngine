#pragma once


int imgui_SDL2( VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, 
        VkQueue queue, uint32_t queueFamily, VkSurfaceKHR surface, VkAllocationCallbacks* allocator);
