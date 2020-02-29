/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VHINCLUDE_H
#define VHINCLUDE_H



#include <fstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <array>
#include <set>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <random>
#include <cmath>

#include "VEDefines.h"

namespace vh {

	//--------------------------------------------------------------------------------------------------------------------------------
	///need only for start up
	struct QueueFamilyIndices {
		int graphicsFamily = -1;	///<Index of graphics family
		int presentFamily = -1;		///<Index of present family

		///\returns true if the structure is filled completely
		bool isComplete() {
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};

	///need only for start up
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;			///<Surface capabilities
		std::vector<VkSurfaceFormatKHR> formats;		///<Surface formats available
		std::vector<VkPresentModeKHR> presentModes;		///<Possible present modes
	};


	//--------------------------------------------------------------------------------------------------------------------------------
	//declaration of all helper functions

	//use this macro to check the function result, if its not VK_SUCCESS then return the error
	#define VHCHECKRESULT(x) { \
		VkResult retval = (x); \
		if (retval != VK_SUCCESS) { \
			return retval; \
		} \
	}



	//--------------------------------------------------------------------------------------------------------------------------------
	//debug
	VKAPI_ATTR VkBool32 VKAPI_CALL vhDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);
	VkResult vhDebugCreateReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
	void vhDebugDestroyReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
	void vhSetupDebugCallback(VkInstance instance, VkDebugReportCallbackEXT *callback);

}


#endif