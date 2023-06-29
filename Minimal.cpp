//https://gist.github.com/YukiSnowy

#include "vector"
#include "set"
#include "vulkan/vulkan.h"
#include "glm.hpp"
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_vulkan.h"

SDL_Window* window;
const char* window_name{ "Minimal Vulkan" };
VkInstance instance;
PFN_vkCreateDebugReportCallbackEXT SDL2_vkCreateDebugReportCallbackEXT = nullptr;
VkDebugReportCallbackEXT debugCallback;
VkSurfaceKHR surface;
VkPhysicalDevice physical_device{ VK_NULL_HANDLE };
int graphics_QueueFamilyIndex{ -1 };
int present_QueueFamilyIndex{ -1 };
VkDevice device;
VkQueue graphicsQueue;
VkQueue presentQueue;

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanReportFunc(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location,
    int32_t code, const char* layerPrefix, const char* msg, void* userData) {
    printf("VULKAN VALIDATION: %s\n", msg);
    return VK_FALSE;
}

auto InitVulkan() {
    //instance
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensionNames(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());
    VkApplicationInfo appInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = window_name, .applicationVersion = VK_MAKE_VERSION(1, 0, 0) };
    VkInstanceCreateInfo instanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo, .enabledExtensionCount = (uint32_t)extensionNames.size(), .ppEnabledExtensionNames = extensionNames.data() };
    vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    //debug
    SDL2_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)SDL_Vulkan_GetVkGetInstanceProcAddr();
    VkDebugReportCallbackCreateInfoEXT debugCallbackCreateInfo = {};
    debugCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debugCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debugCallbackCreateInfo.pfnCallback = VulkanReportFunc;
    SDL2_vkCreateDebugReportCallbackEXT(instance, &debugCallbackCreateInfo, 0, &debugCallback);

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
    if (physical_device == VK_NULL_HANDLE) { printf("Error: No GPU found!\n");  exit(1); }

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
}

void DestroyVulkan() {

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
