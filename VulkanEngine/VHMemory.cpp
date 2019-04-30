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
	*
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

		return std::numeric_limits<uint32_t>::max();
	}


	/**
	*
	* \brief Initialize the VMA allocator library
	*
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

		return vmaCreateAllocator(&allocatorInfo, &allocator);
	}

	///A block of N entries, which are UBOs that e.g. define world matrices etc.
	struct vhMemoryBlock {
		VkBuffer buffer;				///<handle to GPU buffer
		VmaAllocation allocation;		///<VMA allocation info
		int8_t * pMemory;				///<pointer to the host memory
		uint32_t maxNumEntries;			///<maximum number of entries
		uint32_t sizeEntry;			///<length of one entry
		std::vector<void*> owners;		///<list of pointers to the entry owners
		bool dirty;						///<if dirty this block needs to be updated
	};


	VkResult vhMemoryBlockAdd(VmaAllocator allocator, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage, 
						std::vector<vhMemoryBlock> &blocklist, uint32_t maxNumEntries, uint32_t sizeEntry,
						void* owner, vhMemoryBlock **memBlock, uint32_t *blockIndex  ) {

		if (blocklist.size() == 0 || blocklist[blocklist.size() - 1].owners.size() == maxNumEntries) {
			vhMemoryBlock block = {};

			VHCHECKRESULT( vhBufCreateBuffer( allocator, sizeEntry, usage, vmaUsage, &block.buffer, &block.allocation ) );

			block.pMemory = new int8_t[sizeEntry*maxNumEntries];
			block.maxNumEntries = maxNumEntries;
			block.sizeEntry = sizeEntry;

			blocklist.push_back(block);
		}

		*memBlock = &blocklist[blocklist.size() - 1];				//pointer to last mem block
		*blockIndex = (uint32_t)(*memBlock)->owners.size() - 1;		//index of last entry in block

		return VK_SUCCESS;
	}


	void vhMemoryBlockUpdate(VmaAllocator allocator, std::vector<vhMemoryBlock> &blocklist) {
		for (auto block : blocklist) {
			if (block.dirty) {
				void* data = nullptr;
				vmaMapMemory(allocator, block.allocation, &data);
				memcpy(data, block.pMemory, block.maxNumEntries*block.sizeEntry );
				vmaUnmapMemory(allocator, block.allocation );
			}
		}
	}


	void vhMemoryBlockDelete(VmaAllocator allocator, vhMemoryBlock *pBlock ) {

	}


	void vhMemoryBlockClear(VmaAllocator allocator,  std::vector<vhMemoryBlock> &blocklist ) {
		for (auto block : blocklist) {
			delete block.pMemory;
			vmaDestroyBuffer( allocator, block.buffer, block.allocation);
		}
		blocklist.clear();
	}



}


