//https://gist.github.com/YukiSnowy
//https://gist.github.com/YukiSnowy/dc31f47448ac61dd6aedee18b5d53858


#include "vector"
#include "set"
#include "algorithm"
#include "iostream"
#include "vulkan/vulkan.h"
#include "glm.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_vulkan.h"

SDL_Window* window;
const char* window_name{ "Minimal Vulkan" };
VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger; 
VkSurfaceKHR surface;
VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
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
    VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = window_name, .applicationVersion = VK_MAKE_VERSION(1, 0, 0) };
    VkInstanceCreateInfo instanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo, .enabledLayerCount = (uint32_t)validationLayers.size(), .ppEnabledLayerNames = validationLayers.data(), .enabledExtensionCount = (uint32_t)extensionNames.size(), .ppEnabledExtensionNames = extensionNames.data()};
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
        vkGetPhysicalDeviceProperties( pd, &deviceProperties);
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { physical_device = pd; break; }
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) { physical_device = pd; }
    }
    if (physical_device == VK_NULL_HANDLE) { throw std::runtime_error("Error: No GPU found!\n"); }

    //queue families
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
    queueFamilyProperties.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilyProperties.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilyProperties) {
        if (graphics_QueueFamilyIndex == -1 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics_QueueFamilyIndex = i;
        if (present_QueueFamilyIndex == -1) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) present_QueueFamilyIndex = i;
        }
        if (graphics_QueueFamilyIndex != -1 && present_QueueFamilyIndex != -1) break;
        i++;
    }

    //device and queues
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    const float queue_priority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, graphics_QueueFamilyIndex, 1, &queue_priority);
    if(graphics_QueueFamilyIndex != present_QueueFamilyIndex)
        queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, present_QueueFamilyIndex, 1, &queue_priority);

    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo deviceCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = nullptr
        , .queueCreateInfoCount = (uint32_t)queueCreateInfos.size(), .pQueueCreateInfos = queueCreateInfos.data()
        , .enabledLayerCount = 0, .ppEnabledLayerNames = nullptr
        , .enabledExtensionCount = (uint32_t)deviceExtensions.size(), .ppEnabledExtensionNames = deviceExtensions.data()
        , .pEnabledFeatures = &deviceFeatures
    };
    vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device);
    vkGetDeviceQueue(device, graphics_QueueFamilyIndex, 0, &graphicsQueue);
    if (graphics_QueueFamilyIndex != present_QueueFamilyIndex) vkGetDeviceQueue(device, present_QueueFamilyIndex, 0, &presentQueue);
    else presentQueue = graphicsQueue;

    //swapchain
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCapabilities);
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    uint32_t surfaceFormatsCount{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surfaceFormatsCount, nullptr);
    surfaceFormats.resize(surfaceFormatsCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surfaceFormatsCount, surfaceFormats.data());
    auto pformat = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [](const VkSurfaceFormatKHR& sf) { return sf.format == VK_FORMAT_B8G8R8A8_UNORM; } );
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
    } else { scCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
    vkCreateSwapchainKHR(device, &scCreateInfo, nullptr, &swapchain);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    //image views
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
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    };

    for (const auto& image : swapchainImages) swapchainImageViews.push_back( createImageView(image, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT) );


}

void DestroyVulkan() {
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
