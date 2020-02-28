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


	struct VeVulkanState {
		VkInstance m_instance;
		VkPhysicalDevice m_physicalDevice;
		VkDevice m_device;

		std::vector<VkQueue> m_graphics_queues;
		std::vector<VkQueue> m_present_queues;
		std::vector<VkQueue> m_compute_queues;

		VeVulkanState() : m_instance(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE), m_device(VK_NULL_HANDLE),
			m_graphics_queues(), m_present_queues(), m_compute_queues() {};
	};


	void init();
	void tick();
	void sync();
	void close();


}

