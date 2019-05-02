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
	* \param[in] device The logical Vulkan device
	* \param[in] pool A descriptor set pool
	* \param[in] layout Descriptor set layout
	* \param[in] allocator The VMA allocator
	* \param[in] maxNumEntries Maximum number of entries in a memory block
	* \param[in] sizeEntry Size of an entry in bytes
	* \param[in] numBuffers Number of buffers (one for each framebuffer)
	* \param[in] blocklist A reference to the memory block list
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockListInit(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
								VmaAllocator allocator, uint32_t maxNumEntries, uint32_t sizeEntry, 
								uint32_t numBuffers, std::vector<vhMemoryBlock> &blocklist) {
		if (blocklist.empty()) {
			vhMemoryBlock block = {};
			VHCHECKRESULT( vhMemBlockInit(device, pool, layout, allocator, maxNumEntries, sizeEntry, numBuffers, block) );
			blocklist.push_back(block);
		}
		return VK_SUCCESS;
	}


	/**
	*
	* \brief Initialize a single memory block
	*
	* \param[in] device The logical Vulkan device
	* \param[in] pool A descriptor set pool
	* \param[in] layout Descriptor set layout
	* \param[in] allocator The VMA allocator
	* \param[in] maxNumEntries Maximum number of entries in a memory block
	* \param[in] sizeEntry Size of an entry in bytes
	* \param[in] numBuffers Number of buffers (one for each framebuffer)
	* \param[in] block A reference to the memory block
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhMemBlockInit(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
							VmaAllocator allocator, uint32_t maxNumEntries, uint32_t sizeEntry, 
							uint32_t numBuffers, vhMemoryBlock &block ) {

		block.device = device;			//needed for creating descriptor sets
		block.pool = pool;
		block.layout = layout;
		block.allocator = allocator;	//needed for allocating buffers

		block.pMemory = new int8_t[sizeEntry*maxNumEntries];	//allocate host memory
		block.maxNumEntries = maxNumEntries;					//max number of entries in a block
		block.sizeEntry = sizeEntry;							//size of an entry (a UBO)
		block.dirty.resize(numBuffers);							//flags for determining whether to update GPU buffers from host buffer
		for (auto d : block.dirty) d = false;					//default is dirty -> update the next time

		block.buffers.resize(numBuffers);						//Vulkan buffers
		block.allocations.resize(numBuffers);					//VMA allocations
		block.descriptorSets.resize(numBuffers);				//descriptor sets
		VHCHECKRESULT(vhRenderCreateDescriptorSets(block.device, numBuffers, block.layout, block.pool, block.descriptorSets));
		VHCHECKRESULT(vhBufCreateUniformBuffers(allocator, numBuffers, sizeEntry*maxNumEntries, block.buffers, block.allocations));

		for (uint32_t i = 0; i < numBuffers; i++) {
			VHCHECKRESULT( vhRenderUpdateDescriptorSet(	block.device, block.descriptorSets[i],
														{ block.buffers[i] },				//UBOs
														{ sizeEntry*maxNumEntries },		//UBO sizes
														{ {VK_NULL_HANDLE} }, { {VK_NULL_HANDLE} }));
		}
		return VK_SUCCESS;
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

		for (uint32_t i = 0; i < blocklist.size(); i++ ) {							//find a memory block with a free entry
			if ( blocklist[i].handles.size() < blocklist[0].maxNumEntries) {
				pMemBlock = &blocklist[i];
				break;
			}
		}

		if ( pMemBlock==nullptr ) {													//none found - create a new block
			vhMemoryBlock block = {};												//new block
			
			//initialize the new block using the first block
			VHCHECKRESULT( vhMemBlockInit(	blocklist[0].device, blocklist[0].pool, blocklist[0].layout, 
											blocklist[0].allocator, blocklist[0].maxNumEntries, blocklist[0].sizeEntry, 
											(uint32_t)blocklist[0].buffers.size(), block) );

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
	* \brief Add an entry to a memory block (not a block list)
	*
	* This function should only be called for single memory blocks that are not part of a block list.
	* If the block gets full, it is reallocated with twice the old capacity.
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
			uint32_t oldSize = block.sizeEntry * block.maxNumEntries;	//remember old host memory size

			block.maxNumEntries *= 2;									//double the capacity
			for (uint32_t i = 0; i < block.buffers.size(); i++) {		//destroy the old Vulkan buffers
				vmaDestroyBuffer(block.allocator, block.buffers[i], block.allocations[i]);
			}
			vkFreeDescriptorSets(block.device, block.pool, (uint32_t)block.descriptorSets.size(), block.descriptorSets.data());

			VHCHECKRESULT( vhMemBlockInit(	block.device, block.pool, block.layout,				//allocate new memory, buffers, desc. sets
											block.allocator, block.maxNumEntries, block.sizeEntry, 
											(uint32_t)block.buffers.size(), block) );

			memcpy(block.pMemory, pOldMem, oldSize);						//copy old data to the new buffer
			for (auto d : block.dirty) d = true;							//set the block dirty
			delete[] pOldMem;												//deallocate the old buffer memory
		}

		handle->owner = owner;												//need to know this for drawing
		handle->pMemBlock = &block;											//pointer to last mem block
		block.handles.push_back(handle);									//add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(block.handles.size() - 1);			//index of last entry in block
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
	VkResult vhMemBlockUpdateBlockList( std::vector<vhMemoryBlock> &blocklist, uint32_t index) {
		for (auto block : blocklist) {												//go through all blocks
			if (block.dirty[index]) {												//update only if dirty
				void* data = nullptr;
				vmaMapMemory(block.allocator, block.allocations[index], &data);		//map device to host memory
				memcpy(data, block.pMemory, block.maxNumEntries*block.sizeEntry );	//copy host memory
				vmaUnmapMemory(block.allocator, block.allocations[index] );			//unmap
				block.dirty[index] = false;											//no more dirty
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
		uint32_t idxEntry = pHandle->entryIndex;					//index of the entry to be removed
		uint32_t idxLast  = (uint32_t)pMemBlock->handles.size() - 1;//index of the last entry in the block

		void *pEntry = pMemBlock->pMemory + pMemBlock->sizeEntry * idxEntry;	//pointer to entry to be removed
		void *pLast	 = pMemBlock->pMemory + pMemBlock->sizeEntry * idxLast;		//pointer to last entry in the block
		memcpy(pEntry, pLast, pMemBlock->sizeEntry);				//copy last entry over the removed entry

		vhMemoryHandle *pHandleLast = pMemBlock->handles[idxLast];	//pointer to the last handle in the list
		pMemBlock->handles[idxEntry] = pHandleLast;					//write over old handle
		pHandleLast->entryIndex = idxEntry;							//update also handle in object

		pMemBlock->handles.pop_back();								//remove last handle, could also be identical to the removed one
		
		for( auto d : pMemBlock->dirty) d = true;					//mark as dirty

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
		VHCHECKRESULT( vhMemBlockRemoveEntry( pHandle ) );					//remove from the old block
		return vhMemBlockListAdd( dstblocklist, pHandle->owner, pHandle );	//add to the destination block list
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
		vkFreeDescriptorSets(block.device, block.pool, (uint32_t)block.descriptorSets.size(), block.descriptorSets.data());
		return VK_SUCCESS;
	}



}


