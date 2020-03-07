#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/

namespace vh::deb {

	VKAPI_ATTR VkBool32 VKAPI_CALL vhDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, 
		uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);

	VkResult vhDebugCreateReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, 
		const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);

	void vhDebugDestroyReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, 
		const VkAllocationCallbacks* pAllocator);

	void vhSetupDebugCallback(VkInstance instance, VkDebugReportCallbackEXT* callback);

}


