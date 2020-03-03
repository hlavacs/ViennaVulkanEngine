#pragma once


//use this macro to check the function result, if its not VK_SUCCESS then return the error
#define VECHECKRESULT(x) { \
		VkResult retval = (x); \
		assert (retval == VK_SUCCESS); \
	}

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"


namespace vve::sysvul {

	void init();
	void tick();
	void sync();
	void close();

	void windowSizeChanged();

}

