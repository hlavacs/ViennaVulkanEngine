
#include "VHInclude.h"


namespace vh {

	void ComCreateCommandPool(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool& commandPool) {
        QueueFamilyIndices queueFamilyIndices = DevFindQueueFamilies(physicalDevice, surface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

    VkCommandBuffer ComBeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    VkResult ComEndSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        return VK_SUCCESS;
    }


    void ComCreateCommandBuffers(VkDevice device, VkCommandPool commandPool, std::vector<VkCommandBuffer>& commandBuffers) {
        if(commandBuffers.size() == 0) commandBuffers.resize(2);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }


	void ComStartRecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, bool clear, glm::vec4 clearColor, uint32_t currentFrame) {

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChain.m_swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChain.m_swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		if( clear) {
	        clearValues[0].color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.w}};  
	        clearValues[1].depthStencil = {1.0f, 0};

	        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	        renderPassInfo.pClearValues = clearValues.data();
		}

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

	void ComBindPipeline(VkCommandBuffer commandBuffer, uint32_t imageIndex
        , SwapChain& swapChain, VkRenderPass renderPass, Pipeline& graphicsPipeline
        , std::vector<VkViewport> viewPorts, std::vector<VkRect2D> scissors
        , std::array<float,4>& blendConstants
        , std::vector<PushConstants> pushConstants
        , uint32_t currentFrame) {

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChain.m_swapChainExtent.width;
        viewport.height = (float) swapChain.m_swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        if(viewPorts.size() == 0) viewPorts.push_back(viewport);
        vkCmdSetViewport(commandBuffer, 0, viewPorts.size(), viewPorts.data());
  
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain.m_swapChainExtent;
        if(scissors.size() == 0) scissors.push_back(scissor);
        vkCmdSetScissor(commandBuffer, 0, scissors.size(), scissors.data());

        vkCmdSetBlendConstants(commandBuffer, &blendConstants[0]);

        for( auto& pc : pushConstants ) {
            vkCmdPushConstants(commandBuffer, pc.layout, pc.stageFlags, pc.offset, pc.size, pc.pValues);
        }

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.m_pipeline);
	}

    void ComEndRecordCommandBuffer(VkCommandBuffer commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void ComRecordObject(VkCommandBuffer commandBuffer, Pipeline& graphicsPipeline, 
			const std::vector<DescriptorSet>&& descriptorSets, std::string type, Mesh& mesh, uint32_t currentFrame) {

        auto offsets = mesh.m_verticesData.getOffsets(type);
        std::vector<VkBuffer> vertexBuffers(offsets.size(), mesh.m_vertexBuffer);
       	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)offsets.size(), vertexBuffers.data(), offsets.data());

        vkCmdBindIndexBuffer(commandBuffer, mesh.m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for( auto& descriptorSet : descriptorSets ) {
        	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.m_pipelineLayout, 
            	descriptorSet.m_set, 1, &descriptorSet.m_descriptorSetPerFrameInFlight[currentFrame], 0, nullptr);
		}

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.m_indices.size()), 1, 0, 0, 0);
	}

	void ComSubmitCommandBuffers(VkDevice device, VkQueue graphicsQueue, std::vector<VkCommandBuffer>& commandBuffers, 
		std::vector<VkSemaphore>& imageAvailableSemaphores, 
		std::vector<VkSemaphore>& renderFinishedSemaphores, 
        std::vector<Semaphores>& intermediateSemaphores, 
		std::vector<VkFence>& fences, uint32_t currentFrame) {

		size_t size = commandBuffers.size();
		if( size > intermediateSemaphores.size() ) {
			vh::SynCreateSemaphores(device, imageAvailableSemaphores, renderFinishedSemaphores, size, intermediateSemaphores);
		}

        vkResetFences(device, 1, &fences[currentFrame]);

		VkSemaphore* waitSemaphore = &imageAvailableSemaphores[currentFrame];
		std::vector<VkSubmitInfo> submitInfos(commandBuffers.size());
	    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkFence fence = VK_NULL_HANDLE;
         
		for( int i = 0; i < size; i++ ) {
            submitInfos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	        submitInfos[i].commandBufferCount = 1;
	        submitInfos[i].pCommandBuffers = &commandBuffers[i];
	        
            submitInfos[i].waitSemaphoreCount = 1;
	        submitInfos[i].pWaitSemaphores = waitSemaphore;
	        submitInfos[i].pWaitDstStageMask = waitStages;
	        
            VkSemaphore* signalSemaphore = &intermediateSemaphores[i].m_renderFinishedSemaphores[currentFrame];
			if( i== size-1 ) {
                fence = fences[currentFrame];
                signalSemaphore = &renderFinishedSemaphores[currentFrame];
            }
            submitInfos[i].signalSemaphoreCount = 1;
	        submitInfos[i].pSignalSemaphores = signalSemaphore;

			waitSemaphore = &intermediateSemaphores[i].m_renderFinishedSemaphores[currentFrame];
		}
  	    if (vkQueueSubmit(graphicsQueue, submitInfos.size(), submitInfos.data(), fence) != VK_SUCCESS) {
	        throw std::runtime_error("failed to submit draw command buffer!");
	    }
	}

	VkResult ComPresentImage(VkQueue presentQueue, SwapChain swapChain, uint32_t& imageIndex, VkSemaphore& signalSemaphore) {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &signalSemaphore;
        VkSwapchainKHR swapChains[] = {swapChain.m_swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(presentQueue, &presentInfo);
	}


} // namespace vh

