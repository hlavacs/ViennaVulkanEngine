
#include <iostream>
#include <vector>
#include <set>
#include "VHDevice.h"



namespace vh {

    void CheckResult(VkResult err) {
        if (err == 0)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }


    bool IsExtensionAvailable(const std::vector<VkExtensionProperties>& properties, const char* extension) {
        for (const VkExtensionProperties& p : properties)
            if (strcmp(p.extensionName, extension) == 0)
                return true;
        return false;
    }


    void SetUpInstance(std::vector<const char*> layers, std::vector<const char*> extensions, VkAllocationCallbacks* allocator, VkInstance* instance) {
        #ifdef IMGUI_IMPL_VULKAN_USE_VOLK
            volkInitialize();
        #endif

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        std::vector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vh::CheckResult( vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.data()) );

        // Enable required extensions
        if (vh::IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        if (vh::IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        // Enabling validation layers
        if(layers.size()>0) {
            create_info.enabledLayerCount = (uint32_t)layers.size();
            create_info.ppEnabledLayerNames = layers.data();
        }

        if(extensions.size()>0) {
            create_info.enabledExtensionCount = (uint32_t)extensions.size();
            create_info.ppEnabledExtensionNames = extensions.data();
        } 
        vh::CheckResult( vkCreateInstance(&create_info, allocator, instance) );

        #ifdef IMGUI_IMPL_VULKAN_USE_VOLK
            volkLoadInstance(g_Instance);
        #endif
    }


    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
        (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
        fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
        return VK_FALSE;
    }


    void SetupDebugReport( VkInstance instance, VkAllocationCallbacks* allocator, VkDebugReportCallbackEXT* debugReport) {
            auto PFN_CreateDebugReportCallbackEXT_ptr = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
            if(PFN_CreateDebugReportCallbackEXT_ptr == nullptr) return;
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_report;
            debug_report_ci.pUserData = nullptr;
            vh::CheckResult( PFN_CreateDebugReportCallbackEXT_ptr(instance, &debug_report_ci, allocator, debugReport) );
    }


    void SetupPhysicalDevice(VkInstance instance, std::vector<const char*> device_extensions, VkPhysicalDevice* physicalDevice) {
        uint32_t gpu_count;
        vh::CheckResult( vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr) );
        if(gpu_count == 0) return;
        std::vector<VkPhysicalDevice> gpus;
        gpus.resize(gpu_count);
        vh::CheckResult( vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data()) );

        for (VkPhysicalDevice& device : gpus) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                *physicalDevice = device;
                return;
            }
        }
        if (gpu_count > 0) *physicalDevice = gpus[0]; // Use first GPU (Integrated) if a Discrete one is not available.
        return;
    }


    // Select graphics queue family
    void SetupGraphicsQueueFamily( VkPhysicalDevice physicalDevice, uint32_t* queueFamily) {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queues);
        for (uint32_t i = 0; i < count; i++)
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                *queueFamily = i;
                break;
            }
        free(queues);
        assert(*queueFamily != (uint32_t)-1);
    }


    void SetupDevice(VkPhysicalDevice physicalDevice, VkAllocationCallbacks* allocator, 
                    std::vector<const char*>& device_extensions, uint32_t queueFamily, 
                    VkDevice* device ) {

        // Enumerate physical device extension
        uint32_t properties_count;
        std::vector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &properties_count, properties.data());

    #ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    #endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        if( device_extensions.size() > 0 ) {
            create_info.enabledExtensionCount = (uint32_t)device_extensions.size();
            create_info.ppEnabledExtensionNames = device_extensions.data();
        }
        vh::CheckResult( vkCreateDevice(physicalDevice, &create_info, allocator, device) );
    }


    void SetupDescriptorPool(VkDevice device, VkDescriptorPool* descriptorPool) {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        vh::CheckResult( vkCreateDescriptorPool(device, &pool_info, nullptr, descriptorPool) );
    }


    VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkFormat> requestSurfaceImageFormat) {
        // Get the list of supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        for (const auto& format : requestSurfaceImageFormat) {
            if( std::find_if(formats.begin(), formats.end(), [&](VkSurfaceFormatKHR& f){ return format == f.format; } ) != formats.end() ) {
                return { format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
            }
        }
        // If no suitable format is found, fall back to guaranteed supported format
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }


    VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::vector<VkPresentModeKHR> requestPresentModes) {
        // Get the list of supported present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
      
        // Prioritize present modes with the following characteristics:
        // 1. Mailbox mode (for lowest latency and best performance)
        // 2. FIFO mode (for reliability and compatibility)
        VkPresentModeKHR bestPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& presentMode : requestPresentModes) {
            if( std::find_if(presentModes.begin(), presentModes.end(), [&](VkPresentModeKHR& p){ return presentMode == p; } ) != presentModes.end() ) {
                return presentMode;
            }
        }
      
        return VK_PRESENT_MODE_FIFO_KHR;
    }


    void SetupSurface( VkDevice device, VkDescriptorPool descriptorPool, VkSurfaceKHR* surface)
    {
        /*
        wd->Surface = surface;

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
        if (res != VK_TRUE)
        {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }

        // Select Surface Format
        const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

        // Select Present Mode
    #ifdef APP_UNLIMITED_FRAME_RATE
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    #else
        VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    #endif
        wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
        //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

        // Create SwapChain, RenderPass, Framebuffer, etc.
        IM_ASSERT(g_MinImageCount >= 2);
        ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);

    */
    }

}; // namespace vh


