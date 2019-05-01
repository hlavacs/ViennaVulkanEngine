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
	* \brief Initialize a list of memory blocks
	*
	* \param[in] allocator The VMA allocator
	* \param[in] usage VkBuffer usage flags
	* \param[in] vmaUsage VMA usage flags
	* \param[in] blocklist A reference to the memory block list
	* \param[in] maxNumEntries Maximum number of entries in a memory block
	* \param[in] sizeEntry Size of an entry in bytes
	* \param[in] numBuffers Number of buffers (one for each framebuffer)
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockListInit(VmaAllocator allocator, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
								std::vector<vhMemoryBlock> &blocklist, uint32_t maxNumEntries,
								uint32_t sizeEntry, uint32_t numBuffers ) {
		if (blocklist.empty()) {
			vhMemoryBlock block = {};
			VHCHECKRESULT( vhMemBlockInit(allocator, usage, vmaUsage, block, maxNumEntries, sizeEntry, numBuffers) );
			blocklist.push_back(block);
		}
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Initialize a single memory block
	*
	* \param[in] allocator The VMA allocator
	* \param[in] usage VkBuffer usage flags
	* \param[in] vmaUsage VMA usage flags
	* \param[in] block A reference to the memory block
	* \param[in] maxNumEntries Maximum number of entries in a memory block
	* \param[in] sizeEntry Size of an entry in bytes
	* \param[in] numBuffers Number of buffers (one for each framebuffer)
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockInit(VmaAllocator allocator, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
							vhMemoryBlock &block, uint32_t maxNumEntries, uint32_t sizeEntry, uint32_t numBuffers) {

		block.allocator = allocator;
		block.usage = usage;
		block.vmaUsage = vmaUsage;

		block.pMemory = new int8_t[sizeEntry*maxNumEntries];
		block.maxNumEntries = maxNumEntries;
		block.sizeEntry = sizeEntry;
		block.dirty.resize(numBuffers);
		for (auto d : block.dirty) d = false;

		block.buffers.resize(numBuffers);
		block.allocations.resize(numBuffers);
		block.descriptorSets.resize(numBuffers);
		for (uint32_t i = 0; i < numBuffers; i++) {
			VHCHECKRESULT(vhBufCreateBuffer(allocator, sizeEntry*maxNumEntries, usage, vmaUsage, &block.buffers[i], &block.allocations[i]));
		}

		//TODO create descriptor sets!

	}


	/**
	*
	* \brief Add an entry to a memory block list
	*
	* \param[in] blocklist The memory block list
	* \param[in] owner Pointer to the entry owner
	* \param[in] handle A handle to the entry
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockListAdd(	std::vector<vhMemoryBlock> &blocklist, void* owner, vhMemoryHandle *handle) {

		vhMemoryBlock *pMemBlock = nullptr;

		for (uint32_t i = 0; i < blocklist.size(); i++ ) {			//find a memory block with a free entry
			if ( blocklist[i].handles.size() < blocklist[0].maxNumEntries) {
				pMemBlock = &blocklist[i];
				break;
			}
		}

		if ( pMemBlock==nullptr ) {													//none found - create one
			vhMemoryBlock block = {};												//new block
			VHCHECKRESULT( vhMemBlockInit(	blocklist[0].allocator, blocklist[0].usage,		//initialize the new block using the first block
											blocklist[0].vmaUsage, 
											block, blocklist[0].maxNumEntries, 
											blocklist[0].sizeEntry, 
											(uint32_t)blocklist[0].buffers.size()) );

			blocklist.push_back(block);												//add the new mem block to the block list

			pMemBlock = &blocklist[blocklist.size() - 1];							//use the new mem block in the following
		}

		handle->owner = owner;														//need to know this for drawing
		handle->pMemBlock = pMemBlock;												//pointer to last mem block
		pMemBlock->handles.push_back(handle);										//add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(pMemBlock->handles.size() - 1);				//index of last entry in block

		return VK_SUCCESS;
	}


	/**
	*
	* \brief Add an entry to a memory block
	*
	* \param[in] block The memory block to be appended
	* \param[in] owner Pointer to the entry owner
	* \param[in] handle A handle to the entry
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockAdd(	vhMemoryBlock &block, void* owner, vhMemoryHandle *handle ) {
		if (block.handles.size() == block.maxNumEntries ) {				//is the block already full?
			int8_t *pOldMem = block.pMemory;							//remember old memory
			uint32_t oldSize = block.sizeEntry * block.maxNumEntries;	//remember old buffer size

			block.maxNumEntries *= 2;									//double the capacity
			for (uint32_t i = 0; i < block.buffers.size(); i++) {		//destroy the old Vulkan buffers
				vmaDestroyBuffer(block.allocator, block.buffers[i], block.allocations[i]);
			}

			VHCHECKRESULT( vhMemBlockInit(	block.allocator, block.usage, block.vmaUsage,	//allocate new memory and buffers
											block, block.maxNumEntries, 
											block.sizeEntry, (uint32_t)block.buffers.size()) );

			memcpy(block.pMemory, pOldMem, oldSize);			//copy old data to the new buffer
			for (auto d : block.dirty) d = true;				//set the block dirty
			delete[] pOldMem;									//deallocate the old buffer memory
		}

		handle->owner = owner;												//need to know this for drawing
		handle->pMemBlock = &block;											//pointer to last mem block
		block.handles.push_back(handle);									//add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(block.handles.size() - 1);			//index of last entry in block
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
		for (auto d : pHandle->pMemBlock->dirty) d = true;								//mark as dirty
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Update all memory blocks by copying them to the GPU
	*
	* \param[in] blocklist The memory blocks
	* \param[in] index The index of the buffer to use
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockUpdateMem( std::vector<vhMemoryBlock> &blocklist, uint32_t index) {
		for (auto block : blocklist) {
			if (block.dirty[index]) {
				void* data = nullptr;
				vmaMapMemory(block.allocator, block.allocations[index], &data);
				memcpy(data, block.pMemory, block.maxNumEntries*block.sizeEntry );
				vmaUnmapMemory(block.allocator, block.allocations[index] );
				block.dirty[index] = false;
			}
		}
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Remove an entry from a memory block
	*
	* \param[in] pHandle Pointer to the handle of the entry that should be removed
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockRemoveEntry(vhMemoryHandle *pHandle) {

		vhMemoryBlock *pMemBlock = pHandle->pMemBlock;				//to keep code readable
		uint32_t idxEntry = pHandle->entryIndex;
		uint32_t idxLast  = (uint32_t)pMemBlock->handles.size() - 1;

		void *pEntry = pMemBlock->pMemory + pMemBlock->sizeEntry * idxEntry;	//entry to be deleted
		void *pLast	 = pMemBlock->pMemory + pMemBlock->sizeEntry * idxLast;		//last entry in the block
		memcpy(pEntry, pLast, pMemBlock->sizeEntry);				//copy last entry over the deleted entry

		vhMemoryHandle *pHandleLast = pMemBlock->handles[idxLast];	//pointer to the last handle in the list
		pMemBlock->handles[idxEntry] = pHandleLast;					//write over old handle
		pHandleLast->entryIndex = idxEntry;							//update also handle in object

		pMemBlock->handles.pop_back();								//remove last handle, could also be the deleted one
		
		for( auto d : pMemBlock->dirty) d = true;

		return VK_SUCCESS;
	}


	/**
	*
	* \brief Move a handle from one list to another list
	*
	* \param[in] dstblocklist The destination block list
	* \param[in] pHandle Pointer to the handle of the entry that should be moved
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockMoveEntry(std::vector<vhMemoryBlock> &dstblocklist, vhMemoryHandle *pHandle ) {
		VHCHECKRESULT( vhMemBlockRemoveEntry( pHandle ) );
		return vhMemBlockListAdd( dstblocklist, pHandle->owner, pHandle );
	}


	/**
	*
	* \brief Delete all entries and all blocks from a block list
	*
	* \param[in] blocklist The memory block list
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockListClear(  std::vector<vhMemoryBlock> &blocklist ) {
		for (auto block : blocklist) {
			VHCHECKRESULT( vhMemBlockDeallocate(block) );
		}
		blocklist.clear();
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Deallocate the memory from a block
	*
	* \param[in] block The memory block that is to be deleted
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockDeallocate( vhMemoryBlock &block ) {
		delete[] block.pMemory;
		for (uint32_t i = 0; i < block.buffers.size(); i++) {
			vmaDestroyBuffer(block.allocator, block.buffers[i], block.allocations[i]);
		}
		//TODO destroy descriptor sets
		return VK_SUCCESS;
	}



}


