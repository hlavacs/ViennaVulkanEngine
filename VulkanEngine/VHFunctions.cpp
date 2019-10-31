
#if defined( _WIN32 )
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <iostream>

#define VK_NO_PROTOTYPES
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#undef VK_EXPORTED_FUNCTION
#undef VK_GLOBAL_LEVEL_FUNCTION
#undef VK_INSTANCE_LEVEL_FUNCTION
#undef VK_DEVICE_LEVEL_FUNCTION

#define VK_EXPORTED_FUNCTION( fun ) PFN_##fun fun;
#define VK_GLOBAL_LEVEL_FUNCTION( fun ) PFN_##fun fun;
#define VK_INSTANCE_LEVEL_FUNCTION( fun ) PFN_##fun fun;
#define VK_DEVICE_LEVEL_FUNCTION( fun ) PFN_##fun fun;

#include "VHFunctions.inl"

#undef VK_EXPORTED_FUNCTION
#undef VK_GLOBAL_LEVEL_FUNCTION
#undef VK_INSTANCE_LEVEL_FUNCTION
#undef VK_DEVICE_LEVEL_FUNCTION

#include <dlfcn.h>
void *VulkanLibrary;


VkResult vhLoadVulkanLibrary() {

#if defined(VK_USE_PLATFORM_WIN32_KHR)
 
    HMODULE VulkanLibrary;

	VulkanLibrary = LoadLibrary((LPCWSTR)L"vulkan-1.dll");
#else
	//VulkanLibrary = dlopen("libvulkan.so.1", RTLD_NOW);
    VulkanLibrary = dlopen("libvulkan.1.dylib", RTLD_LOCAL);
#endif

	if (VulkanLibrary == nullptr) {
		std::cout << "Could not load Vulkan library!" << std::endl;
		return VK_INCOMPLETE;
	}
	return VK_SUCCESS;
}


VkResult vhLoadExportedEntryPoints() {

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#define LoadProcAddress GetProcAddress
#else
	#define LoadProcAddress dlsym
#endif

#define VK_EXPORTED_FUNCTION( fun )                                                   \
    if( !(fun = (PFN_##fun)LoadProcAddress( VulkanLibrary, #fun )) ) {                \
      std::cout << "Could not load exported function: " << #fun << "!" << std::endl;  \
      return VK_INCOMPLETE;                                                                   \
    }

#include "VHFunctions.inl"

	return VK_SUCCESS;
}



VkResult vhLoadGlobalLevelEntryPoints() {

#define VK_GLOBAL_LEVEL_FUNCTION( fun )                                                   \
    if( !(fun = (PFN_##fun)vkGetInstanceProcAddr( nullptr, #fun )) ) {                    \
      std::cout << "Could not load global level function: " << #fun << "!" << std::endl;  \
      return VK_INCOMPLETE;                                                                       \
    }

#include "VHFunctions.inl"

	return VK_SUCCESS;
}


VkResult vhLoadInstanceLevelEntryPoints( VkInstance instance ) {
#define VK_INSTANCE_LEVEL_FUNCTION( fun )                                                   \
    if( !(fun = (PFN_##fun)vkGetInstanceProcAddr( instance, #fun )) ) {              \
      std::cout << "Could not load instance level function: " << #fun << "!" << std::endl;  \
      return VK_INCOMPLETE;                                                                         \
    }

#include "VHFunctions.inl"

	return VK_SUCCESS;
}

    /*if( !(fun = (PFN_##fun)vkGetDeviceProcAddr( device, #fun )) ) {                \  */

VkResult vhLoadDeviceLevelEntryPoints(VkInstance instance, VkDevice device ) {
#define VK_DEVICE_LEVEL_FUNCTION( fun )                                                   \
    if( !(fun = (PFN_##fun)vkGetInstanceProcAddr( instance, #fun )) ) {              \
      std::cout << "Could not load device level function: " << #fun << "!" << std::endl;  \
      return VK_INCOMPLETE;                                                                       \
    }

#include "VHFunctions.inl"

	return VK_SUCCESS;
}


