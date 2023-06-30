//https://gist.github.com/YukiSnowy
//https://gist.github.com/YukiSnowy/dc31f47448ac61dd6aedee18b5d53858


#include "vector"
#include "set"
#include "algorithm"
#include "iostream"
#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_vulkan.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vma/vk_mem_alloc.h"

SDL_Window* window;
const char* window_name{ "Minimal Vulkan" };
VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger; 
VkSurfaceKHR surface;
VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
VkPhysicalDeviceFeatures deviceFeatures;
int graphics_QueueFamilyIndex{ -1 };
int present_QueueFamilyIndex{ -1 };
VkDevice device;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSurfaceCapabilitiesKHR surfaceCapabilities;
VkSurfaceFormatKHR surfaceFormat;
VkExtent2D swapchainSize;
uint32_t swapchainImageCount;
VkSwapchainKHR swapchain;
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
VmaAllocator vmaAllocator;
VkFormat depthFormat;
VkImage depthImage;
VkImageView depthImageView;
VmaAllocation depthImageAllocation;
VmaAllocationInfo depthImageAllocationInfo;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

auto InitVulkan() {
    //instance
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());
    extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    
    VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = window_name, .applicationVersion = VK_MAKE_VERSION(1, 0, 0), .apiVersion = VK_API_VERSION_1_3     };    
    VkInstanceCreateInfo instanceCreateInfo{ 
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
        , .pApplicationInfo = &appInfo
        , .enabledLayerCount = (uint32_t)validationLayers.size()
        , .ppEnabledLayerNames = validationLayers.data()
        , .enabledExtensionCount = (uint32_t)extensionNames.size()
        , .ppEnabledExtensionNames = extensionNames.data() 
    };
    vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    //debug
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
        , .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        , .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        , .pfnUserCallback = debugCallback };
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) { func(instance, &debugCreateInfo, nullptr, &debugMessenger); }
    else { throw std::runtime_error("Cannot create Debug Messenger!\n"); };

    //surface
    SDL_Vulkan_CreateSurface(window, instance, &surface);

    //physical device
    std::vector<VkPhysicalDevice> physicalDevices;
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    physicalDevices.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    for (const auto& pd : physicalDevices) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pd, &deviceProperties);
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { physicalDevice = pd; break; }
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) { physicalDevice = pd; }
    }
    if (physicalDevice == VK_NULL_HANDLE) { throw std::runtime_error("Error: No GPU found!\n"); }
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    //queue families
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilyProperties) {
        if (graphics_QueueFamilyIndex == -1 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_QueueFamilyIndex = i;
        if (present_QueueFamilyIndex == -1) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) present_QueueFamilyIndex = i;
        }
        if (graphics_QueueFamilyIndex != -1 && present_QueueFamilyIndex != -1) break;
        i++;
    }

    //device and queues
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE_4_EXTENSION_NAME };
    const float queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, graphics_QueueFamilyIndex, 1, &queue_priority);
    if (graphics_QueueFamilyIndex != present_QueueFamilyIndex)
        queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, present_QueueFamilyIndex, 1, &queue_priority);

    VkDeviceCreateInfo deviceCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = nullptr
        , .queueCreateInfoCount = (uint32_t)queueCreateInfos.size(), .pQueueCreateInfos = queueCreateInfos.data()
        , .enabledLayerCount = 0, .ppEnabledLayerNames = nullptr
        , .enabledExtensionCount = (uint32_t)deviceExtensions.size(), .ppEnabledExtensionNames = deviceExtensions.data()
        , .pEnabledFeatures = &deviceFeatures
    };
    vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    vkGetDeviceQueue(device, graphics_QueueFamilyIndex, 0, &graphicsQueue);
    if (graphics_QueueFamilyIndex != present_QueueFamilyIndex) vkGetDeviceQueue(device, present_QueueFamilyIndex, 0, &presentQueue);
    else presentQueue = graphicsQueue;

    //swapchain
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    uint32_t surfaceFormatsCount{ 0 };
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, nullptr);
    surfaceFormats.resize(surfaceFormatsCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());
    auto pformat = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [](const VkSurfaceFormatKHR& sf) { return sf.format == VK_FORMAT_B8G8R8A8_UNORM; });
    if (pformat == surfaceFormats.end()) { throw std::runtime_error("Error: Surface does not support format VK_FORMAT_B8G8R8A8_UNORM!\n"); }
    else surfaceFormat = *pformat;

    int width, height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    swapchainSize.width = std::clamp(width, (int)surfaceCapabilities.minImageExtent.width, (int)surfaceCapabilities.maxImageExtent.width);
    swapchainSize.height = std::clamp(height, (int)surfaceCapabilities.minImageExtent.height, (int)surfaceCapabilities.maxImageExtent.height);
    swapchainImageCount = surfaceCapabilities.maxImageCount > 0 ? std::min(surfaceCapabilities.minImageCount + 1, surfaceCapabilities.maxImageCount) : surfaceCapabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR scCreateInfo{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = surface
        , .minImageCount = swapchainImageCount, .imageFormat = surfaceFormat.format, .imageColorSpace = surfaceFormat.colorSpace
        , .imageExtent = swapchainSize, .imageArrayLayers = 1, .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        , .preTransform = surfaceCapabilities.currentTransform, .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        , .presentMode = VK_PRESENT_MODE_FIFO_KHR, .clipped = VK_TRUE
    };

    uint32_t queueFamilyIndices[] = { (uint32_t)graphics_QueueFamilyIndex, (uint32_t)present_QueueFamilyIndex };
    if (graphics_QueueFamilyIndex != present_QueueFamilyIndex) {
        scCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        scCreateInfo.queueFamilyIndexCount = 2;
        scCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else { scCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
    vkCreateSwapchainKHR(device, &scCreateInfo, nullptr, &swapchain);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    //swap chain image views
    auto createImageView = [](VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) { throw std::runtime_error("failed to create texture image view!"); }
        return imageView;
    };
    for (const auto& image : swapchainImages) swapchainImageViews.push_back(createImageView(image, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT));

    //VMA
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    //vulkanFunctions.vkGetDeviceBufferMemoryRequirements = &vkGetDeviceBufferMemoryRequirements;
    //vulkanFunctions.vkGetDeviceImageMemoryRequirements = &vkGetDeviceImageMemoryRequirements;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
    vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator);

    //depth map
    std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
    for (auto& format : depthFormats) {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { depthFormat = format; break; }
    }

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = swapchainSize.width;
    imageCreateInfo.extent.height = swapchainSize.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = depthFormat;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    vmaCreateImage(vmaAllocator, &imageCreateInfo, &allocCreateInfo, &depthImage, &depthImageAllocation, &depthImageAllocationInfo);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);




}


void DestroyVulkan() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vmaDestroyImage(vmaAllocator, depthImage, depthImageAllocation);
    for (const auto& imageView : swapchainImageViews) vkDestroyImageView(device, imageView, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr );
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) { func(instance, debugMessenger, nullptr); }
    vkDestroyInstance(instance, nullptr);
}

auto AcquireNextImage() {

}

auto RecordCommandBuffer() {

}

auto QueueSubmit() {
}

auto QueuePresent() {

}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_EVERYTHING);
    window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

    InitVulkan();

    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) { if (event.type == SDL_QUIT) running = false; }
        AcquireNextImage();
        RecordCommandBuffer();
        QueueSubmit();
        QueuePresent();
    }

    DestroyVulkan();

    SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
    return 0;
}
