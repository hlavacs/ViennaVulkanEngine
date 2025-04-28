#pragma once

namespace vvh {

	//---------------------------------------------------------------------------------------------

    struct ComCreateCommandPoolinfo {
		const VkSurfaceKHR& 	m_surface;
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		VkCommandPool& 			m_commandPool;
	};

	template<typename T = ComCreateCommandPoolinfo>
	inline void ComCreateCommandPool(T&& info) {
        QueueFamilyIndices queueFamilyIndices = DevFindQueueFamilies({ info.m_physicalDevice, info.m_surface });

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(info.m_device, &poolInfo, nullptr, &info.m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

	//---------------------------------------------------------------------------------------------

    struct ComBeginSingleTimeCommandsInfo {
		const VkDevice& m_device;
		const VkCommandPool& 	m_commandPool;
	};

	template<typename T = ComBeginSingleTimeCommandsInfo>
	inline auto ComBeginSingleTimeCommands(T&& info) -> VkCommandBuffer {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = info.m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(info.m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

	//---------------------------------------------------------------------------------------------
	//struct defined in VHBuffer.h
	
	template<typename T>
	inline void ComEndSingleTimeCommands(T&& info) {
        vkEndCommandBuffer(info.m_commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &info.m_commandBuffer;

        vkQueueSubmit(info.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(info.m_graphicsQueue);

        vkFreeCommandBuffers(info.m_device, info.m_commandPool, 1, &info.m_commandBuffer);
    }

	//---------------------------------------------------------------------------------------------

	struct ComCreateCommandBuffersInfo { 
		const VkDevice& 				m_device;
		const VkCommandPool& 			m_commandPool;
		std::vector<VkCommandBuffer>& 	m_commandBuffers;
	};

	template<typename T = ComCreateCommandBuffersInfo>
    inline void ComCreateCommandBuffers(T&& info) {
        if(info.m_commandBuffers.size() == 0) info.m_commandBuffers.resize(2);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = info.m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) info.m_commandBuffers.size();

        if (vkAllocateCommandBuffers(info.m_device, &allocInfo, info.m_commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

	//---------------------------------------------------------------------------------------------

	struct ComBeginCommandBufferInfo {
		const VkCommandBuffer& m_commandBuffer;
	};

	template<typename T = ComBeginCommandBufferInfo>
	inline void ComBeginCommandBuffer(T&& info) {

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(info.m_commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
	}
	
	//---------------------------------------------------------------------------------------------

	struct ComBeginRenderPassInfo {
		const VkCommandBuffer& 	m_commandBuffer;
		const uint32_t& 	m_imageIndex;
		const SwapChain& 	m_swapChain;
		const VkRenderPass& m_renderPass;
		const bool& 		m_clear;
		const glm::vec4& 	m_clearColor;
		const uint32_t& 	m_currentFrame;
	};

	template<typename T = ComBeginRenderPassInfo>
	inline void ComBeginRenderPass(T&& info) {
		
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = info.m_renderPass;
		renderPassInfo.framebuffer = info.m_swapChain.m_swapChainFramebuffers[info.m_imageIndex];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = info.m_swapChain.m_swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		if( info.m_clear) {
			clearValues[0].color = {{info.m_clearColor.r, info.m_clearColor.g, info.m_clearColor.b, info.m_clearColor.w}};  
			clearValues[1].depthStencil = {1.0f, 0};

			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();
		}

		vkCmdBeginRenderPass(info.m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}
		
	//---------------------------------------------------------------------------------------------

    struct ComBindPipelineInfo { 
		const VkCommandBuffer& 				m_commandBuffer;
		const Pipeline& 						m_graphicsPipeline;
		const uint32_t&					m_imageIndex;
        const SwapChain& 				m_swapChain;
		const VkRenderPass& 			m_renderPass; 
        const std::vector<VkViewport>& 	m_viewPorts;
		const std::vector<VkRect2D>& 	m_scissors;
        const std::array<float,4>& 		m_blendConstants;
		const std::vector<PushConstants>& m_pushConstants;
        const uint32_t& 				m_currentFrame;
	};

	template<typename T = ComBindPipelineInfo>
	inline void ComBindPipeline(T&& info) {

		std::vector<VkViewport> viewPorts = info.m_viewPorts;
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) info.m_swapChain.m_swapChainExtent.width;
		viewport.height = (float) info.m_swapChain.m_swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		if(viewPorts.size() == 0) viewPorts.push_back(viewport);
		vkCmdSetViewport(info.m_commandBuffer, 0, viewPorts.size(), viewPorts.data());
  

		std::vector<VkRect2D> scissors = info.m_scissors;
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = info.m_swapChain.m_swapChainExtent;
		if(scissors.size() == 0) scissors.push_back(scissor);
		vkCmdSetScissor(info.m_commandBuffer, 0, scissors.size(), scissors.data());

		vkCmdSetBlendConstants(info.m_commandBuffer, &info.m_blendConstants[0]);

		for( auto& pc : info.m_pushConstants ) {
			vkCmdPushConstants(info.m_commandBuffer, pc.layout, pc.stageFlags, pc.offset, pc.size, pc.pValues);
		}

		vkCmdBindPipeline(info.m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, info.m_graphicsPipeline.m_pipeline);
	}
	
	//---------------------------------------------------------------------------------------------

	struct ComEndCommandBufferInfo {
		const VkCommandBuffer& m_commandBuffer;
	};

	template<typename T = ComEndCommandBufferInfo>
	inline void ComEndCommandBuffer(T&& info) {
		if (vkEndCommandBuffer(info.m_commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}
	
	//---------------------------------------------------------------------------------------------

    struct ComEndRenderPassInfo {
		const VkCommandBuffer& m_commandBuffer;
	};

	template<typename T = ComEndRenderPassInfo>
	inline void ComEndRenderPass(T&& info) {
        vkCmdEndRenderPass(info.m_commandBuffer);
    }

	//---------------------------------------------------------------------------------------------

    struct ComRecordObjectInfo {
		const VkCommandBuffer& 	m_commandBuffer;
		const Pipeline& 		m_graphicsPipeline;
		const std::vector<DescriptorSet>&& m_descriptorSets;
		const std::string 	m_type;
		const Mesh& 		m_mesh;
		const uint32_t& 	m_currentFrame;
	};

	template<typename T = ComRecordObjectInfo>
	inline void ComRecordObject(T&& info) {
	
		auto offsets = info.m_mesh.m_verticesData.getOffsets(info.m_type);
		std::vector<VkBuffer> vertexBuffers(offsets.size(), info.m_mesh.m_vertexBuffer);
		   vkCmdBindVertexBuffers(info.m_commandBuffer, 0, (uint32_t)offsets.size(), vertexBuffers.data(), offsets.data());

		vkCmdBindIndexBuffer(info.m_commandBuffer, info.m_mesh.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for( auto& descriptorSet : info.m_descriptorSets ) {
			vkCmdBindDescriptorSets(info.m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, info.m_graphicsPipeline.m_pipelineLayout, 
				descriptorSet.m_set, 1, &descriptorSet.m_descriptorSetPerFrameInFlight[info.m_currentFrame], 0, nullptr);
		}

		vkCmdDrawIndexed(info.m_commandBuffer, static_cast<uint32_t>(info.m_mesh.m_indices.size()), 1, 0, 0, 0);
	}

	//---------------------------------------------------------------------------------------------

	struct ComSubmitCommandBuffersInfo {
		const VkDevice& 					m_device;
		const VkQueue& 						m_graphicsQueue;
		const std::vector<VkCommandBuffer>& m_commandBuffers;
		std::vector<VkSemaphore>& 			m_imageAvailableSemaphores;
		std::vector<VkSemaphore>& 			m_renderFinishedSemaphores; 
        std::vector<Semaphores>& 			m_intermediateSemaphores; 
		const std::vector<VkFence>& 		m_fences; 
		const uint32_t& 					m_currentFrame;
	};

	template<typename T = ComSubmitCommandBuffersInfo>
	inline void ComSubmitCommandBuffers(T&& info) {

		size_t size = info.m_commandBuffers.size();
		if( size > info.m_intermediateSemaphores.size() ) {
			vvh::SynCreateSemaphores({
				.m_device = info.m_device, 
				.m_imageAvailableSemaphores = info.m_imageAvailableSemaphores, 
				.m_renderFinishedSemaphores = info.m_renderFinishedSemaphores, 
				.m_size 					= size,
				.m_intermediateSemaphores 	= info.m_intermediateSemaphores 
			});
		}

		vkResetFences(info.m_device, 1, &info.m_fences[info.m_currentFrame]);

		const VkSemaphore* waitSemaphore = &info.m_imageAvailableSemaphores[info.m_currentFrame];
		std::vector<VkSubmitInfo> submitInfos(info.m_commandBuffers.size());
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkFence fence = VK_NULL_HANDLE;
		 
		for( int i = 0; i < size; i++ ) {
			submitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfos[i].commandBufferCount = 1;
			submitInfos[i].pCommandBuffers = &info.m_commandBuffers[i];
			
			submitInfos[i].waitSemaphoreCount = 1;
			submitInfos[i].pWaitSemaphores = waitSemaphore;
			submitInfos[i].pWaitDstStageMask = waitStages;
			
			VkSemaphore* signalSemaphore = &info.m_intermediateSemaphores[i].m_renderFinishedSemaphores[info.m_currentFrame];
			if( i== size-1 ) {
				fence = info.m_fences[info.m_currentFrame];
				signalSemaphore = &info.m_renderFinishedSemaphores[info.m_currentFrame];
			}
			submitInfos[i].signalSemaphoreCount = 1;
			submitInfos[i].pSignalSemaphores = signalSemaphore;

			waitSemaphore = &info.m_intermediateSemaphores[i].m_renderFinishedSemaphores[info.m_currentFrame];
		}
		  if (vkQueueSubmit(info.m_graphicsQueue, submitInfos.size(), submitInfos.data(), fence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
	}
	   
	//---------------------------------------------------------------------------------------------

	struct ComPresentImageInfo {
		const VkQueue& 		m_presentQueue;
		const SwapChain& 	m_swapChain;
		const uint32_t& 	m_imageIndex;
		const VkSemaphore& 	m_signalSemaphore;
	};

	template<typename T = ComPresentImageInfo>
	inline auto ComPresentImage(T&& info) -> VkResult {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &info.m_signalSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &info.m_swapChain.m_swapChain;
        presentInfo.pImageIndices = &info.m_imageIndex;
        return vkQueuePresentKHR(info.m_presentQueue, &presentInfo);
	}

} // namespace vh


