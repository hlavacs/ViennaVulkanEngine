/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VHHelper.h"

namespace vh
{
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
	uint32_t
		vhMemFindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
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
	VkResult vhMemCreateVMAAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator &allocator)
	{
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = physicalDevice;
		allocatorInfo.device = device;
		allocatorInfo.instance = instance;
		allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		return vmaCreateAllocator(&allocatorInfo, &allocator);
	}

	//-------------------------------------------------------------------------------------------------------
	//memory blocks

	/**
		*
		* \brief Initialize a list of memory blocks
		*
		* \param[in] device The logical Vulkan device
		* \param[in] allocator The VMA allocator
		* \param[in] descriptorPool Descriptor pool for creating descriptor sets
		* \param[in] descriptorLayout Descriptor layout for creating descriptor sets
		* \param[in] maxNumEntries Maximum number of entries in a memory block
		* \param[in] sizeEntry Size of an entry in bytes
		* \param[in] numBuffers Number of buffers (one for each framebuffer)
		* \param[in] blocklist A reference to the memory block list
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhMemBlockListInit(VkDevice device, VmaAllocator allocator, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorLayout, uint32_t maxNumEntries, uint32_t sizeEntry, uint32_t numBuffers, std::vector<vhMemoryBlock *> &blocklist)
	{
		if (blocklist.empty())
		{
			vhMemoryBlock *pBlock = new vhMemoryBlock;
			VHCHECKRESULT(vhMemBlockInit(device, allocator, descriptorPool, descriptorLayout, maxNumEntries, sizeEntry,
				numBuffers, pBlock));
			blocklist.push_back(pBlock);
		}
		return VK_SUCCESS;
	}

	/**
		*
		* \brief Initialize a single memory block
		*
		* \param[in] device The logical Vulkan device
		* \param[in] allocator The VMA allocator
		* \param[in] descriptorPool Descriptor pool for creating descriptor sets
		* \param[in] descriptorLayout Descriptor layout for creating descriptor sets
		* \param[in] maxNumEntries Maximum number of entries in a memory block
		* \param[in] sizeEntry Size of an entry in bytes
		* \param[in] numBuffers Number of buffers (one for each framebuffer)
		* \param[in] pBlock A pointer to the memory block
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhMemBlockInit(VkDevice device, VmaAllocator allocator, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorLayout, uint32_t maxNumEntries, uint32_t sizeEntry, uint32_t numBuffers, vhMemoryBlock *pBlock)
	{
		pBlock->device = device; //needed for creating descriptor sets
		pBlock->allocator = allocator; //needed for allocating buffers
		pBlock->descriptorPool = descriptorPool; //for creating descriptor sets
		pBlock->descriptorLayout = descriptorLayout; //for creating descriptor sets

		pBlock->pMemory = new int8_t[sizeEntry * maxNumEntries]; //allocate host memory
		pBlock->maxNumEntries = maxNumEntries; //max number of entries in a block
		pBlock->sizeEntry = sizeEntry; //size of an entry (a UBO)
		pBlock->dirty.resize(
			numBuffers); //flags for determining whether to update GPU buffers from host buffer
		pBlock->setDirty(); //default is dirty -> update the next time

		VHCHECKRESULT(vhBufCreateUniformBuffers(allocator, numBuffers, sizeEntry * maxNumEntries, pBlock->buffers,
			pBlock->allocations));
		auto result = vhRenderCreateDescriptorSets(device, numBuffers, descriptorLayout, descriptorPool,
			pBlock->descriptorSets);
		VHCHECKRESULT(result);

		for (uint32_t i = 0; i < numBuffers; i++)
		{
			VHCHECKRESULT(vhRenderUpdateDescriptorSet(device, pBlock->descriptorSets[i],
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
				{ pBlock->buffers[i] }, { sizeEntry },
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
	VkResult vhMemBlockListAdd(std::vector<vhMemoryBlock *> &blocklist, void *owner, vhMemoryHandle *handle)
	{
		vhMemoryBlock *pMemBlock = nullptr;

		for (uint32_t i = 0; i < blocklist.size(); i++)
		{ //find a memory block with a free entry
			if (blocklist[i]->handles.size() < blocklist[0]->maxNumEntries)
			{
				pMemBlock = blocklist[i];
				break;
			}
		}

		if (pMemBlock == nullptr)
		{ //none found - create a new block
			pMemBlock = new vhMemoryBlock; //new block

			//initialize the new block using the first block
			VHCHECKRESULT(vhMemBlockInit(blocklist[0]->device,
				blocklist[0]->allocator,
				blocklist[0]->descriptorPool,
				blocklist[0]->descriptorLayout,
				blocklist[0]->maxNumEntries,
				blocklist[0]->sizeEntry,
				(uint32_t)blocklist[0]->buffers.size(), pMemBlock));

			blocklist.push_back(pMemBlock); //add the new mem block to the block list
		}

		handle->owner = owner; //need to know this for drawing
		handle->pMemBlock = pMemBlock; //pointer to last mem block
		pMemBlock->handles.push_back(handle); //add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(pMemBlock->handles.size() - 1); //index of last entry in block

		return VK_SUCCESS;
	}

	/**
		*
		* \brief Add an entry to a memory block (not a block list)
		*
		* This function should only be called for single memory blocks that are not part of a block list.
		* If the block gets full, it is reallocated with twice the old capacity.
		*
		* \param[in] pBlock The memory block to be appended
		* \param[in] owner Pointer to the entry owner
		* \param[in] handle A handle to the entry
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhMemBlockAdd(vhMemoryBlock *pBlock, void *owner, vhMemoryHandle *handle)
	{
		if (pBlock->handles.size() == pBlock->maxNumEntries)
		{ //is the block already full?
			int8_t *pOldMem = pBlock->pMemory; //remember old memory
			uint32_t oldSize = pBlock->sizeEntry * pBlock->maxNumEntries; //remember old host memory size

			pBlock->maxNumEntries *= 2; //double the capacity
			for (uint32_t i = 0; i < pBlock->buffers.size(); i++)
			{ //destroy the old Vulkan buffers
				vmaDestroyBuffer(pBlock->allocator, pBlock->buffers[i], pBlock->allocations[i]);
			}

			VHCHECKRESULT(vhMemBlockInit(pBlock->device, pBlock->allocator,
				pBlock->descriptorPool, pBlock->descriptorLayout,
				pBlock->maxNumEntries, pBlock->sizeEntry,
				(uint32_t)pBlock->buffers.size(), pBlock));

			memcpy(pBlock->pMemory, pOldMem, oldSize); //copy old data to the new buffer
			delete[] pOldMem; //deallocate the old buffer memory
		}

		handle->owner = owner; //need to know this for drawing
		handle->pMemBlock = pBlock; //pointer to last mem block
		pBlock->handles.push_back(handle); //add pointer to the handle to the handle list
		handle->entryIndex = (uint32_t)(pBlock->handles.size() - 1); //index of last entry in block
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
	VkResult vhMemBlockUpdateEntry(vhMemoryHandle *pHandle, void *data)
	{
		memcpy(pHandle->getPointer(), data, pHandle->pMemBlock->sizeEntry);
		pHandle->pMemBlock->setDirty();
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
	VkResult vhMemBlockUpdateBlockList(std::vector<vhMemoryBlock *> &blocklist, uint32_t index)
	{
		for (auto pBlock : blocklist)
		{ //go through all blocks
			if (pBlock->dirty[index])
			{ //update only if dirty
				void *data = nullptr;
				vmaMapMemory(pBlock->allocator, pBlock->allocations[index], &data); //map device to host memory
				memcpy(data, pBlock->pMemory, pBlock->maxNumEntries * pBlock->sizeEntry); //copy host memory
				vmaUnmapMemory(pBlock->allocator, pBlock->allocations[index]); //unmap
				pBlock->dirty[index] = false; //no more dirty
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
	VkResult vhMemBlockRemoveEntry(vhMemoryHandle *pHandle)
	{
		vhMemoryBlock *pMemBlock = pHandle->pMemBlock; //to keep code readable
		uint32_t idxEntry = pHandle->entryIndex; //index of the entry to be removed
		uint32_t idxLast = (uint32_t)pMemBlock->handles.size() - 1; //index of the last entry in the block

		void *pEntry = pMemBlock->pMemory + pMemBlock->sizeEntry * idxEntry; //pointer to entry to be removed
		void *pLast = pMemBlock->pMemory + pMemBlock->sizeEntry * idxLast; //pointer to last entry in the block
		memcpy(pEntry, pLast, pMemBlock->sizeEntry); //copy last entry over the removed entry

		vhMemoryHandle *pHandleLast = pMemBlock->handles[idxLast]; //pointer to the last handle in the list
		pMemBlock->handles[idxEntry] = pHandleLast; //write over old handle
		pHandleLast->entryIndex = idxEntry; //update also handle in object

		pMemBlock->handles.pop_back(); //remove last handle, could also be identical to the removed one

		for (auto d : pMemBlock->dirty)
			d = true; //mark as dirty

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
	VkResult vhMemBlockMoveEntry(std::vector<vhMemoryBlock *> &dstblocklist, vhMemoryHandle *pHandle)
	{
		VHCHECKRESULT(vhMemBlockRemoveEntry(pHandle)); //remove from the old block
		return vhMemBlockListAdd(dstblocklist, pHandle->owner, pHandle); //add to the destination block list
	}

	/**
		*
		* \brief Delete all entries and all blocks from a block list
		*
		* \param[in] blocklist The memory block list
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhMemBlockListClear(std::vector<vhMemoryBlock *> &blocklist)
	{
		for (auto block : blocklist)
		{
			VHCHECKRESULT(vhMemBlockDeallocate(block));
		}
		blocklist.clear();
		return VK_SUCCESS;
	}

	/**
		*
		* \brief Deallocate the memory from a block
		*
		* \param[in] pBlock The memory block that is to be deleted
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhMemBlockDeallocate(vhMemoryBlock *pBlock)
	{
		delete[] pBlock->pMemory;
		for (uint32_t i = 0; i < pBlock->buffers.size(); i++)
		{
			vmaDestroyBuffer(pBlock->allocator, pBlock->buffers[i], pBlock->allocations[i]);
		}
		delete pBlock;
		return VK_SUCCESS;
	}

} // namespace vh
