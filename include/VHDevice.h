#pragma once

#include <vector>
#include <assert.h>
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

namespace vh {

    void CheckResult(VkResult err);

    //instance and device
    bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension);
    auto SelectPhysicalDevice() ;    
    void SetUpInstance(std::vector<const char*> layers, std::vector<const char*> instance_extensions, VkAllocationCallbacks* allocator, VkInstance* instance);
    void SetupDebugReport(VkInstance instance, VkAllocationCallbacks* allocator, VkDebugReportCallbackEXT* debugReport);
    void SetupPhysicalDevice(VkInstance instance, std::vector<const char*> device_extensions, VkPhysicalDevice* physicalDevice);
    void SetupGraphicsQueueFamily( VkPhysicalDevice physicalDevice, uint32_t* queueFamily);
    void SetupDevice(   VkPhysicalDevice physicalDevice, VkAllocationCallbacks* allocator, 
                        std::vector<const char*>& device_extensions, uint32_t queueFamily, VkDevice* device);

    void SetupDescriptorPool(VkDevice device, VkDescriptorPool* descriptorPool);
    

    //surface
    VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkFormat> requestSurfaceImageFormat);
    VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkPresentModeKHR> requestPresentModes);
    void SetupSurface( VkDevice device, VkDescriptorPool descriptorPool, VkSurfaceKHR* surface);

}

