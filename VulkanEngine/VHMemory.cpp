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


	//-------------------------------------------------------------------------------------------------------
	//memory blocks

	/**
	*
	* \brief Add an entry to a memory block
	*
	* \param[in] allocator The VMA allocator
	* \param[in] usage VkBuffer usage flags
	* \param[in] vmaUsage VMA usage flags
	* \param[in] blocklist The memory block list
	* \param[in] maxNumEntries Maximum number of entries in a memory block
	* \param[in] sizeEntry Size of an entry in bytes
	* \param[in] numBuffers Number of buffers (one for each framebuffer)
	* \param[in] owner Pointer to the entry owner
	* \param[in] handle A handle to the entry
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockAdd(	VmaAllocator allocator, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
							std::vector<vhMemoryBlock> &blocklist, uint32_t maxNumEntries, 
							uint32_t sizeEntry, uint32_t numBuffers,
							void* owner, vhMemoryHandle *handle) {

		vhMemoryBlock *pMemBlock = nullptr;

		for (uint32_t i = 0; i < blocklist.size(); i++ ) {			//find a memory block with a free entry
			if ( blocklist[i].handles.size() < maxNumEntries) {
				pMemBlock = &blocklist[i];
				break;
			}
		}

		if ( pMemBlock==nullptr ) {			//none found - create one
			vhMemoryBlock block = {};
			block.buffers.resize(numBuffers);
			block.allocations.resize(numBuffers);
			block.descriptorSets.resize(numBuffers);

			for (uint32_t i = 0; i < numBuffers; i++) {
				VHCHECKRESULT(vhBufCreateBuffer(allocator, sizeEntry*maxNumEntries, usage, vmaUsage, &block.buffers[i], &block.allocations[i]));
			}

			block.pMemory = new int8_t[sizeEntry*maxNumEntries];
			block.maxNumEntries = maxNumEntries;
			block.sizeEntry = sizeEntry;
			block.dirty = false;

			blocklist.push_back(block);

			pMemBlock = &blocklist[blocklist.size() - 1];							//use the new mem block
		}

		handle->owner = owner;														//need to know this for drawing
		handle->pMemBlock = pMemBlock;												//pointer to last mem block
		handle->pMemBlock->handles.push_back(handle);								//add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(handle->pMemBlock->handles.size() - 1);		//index of last entry in block

		return VK_SUCCESS;
	}


	/**
	*
	* \brief Update an entry in one of the memory blocks
	*
	* \param[in] pHandle Pointer to the handle of the entry
	* \param[in] data Pointer to a block of data that should be written over the entry
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockUpdateEntry( vhMemoryHandle *pHandle, void *data ) {
		void *pEntry = pHandle->pMemBlock->pMemory + pHandle->pMemBlock->sizeEntry * pHandle->entryIndex;
		memcpy(pEntry, data, pHandle->pMemBlock->sizeEntry );
		pHandle->pMemBlock->dirty = true;
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Update all memory blocks by copying them to the GPU
	*
	* \param[in] allocator The VMA allocator
	* \param[in] blocklist The memory blocks
	* \param[in] index The index of the buffer to use
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockUpdateMem(VmaAllocator allocator, std::vector<vhMemoryBlock> &blocklist, uint32_t index) {
		for (auto block : blocklist) {
			if (block.dirty) {
				void* data = nullptr;
				vmaMapMemory(allocator, block.allocations[index], &data);
				memcpy(data, block.pMemory, block.maxNumEntries*block.sizeEntry );
				vmaUnmapMemory(allocator, block.allocations[index] );
				block.dirty = false;
			}
		}
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Delete an entry from a memory block
	*
	* \param[in] allocator The VMA allocator
	* \param[in] blocklist The memory blocks
	* \param[in] pHandle Pointer to the handle of the entry that should be deleted
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockDeleteEntry(VmaAllocator allocator, std::vector<vhMemoryBlock> &blocklist, vhMemoryHandle *pHandle) {

		vhMemoryBlock *pMemBlock = pHandle->pMemBlock;				//to keep code readable

		void *pEntryDel	 = pMemBlock->pMemory + pMemBlock->sizeEntry * pHandle->entryIndex;				//entry to be deleted
		void *pEntryLast = pMemBlock->pMemory + pMemBlock->sizeEntry * (pMemBlock->handles.size()-1);	//last entry in the block

		memcpy(pEntryDel, pEntryLast, pMemBlock->sizeEntry);		//copy last entry over the deleted entry

		vhMemoryHandle *pHandleLast = pMemBlock->handles[pMemBlock->handles.size() - 1];
		pMemBlock->handles[pHandle->entryIndex] = pHandleLast;		//write over old handle
		pHandleLast->entryIndex = pHandle->entryIndex;				//update also handle in object

		pMemBlock->handles.pop_back();								//remove last handle, could also be the deleted one
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Delete all entries and all blocks
	*
	* \param[in] allocator The VMA allocator
	* \param[in] blocklist The memory blocks
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockClear(VmaAllocator allocator,  std::vector<vhMemoryBlock> &blocklist ) {
		for (auto block : blocklist) {
			delete[] block.pMemory;
			for (uint32_t i = 0; i < block.buffers.size(); i++) {
				vmaDestroyBuffer(allocator, block.buffers[i], block.allocations[i]);
			}
		}
		blocklist.clear();
		return VK_SUCCESS;
	}



}


