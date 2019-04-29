/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VHHelper.h"

namespace vh {

	//-------------------------------------------------------------------------------------------------------
	//

	/**
	*
	* \brief Find the right memory type
	* \param[in] physicalDevice Physical Vulkan device
	* \param[in] typeFilter Memory type
	* \param[in] properties Desired memory properties
	* \returns index of memory heap that fulfills the properties
	*
	*/
	uint32_t vhMemFindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}


	/**
	*
	* \brief Initialize the VMA allocator library
	* \param[in] physicalDevice Physical Vulkan device
	* \param[in] device Logical device
	* \param[out] allocator The created VMA allocator
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemCreateVMAAllocator( VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator &allocator) {
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = physicalDevice;
		allocatorInfo.device = device;

		/*if (VK_KHR_dedicated_allocation_enabled)
		{
			allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
		}

		VkAllocationCallbacks cpuAllocationCallbacks = {};
		if (USE_CUSTOM_CPU_ALLOCATION_CALLBACKS)
		{
			cpuAllocationCallbacks.pUserData = CUSTOM_CPU_ALLOCATION_CALLBACK_USER_DATA;
			cpuAllocationCallbacks.pfnAllocation = &CustomCpuAllocation;
			cpuAllocationCallbacks.pfnReallocation = &CustomCpuReallocation;
			cpuAllocationCallbacks.pfnFree = &CustomCpuFree;
			allocatorInfo.pAllocationCallbacks = &cpuAllocationCallbacks;
		}

		// Uncomment to enable recording to CSV file.
		/*
		{
		VmaRecordSettings recordSettings = {};
		recordSettings.pFilePath = "VulkanSample.csv";
		allocatorInfo.pRecordSettings = &recordSettings;
		}
		*/

		return vmaCreateAllocator(&allocatorInfo, &allocator);
	}

}


