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

	/**
		*
		* \brief Create a new command pool
		*
		* \param[in] physicalDevice Physical Vulkan device
		* \param[in] device Logical Vulkan device
		* \param[in] surface Window surface - Needed for finding the right queue families
		* \param[out] commandPool New command pool for allocating command bbuffers
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhCmdCreateCommandPool(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, VkCommandPool *commandPool)
	{
		QueueFamilyIndices queueFamilyIndices = vhDevFindQueueFamilies(physicalDevice, surface);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

		return vkCreateCommandPool(device, &poolInfo, nullptr, commandPool);
	}

	//-------------------------------------------------------------------------------------------------------

	/**
		*
		* \brief Create a number of command buffers
		*
		* \param[in] device Logical Vulkan device
		* \param[in] commandPool Command pool for allocating command bbuffers
		* \param[in] level Level can be primary or secondary
		* \param[in] count Number of buffers to create
		* \param[in] pBuffers Pointer to an array where the buffer handles should be stored
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhCmdCreateCommandBuffers(VkDevice device, VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer *pBuffers)
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = level;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = count;

		return vkAllocateCommandBuffers(device, &allocInfo, pBuffers);
	}

	/**
		*
		* \brief Start a command buffer for recording commands
		*
		* \param[in] device Logical Vulkan device
		* \param[in] renderPass The render pass that is inherited from the parent command buffer
		* \param[in] subpass Index of subpass
		* \param[in] frameBuffer The framebuffer that is rendered into
		* \param[in] commandBuffer The command buffer to start
		* \param[in] usageFlags Flags telling how the buffer will be used
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult
		vhCmdBeginCommandBuffer(VkDevice device, VkRenderPass renderPass, uint32_t subpass, VkFramebuffer frameBuffer, VkCommandBuffer commandBuffer, VkCommandBufferUsageFlagBits usageFlags)
	{
		VkCommandBufferInheritanceInfo inheritance = {};
		inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritance.framebuffer = frameBuffer;
		inheritance.renderPass = renderPass;
		inheritance.subpass = subpass;

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = usageFlags;
		beginInfo.pInheritanceInfo = &inheritance;

		return vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	/**
		*
		* \brief Start a command buffer for recording commands
		*
		* \param[in] device Logical Vulkan device
		* \param[in] commandBuffer The command buffer to start
		* \param[in] usageFlags Flags telling how the buffer will be used
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhCmdBeginCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkCommandBufferUsageFlagBits usageFlags)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = usageFlags;

		return vkBeginCommandBuffer(commandBuffer, &beginInfo);
	}

	/**
		*
		* \brief Submit a command buffer to a queue
		*
		* \param[in] device Logical Vulkan device
		* \param[in] queue The queue the buffer is sent to
		* \param[in] commandBuffer The command buffer that is sent to the queue
		* \param[in] waitSemaphore A semaphore to wait for before submitting
		* \param[in] signalSemaphore Signal this semaphore after buffer is done
		* \param[in] waitFence Signal to this fence after buffer is done
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhCmdSubmitCommandBuffer(VkDevice device, VkQueue queue, VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence waitFence)
	{
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { waitSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { signalSemaphore };
		if (signalSemaphore != VK_NULL_HANDLE)
		{
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		if (waitFence != VK_NULL_HANDLE)
		{
			vkResetFences(device, 1, &waitFence);
		}

		return vkQueueSubmit(queue, 1, &submitInfo, waitFence);
	}

	/**
		*
		* \brief Begin submitting a single time command
		*
		* \param[in] device Logical Vulkan device
		* \param[in] commandPool Command pool for allocating command bbuffers
		* \returns a new VkCommandBuffer to record commands into
		*
		*/
	VkCommandBuffer vhCmdBeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
	{
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
		*
		* \brief End recording into a single time command buffer and submit it
		*
		* \param[in] device Logical Vulkan device
		* \param[in] graphicsQueue Queue to submit the buffer to
		* \param[in] commandPool Give back the command buffer to the pool
		* \param[in] commandBuffer The ready to be used command buffer
		* \returns VK_SUCCESS or a Vulkan error code
		*
		*/
	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
	{
		VkFence waitFence;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		vkCreateFence(device, &fenceInfo, nullptr, &waitFence);

		VkResult result = vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, waitFence);

		vkDestroyFence(device, waitFence, nullptr);

		return result;
	}

	/**
		*
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
	VkResult vhCmdEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence waitFence)
	{
		VHCHECKRESULT(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { waitSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		if (waitSemaphore != VK_NULL_HANDLE)
		{
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
		}

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkSemaphore signalSemaphores[] = { signalSemaphore };
		if (signalSemaphore != VK_NULL_HANDLE)
		{
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;
		}

		if (waitFence != VK_NULL_HANDLE)
		{
			vkResetFences(device, 1, &waitFence);
		}

		VHCHECKRESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFence));

		if (waitFence != VK_NULL_HANDLE)
		{
			VHCHECKRESULT(vkWaitForFences(device, 1, &waitFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
		}
		else
			VHCHECKRESULT(vkQueueWaitIdle(graphicsQueue));

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

		return VK_SUCCESS;
	}

} // namespace vh
