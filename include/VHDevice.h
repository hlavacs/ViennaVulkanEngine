#pragma once

#include <vector>
#include <assert.h>
#include <vulkan/vulkan.h>

namespace vh {

    void check_vk_result(VkResult err);
    bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension);
    auto SelectPhysicalDevice() -> VkPhysicalDevice;
    void SelectGraphicsQueueFamily();
   
    void SetUpInstance(std::vector<const char*> instance_extensions, VkAllocationCallbacks* allocator, VkInstance* instance);
    void SetupDebugReport();
    void SetupPhysicalDevice();
    void SetupDevice(std::vector<const char*> device_extensions);
    void SetupVulkan();
    void SetupSurface(VkSurfaceKHR surface, int width, int height);

    void CleanupVulkan();


}

