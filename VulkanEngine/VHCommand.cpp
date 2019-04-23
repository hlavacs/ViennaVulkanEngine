/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VHHelper.h"

namespace vh {

	//-------------------------------------------------------------------------------------------------------

	/**
	* \brief Create a new command pool
	*
	* \param[in] physicalDevice Physical Vulkan device
	* \param[in] device Logical Vulkan device
	* \param[in] surface Window surface - Needed for finding the right queue families
	* \param[out] commandPool New command pool for allocating command bbuffers
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhCmdCreateCommandPool( VkPhysicalDevice physicalDevice, VkDevice device,
								VkSurfaceKHR surface, VkCommandPool *commandPool) {
		QueueFamilyIndices queueFamilyIndices = vhDevFindQueueFamilies(physicalDevice, surface);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

		return vkCreateCommandPool(device, &poolInfo, nullptr, commandPool);
	}

	//-------------------------------------------------------------------------------------------------------

	/**
	* \brief Begin submitting a single time command
	*
	* \param[in] device Logical Vulkan device
	* \param[in] commandPool Command pool for allocating command bbuffers
	* \returns a new VkCommandBuffer to record commands into
	*/
	VkCommandBuffer vhCmdBeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool ) {
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
			return VK_NULL_HANDLE;

		return commandBuffer;
	}


	/**
	* \brief End recording into a single time command buffer and submit it
	*
	* \param[in] device Logical Vulkan device
	* \param[in] graphicsQueue Queue to submit the buffer to
	* \param[in] commandPool Give back the command buffer to the pool
	* \param[in] commandBuffer The ready to be used command buffer
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool,
									VkCommandBuffer commandBuffer) {

		return vhCmdEndSingleTimeCommands(	device, graphicsQueue, commandPool, commandBuffer, 
											VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE );
	}


	/**
	* \brief End recording into a single time command buffer and submit it
	*
	* \param[in] device Logical Vulkan device
	* \param[in] graphicsQueue Queue to submit the buffer to
	* \param[in] commandPool Give back the command buffer to the pool
	* \param[in] commandBuffer The ready to be used command buffer
	* \param[in] waitSemaphore A semaphore to wait for before submitting
	* \param[in] signalSemaphore Signal this semaphore after buffer is done
	* \param[in] waitFence Signal to this fence after buffer is done
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool,
									VkCommandBuffer commandBuffer,
									VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence waitFence ) {
		VHCHECKRESULT( vkEndCommandBuffer(commandBuffer) );

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { waitSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		if (waitSemaphore != VK_NULL_HANDLE) {
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { signalSemaphore };
		if (signalSemaphore != VK_NULL_HANDLE) {
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		if (waitFence != VK_NULL_HANDLE) {
			vkResetFences(device, 1, &waitFence);
		}

		VHCHECKRESULT( vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFence) );
		VHCHECKRESULT( vkQueueWaitIdle(graphicsQueue) );

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

		return VK_SUCCESS;
	}


}



