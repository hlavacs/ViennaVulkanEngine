#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/


//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VECHECKRESULT(x) { \
		VkResult retval = (x); \
		assert (retval == VK_SUCCESS); \
	}

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"


namespace vve::sysvul {

	inline const std::string VE_SYSTEM_NAME = "VE SYSTEM VULKAN";
	inline VeHandle VE_SYSTEM_HANDLE = VE_NULL_HANDLE;

	void init();
	void update(sysmes::VeMessageTableEntry e);
	void close(sysmes::VeMessageTableEntry e);

	void windowSizeChanged();

}

