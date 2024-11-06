
#include <iostream>
#include <vector>
#include <set>
#include "VHInclude.h"



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

    void SetupInstance(std::vector<const char*> layers, std::vector<const char*> extensions, VkAllocationCallbacks* allocator, VkInstance* instance) {
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

    void CreateDescriptorPool(VkDevice device, VkDescriptorPool* descriptorPool) {
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


    //--------------------------------------------------------------------------------------






    //--------------------------------------------------------------------------------------


    void DestroyFrame(VkDevice device, ImGui_ImplVulkanH_Frame* fd, const VkAllocationCallbacks* allocator)
    {
        vkDestroyFence(device, fd->Fence, allocator);
        vkFreeCommandBuffers(device, fd->CommandPool, 1, &fd->CommandBuffer);
        vkDestroyCommandPool(device, fd->CommandPool, allocator);
        fd->Fence = VK_NULL_HANDLE;
        fd->CommandBuffer = VK_NULL_HANDLE;
        fd->CommandPool = VK_NULL_HANDLE;

        vkDestroyImageView(device, fd->BackbufferView, allocator);
        vkDestroyFramebuffer(device, fd->Framebuffer, allocator);
    }

    void DestroyFrameSemaphores(VkDevice device, ImGui_ImplVulkanH_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator)
    {
        vkDestroySemaphore(device, fsd->ImageAcquiredSemaphore, allocator);
        vkDestroySemaphore(device, fsd->RenderCompleteSemaphore, allocator);
        fsd->ImageAcquiredSemaphore = fsd->RenderCompleteSemaphore = VK_NULL_HANDLE;
    }


    void CreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, ImGui_ImplVulkanH_Window* wd, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count)
    {
        VkSwapchainKHR old_swapchain = wd->Swapchain;
        wd->Swapchain = VK_NULL_HANDLE;
        vh::CheckResult(vkDeviceWaitIdle(device));

        // We don't use ImGui_ImplVulkanH_DestroyWindow() because we want to preserve the old swapchain to create the new one.
        // Destroy old Framebuffer
        for (uint32_t i = 0; i < wd->ImageCount; i++)
            DestroyFrame(device, &wd->Frames[i], allocator);
        for (uint32_t i = 0; i < wd->SemaphoreCount; i++)
            DestroyFrameSemaphores(device, &wd->FrameSemaphores[i], allocator);

        IM_FREE(wd->Frames);
        IM_FREE(wd->FrameSemaphores);
        wd->Frames = nullptr;
        wd->FrameSemaphores = nullptr;
        wd->ImageCount = 0;
        if (wd->RenderPass)
            vkDestroyRenderPass(device, wd->RenderPass, allocator);
        if (wd->Pipeline)
            vkDestroyPipeline(device, wd->Pipeline, allocator);

        // If min image count was not specified, request different count of images dependent on selected present mode
        if (min_image_count == 0)
            min_image_count = ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(wd->PresentMode);

        // Create Swapchain
        {
            VkSwapchainCreateInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            info.surface = wd->Surface;
            info.minImageCount = min_image_count;
            info.imageFormat = wd->SurfaceFormat.format;
            info.imageColorSpace = wd->SurfaceFormat.colorSpace;
            info.imageArrayLayers = 1;
            info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
            info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            info.presentMode = wd->PresentMode;
            info.clipped = VK_TRUE;
            info.oldSwapchain = old_swapchain;
            VkSurfaceCapabilitiesKHR cap;
            vh::CheckResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, wd->Surface, &cap));
            if (info.minImageCount < cap.minImageCount)
                info.minImageCount = cap.minImageCount;
            else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
                info.minImageCount = cap.maxImageCount;

            if (cap.currentExtent.width == 0xffffffff)
            {
                info.imageExtent.width = wd->Width = w;
                info.imageExtent.height = wd->Height = h;
            }
            else
            {
                info.imageExtent.width = wd->Width = cap.currentExtent.width;
                info.imageExtent.height = wd->Height = cap.currentExtent.height;
            }
            vh::CheckResult(vkCreateSwapchainKHR(device, &info, allocator, &wd->Swapchain));
            vh::CheckResult(vkGetSwapchainImagesKHR(device, wd->Swapchain, &wd->ImageCount, nullptr));
            VkImage backbuffers[16] = {};
            IM_ASSERT(wd->ImageCount >= min_image_count);
            IM_ASSERT(wd->ImageCount < IM_ARRAYSIZE(backbuffers));
            vh::CheckResult(vkGetSwapchainImagesKHR(device, wd->Swapchain, &wd->ImageCount, backbuffers));

            IM_ASSERT(wd->Frames == nullptr && wd->FrameSemaphores == nullptr);
            wd->SemaphoreCount = wd->ImageCount + 1;
            wd->Frames = (ImGui_ImplVulkanH_Frame*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_Frame) * wd->ImageCount);
            wd->FrameSemaphores = (ImGui_ImplVulkanH_FrameSemaphores*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_FrameSemaphores) * wd->SemaphoreCount);
            memset(wd->Frames, 0, sizeof(wd->Frames[0]) * wd->ImageCount);
            memset(wd->FrameSemaphores, 0, sizeof(wd->FrameSemaphores[0]) * wd->SemaphoreCount);
            for (uint32_t i = 0; i < wd->ImageCount; i++)
                wd->Frames[i].Backbuffer = backbuffers[i];
        }
        if (old_swapchain)
            vkDestroySwapchainKHR(device, old_swapchain, allocator);

        // Create the Render Pass
        if (wd->UseDynamicRendering == false)
        {
            VkAttachmentDescription attachment = {};
            attachment.format = wd->SurfaceFormat.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = wd->ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference color_attachment = {};
            color_attachment.attachment = 0;
            color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_attachment;
            VkSubpassDependency dependency = {};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;
            vh::CheckResult(vkCreateRenderPass(device, &info, allocator, &wd->RenderPass));

            // We do not create a pipeline by default as this is also used by examples' main.cpp,
            // but secondary viewport in multi-viewport mode may want to create one with:
            //ImGui_ImplVulkan_CreatePipeline(device, allocator, VK_NULL_HANDLE, wd->RenderPass, VK_SAMPLE_COUNT_1_BIT, &wd->Pipeline, v->Subpass);
        }

        // Create The Image Views
        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = wd->SurfaceFormat.format;
            info.components.r = VK_COMPONENT_SWIZZLE_R;
            info.components.g = VK_COMPONENT_SWIZZLE_G;
            info.components.b = VK_COMPONENT_SWIZZLE_B;
            info.components.a = VK_COMPONENT_SWIZZLE_A;
            VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            info.subresourceRange = image_range;
            for (uint32_t i = 0; i < wd->ImageCount; i++)
            {
                ImGui_ImplVulkanH_Frame* fd = &wd->Frames[i];
                info.image = fd->Backbuffer;
                vh::CheckResult(vkCreateImageView(device, &info, allocator, &fd->BackbufferView));
            }
        }

        // Create Framebuffer
        if (wd->UseDynamicRendering == false)
        {
            VkImageView attachment[1];
            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = wd->RenderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachment;
            info.width = wd->Width;
            info.height = wd->Height;
            info.layers = 1;
            for (uint32_t i = 0; i < wd->ImageCount; i++)
            {
                ImGui_ImplVulkanH_Frame* fd = &wd->Frames[i];
                attachment[0] = fd->BackbufferView;
                vh::CheckResult(vkCreateFramebuffer(device, &info, allocator, &fd->Framebuffer));
            }
        }
    }


    void CreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device, ImGui_ImplVulkanH_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator)
    {
        IM_ASSERT(physical_device != VK_NULL_HANDLE && device != VK_NULL_HANDLE);
        IM_UNUSED(physical_device);

        // Create Command Buffers
        for (uint32_t i = 0; i < wd->ImageCount; i++)
        {
            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[i];
            {
                VkCommandPoolCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                info.flags = 0;
                info.queueFamilyIndex = queue_family;
                vh::CheckResult(vkCreateCommandPool(device, &info, allocator, &fd->CommandPool));
            }
            {
                VkCommandBufferAllocateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                info.commandPool = fd->CommandPool;
                info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                info.commandBufferCount = 1;
                vh::CheckResult(vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer));
            }
            {
                VkFenceCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                vh::CheckResult(vkCreateFence(device, &info, allocator, &fd->Fence));
            }
        }

        for (uint32_t i = 0; i < wd->SemaphoreCount; i++)
        {
            ImGui_ImplVulkanH_FrameSemaphores* fsd = &wd->FrameSemaphores[i];
            {
                VkSemaphoreCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                vh::CheckResult(vkCreateSemaphore(device, &info, allocator, &fsd->ImageAcquiredSemaphore));
                vh::CheckResult(vkCreateSemaphore(device, &info, allocator, &fsd->RenderCompleteSemaphore));
            }
        }
    }




}; // namespace vh


