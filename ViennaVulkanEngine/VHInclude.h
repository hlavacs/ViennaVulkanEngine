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


	struct VhVulkanState {
		VkInstance					m_instance = VK_NULL_HANDLE;

		VkPhysicalDevice			m_physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceFeatures	m_physicalDeviceFeatures;
		VkPhysicalDeviceLimits		m_physicalDeviceLimits;

		VkSurfaceKHR				m_surface = VK_NULL_HANDLE;
		VkSurfaceCapabilitiesKHR	m_surfaceCapabilities;
		VkSurfaceCapabilities2KHR	m_surfaceCapabilities2;

		VkSwapchainKHR				m_swapChain = VK_NULL_HANDLE;
		std::vector<VkImage>		m_swapChainImages;
		std::vector<VkImageView>	m_swapChainImageViews;
		VkFormat					m_swapChainImageFormat;
		VkExtent2D					m_swapChainExtent;

		VkDevice					m_device = VK_NULL_HANDLE;
		std::vector<VkQueue>		m_graphicalQueues;
		std::vector<VkQueue>		m_presentQueues;
		std::vector<VkQueue>		m_computeQueues;
	};


	//--------------------------------------------------------------------------------------------------------------------------------
	///need only for start up
	struct VhQueueFamilyIndices {
		int graphicsFamily = -1;	///<Index of graphics family
		int presentFamily = -1;		///<Index of present family

		///\returns true if the structure is filled completely
		bool isComplete() {
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};

	///need only for start up
	struct VhSwapChainSupportDetails {
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


}


#endif