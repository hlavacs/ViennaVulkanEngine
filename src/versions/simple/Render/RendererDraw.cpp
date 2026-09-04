module;
#include <new>
#include <vulkan/vulkan_core.h>

module VEEngine.Simple.Renderer;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer per-frame recording and presentation: shadow passes, the forward color pass, submit, and present.

namespace vve::simple {

	/**
		* @brief Draws one swapchain frame through the per-frame synchronization objects.
		*
		* @param readback Optional swapchain-image readback sink used by deterministic debug captures.
	*/
	void ForwardRenderer::drawFrame(VulkanReadback *readback) {
		lastReadbackCaptureResult.reset();
		const auto windowExtent = currentWindowPixelExtent();
		if (windowExtent.width == 0U || windowExtent.height == 0U) { return; }
		if (const VkResult result = syncSceneResources(); result != VK_SUCCESS) { reportFrameFailure("scene sync", result); return; }
		// Compare against the extent the swapchain was requested for, not the surface-chosen one, so a driver that clamps or reports a different currentExtent does not force a recreate every frame.
		if (windowExtent.width != swapchain.requestedExtent.width || windowExtent.height != swapchain.requestedExtent.height) {
			if (const VkResult result = recreateSwapchain(windowExtent); result != VK_SUCCESS) { reportFrameFailure("swapchain recreate", result); return; }
		}

		const std::size_t frameCount{frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
		if (frameCount == 0U || frameSync.imageAvailableSemaphores.size() < frameCount || frameSync.renderFinishedSemaphores.empty()) { return; }
		if (commandBuffers.commandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || swapchain.swapchain == VK_NULL_HANDLE) { return; }
		if (currentFrame >= frameCount) { currentFrame = 0U; }

		const VkFence inFlightFence{frameSync.inFlightFences[currentFrame]};
		const VkSemaphore imageAvailableSemaphore{frameSync.imageAvailableSemaphores[currentFrame]};
		if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return; }

		VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, UINT64_MAX);
		if (result != VK_SUCCESS) { reportFrameFailure("fence wait", result); return; }

		std::uint32_t imageIndex{};
		result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) { (void)recreateSwapchain(currentWindowPixelExtent()); return; }
		// VK_SUBOPTIMAL_KHR still acquired an image: it must be rendered and presented, otherwise the swapchain runs out of images and the next acquire blocks forever.
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { reportFrameFailure("image acquire", result); return; }
		if (imageIndex >= frameSync.renderFinishedSemaphores.size()) { return; }
		const VkSemaphore renderFinishedSemaphore{frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.
		if (renderFinishedSemaphore == VK_NULL_HANDLE) { return; }

		const Scalar aspectRatio{swapchain.extent.height == 0U ? one() : static_cast<Scalar>(swapchain.extent.width) / static_cast<Scalar>(swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
		constexpr Scalar cameraVerticalFov{static_cast<Scalar>(0.7853981633974483)}; ///< Fixed 45-degree camera field of view.
		constexpr Scalar cameraNear{static_cast<Scalar>(0.1)}; ///< Camera near plane shared by projection and cascade splitting.
		constexpr Scalar cameraFar{static_cast<Scalar>(100.0)}; ///< Camera far plane bounds directional cascade coverage.
		const Mat4 cameraView{lookAt(cameraEye, cameraTarget, Vec3{zero(), one(), zero()})}; ///< Current camera transform shared by uniforms and cascade fitting.
		const ForwardRendererShadowFrame shadowFrame = prepareShadowFrame(cameraView, cameraVerticalFov, aspectRatio, cameraNear, cameraFar);
		const FrameUniforms frameUniforms{
			.view = cameraView,
			.projection = perspectiveVulkan(cameraVerticalFov, aspectRatio, cameraNear, cameraFar),
			.shadowViewProjs = shadowFrame.shadowViewProjs,
			.cascadeSplits = shadowFrame.cascadeSplitsFar,
			.pointLightPositionRanges = shadowFrame.pointLightPositionRanges,
			.pointLightColorIntensities = shadowFrame.pointLightColorIntensities,
			.spotLightPositionRanges = shadowFrame.spotLightPositionRanges,
			.spotLightColorIntensities = shadowFrame.spotLightColorIntensities,
			.spotLightDirections = shadowFrame.spotLightDirections,
			.spotLightConeAmbients = shadowFrame.spotLightConeAmbients,
			.directionalLightDirections = shadowFrame.directionalLightDirections,
			.directionalLightColorIntensities = shadowFrame.directionalLightColorIntensities,
			.directionalLightAmbients = shadowFrame.directionalLightAmbients,
			.activeDirectionalLightCount = shadowFrame.activeDirectionalLightCount,
			.activeSpotLightCount = shadowFrame.activeSpotLightCount,
			.ambient = static_cast<float>(scene.ambient),
		};
		recordShadowDepthSamples(shadowFrame);
		result = uniformBuffers.update(currentFrame, frameUniforms);
		if (result != VK_SUCCESS) { reportFrameFailure("uniform update", result); return; }

		result = recordCommandBuffer(currentFrame, imageIndex, shadowFrame);
		if (result != VK_SUCCESS) { reportFrameFailure("command recording", result); return; }

		// Reset the fence only once the submit is certain; an unsignaled fence without a submit would block the next frame forever.
		result = vkResetFences(device.device, 1U, &inFlightFence);
		if (result != VK_SUCCESS) { reportFrameFailure("fence reset", result); return; }

		const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}; ///< Acquire completes before the swapchain layout transition executes.
		const VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &imageAvailableSemaphore,
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1U,
			.pCommandBuffers = &commandBuffers.commandBuffers[currentFrame],
			.signalSemaphoreCount = 1U,
			.pSignalSemaphores = &renderFinishedSemaphore,
		};
		result = vkQueueSubmit(device.graphicsQueue, 1U, &submitInfo, inFlightFence);
		if (result != VK_SUCCESS) { reportFrameFailure("queue submit", result); return; }
		fillShadowDepthSamplesFromGpu();
		if (readback != nullptr && imageIndex < swapchain.images.size()) {
			lastReadbackCaptureResult = readback->capture(swapchain.images[imageIndex], 0U, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		VkSwapchainKHR presentSwapchain = swapchain.swapchain;
		const VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &renderFinishedSemaphore,
			.swapchainCount = 1U,
			.pSwapchains = &presentSwapchain,
			.pImageIndices = &imageIndex,
		};
		result = vkQueuePresentKHR(device.presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			(void)recreateSwapchain(currentWindowPixelExtent());
			return;
		}
		if (result != VK_SUCCESS) { reportFrameFailure("present", result); return; }

		lastRenderedImageIndex = imageIndex;
		currentFrame = static_cast<std::uint32_t>((currentFrame + 1U) % frameCount);
	}

	/// @brief Logs a skipped frame with its Vulkan result; capped so a persistent failure does not flood the console.
	void ForwardRenderer::reportFrameFailure(const char *stage, VkResult result) {
		static std::uint32_t reported{0U};
		constexpr std::uint32_t maxReports{16U};
		if (reported >= maxReports) { return; }
		++reported;
		std::cerr << "[vve::simple] frame skipped: " << stage << " failed with VkResult " << static_cast<std::int32_t>(result) << '\n';
	}

	/**
		* @brief Records the shadow passes and forward color pass for one acquired swapchain image.
		*
		* @param frameIndex Index selecting the per-frame command buffer and descriptor set.
		* @param imageIndex Index selecting the swapchain framebuffer.
		* @return VK_SUCCESS when command recording succeeds, otherwise the first failing Vulkan result.
		*/
	VkResult ForwardRenderer::recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex, const ForwardRendererShadowFrame &shadowFrame) {
		recordedPassOrder.clear();
		if (frameIndex >= commandBuffers.commandBuffers.size() || frameIndex >= descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
		if (imageIndex >= swapchain.images.size() || imageIndex >= imageViews.ownedViews.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
		if (shadowPipeline.pipeline == VK_NULL_HANDLE || pipelineLayout.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		const VkCommandBuffer commandBuffer{commandBuffers.commandBuffers[frameIndex]};
		VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
		if (result != VK_SUCCESS) { return result; }

		const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		if (result != VK_SUCCESS) { return result; }

		const auto drawUploadedObjects = [&](std::uint32_t shadowMatrixIndex, bool shadowPass) {
			std::size_t objectIndex{}; // Meshes and scene objects share submission order.
			for (const VulkanMesh &mesh : meshes) {
				if (objectIndex >= scene.objects.size()) { break; }

				const VkBuffer vertexBuffers[]{mesh.vertexBuffer.buffer};
				const VkDeviceSize offsets[]{0U};
				const Object &object = scene.objects[objectIndex];
				if (!object.visible || (shadowPass && !object.castsShadow)) { ++objectIndex; continue; } // Shadow passes skip non-casting objects such as the sun.
				const ObjectPushConstants pushConstants{.model = object.model, .baseColorTextureIndex = object.baseColorTextureIndex, .shadowMatrixIndex = shadowMatrixIndex, .unlit = object.unlit ? 1U : 0U};
				vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
				vkCmdPushConstants(commandBuffer, pipelineLayout.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(ObjectPushConstants), &pushConstants);
				vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
				++objectIndex;
			}
		};
		// Every shadow layer is cleared each frame; active layers also receive the caster geometry. All layers end in shader-read layout.
		const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}};
		const auto recordShadowLayer = [&](const ShadowMap &map, std::uint32_t layer, std::optional<std::uint32_t> matrixIndex, RecordedPass pass) {
			const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = layer, .layerCount = 1U};
			const VkImageMemoryBarrier beginBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = map.image,
				.subresourceRange = range,
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
										0U, 0U, nullptr, 0U, nullptr, 1U, &beginBarrier);
			const VkRenderingAttachmentInfo depthAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = map.layerViews[layer],
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = shadowClear,
			};
			const VkRenderingInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.layerCount = 1U,
				.colorAttachmentCount = 0U,
				.pDepthAttachment = &depthAttachment,
			};
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			if (matrixIndex) {
				recordedPassOrder.push_back(pass);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(*matrixIndex, true);
			}
			vkCmdEndRendering(commandBuffer);
			const VkImageMemoryBarrier readBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = map.image,
				.subresourceRange = range,
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										0U, 0U, nullptr, 0U, nullptr, 1U, &readBarrier);
		};
		const auto recordShadowMap = [&](const ShadowMap &map, std::size_t activeLayers, std::size_t matrixBase, RecordedPass pass) {
			for (std::uint32_t layer{}; layer < map.layerViews.size(); ++layer) {
				recordShadowLayer(map, layer, layer < activeLayers ? std::optional{static_cast<std::uint32_t>(matrixBase + layer)} : std::nullopt, pass);
			}
		};
		recordShadowMap(dirShadowArray, shadowFrame.activeDirectionalLightCount * kNumShadowCascades, kShadowMatrixDirBase, RecordedPass::directional_shadow);
		recordShadowMap(spotShadowArray, shadowFrame.activeSpotLightCount, kShadowMatrixSpotBase, RecordedPass::spot_shadow);
		recordShadowMap(pointShadowArray, shadowFrame.activePointLightCount * pointShadowFaceCount, kShadowMatrixPointBase, RecordedPass::point_shadow);

		constexpr std::array<float, 4U> skyBackgroundColor{0.45F, 0.70F, 1.00F, 1.00F}; // Sky background for the forward color pass.
		const VkImageSubresourceRange colorRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole swapchain image.
		const VkImageSubresourceRange depthRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole depth image.
		const std::array<VkImageMemoryBarrier, 2U> beginBarriers{{
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = swapchain.images[imageIndex],
				.subresourceRange = colorRange,
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = depthImage.image,
				.subresourceRange = depthRange,
			},
		}};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
									VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
									0U, 0U, nullptr, 0U, nullptr, static_cast<std::uint32_t>(beginBarriers.size()), beginBarriers.data());
		const VkRenderingAttachmentInfo colorAttachment{ // Dynamic rendering mirrors the old render-pass color clear/store ops.
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = imageViews.ownedViews[imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.color = {.float32 = {skyBackgroundColor[0], skyBackgroundColor[1], skyBackgroundColor[2], skyBackgroundColor[3]}}},
		};
		const VkRenderingAttachmentInfo depthAttachment{ // Forward depth is cleared to the far plane and kept attachment-local.
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = {.depthStencil = {.depth = 1.0F, .stencil = 0U}},
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
			.layerCount = 1U,
			.colorAttachmentCount = 1U,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
		};

		recordedPassOrder.push_back(RecordedPass::forward_color);
		vkCmdBeginRendering(commandBuffer, &renderingInfo);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
		drawUploadedObjects(0U, false);
		if (guiRecord_) { guiRecord_(commandBuffer); }

		vkCmdEndRendering(commandBuffer);
		const VkImageMemoryBarrier presentBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain.images[imageIndex],
			.subresourceRange = colorRange,
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &presentBarrier);
		result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS) { return result; }

		return VK_SUCCESS;
	}

} // namespace vve::simple
