module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief Vulkan renderer skeleton for the simple forward renderer.
	*
	* Functional objects:
	* - Renderer owns the current CPU scene, swapchain image stack, depth attachment, unbound shadow map and shadow pipeline, forward render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, frame command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes needed before rendering.
	*/
export namespace vve::simple {

	/// @brief Minimal forward renderer owning Vulkan swapchain bring-up without draw state.
	struct Renderer {
		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanDepthImage depthImage{};         ///< Owned swapchain-sized depth attachment image and view.
		ShadowMap shadowMap{};                 ///< Owned unbound shadow-map image reserved for later shadow rendering.
		VulkanRenderPass renderPass{};         ///< Owned forward render pass for swapchain color output.
		VulkanFramebuffers framebuffers{};     ///< Owned swapchain framebuffers for render-pass attachments.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::vector<VulkanMesh> meshes{};      ///< Owned GPU meshes uploaded from the current scene objects.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame readback capture.

		/**
			* @brief Initializes the Vulkan instance, device, swapchain, image views, depth attachment, shadow map, render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes.
			*
			* @param sdlWindow Borrowed SDL window that owns the native platform surface.
			* @return VK_SUCCESS after graphics-pipeline bring-up, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) {
			// CMake provides the binary shader directory so runtime loading follows the generated SPIR-V files.
			const std::string shaderDir{VVE_SIMPLE_SHADER_DIR};
			const std::string vertSpirvPath{shaderDir + "/simple_forward.vert.spv"};
			const std::string fragSpirvPath{shaderDir + "/simple_forward.frag.spv"};
			const std::string shadowVertSpirvPath{shaderDir + "/simple_forward.shadow.vert.spv"};

			window = sdlWindow;
			if (window == nullptr) { return VK_ERROR_INITIALIZATION_FAILED; }

			VkResult result = instance.create();
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = surface.create(instance.instance, window);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = physicalDevice.select(instance.instance, surface.surface);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = device.create(physicalDevice);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			int width{};
			int height{};
			SDL_GetWindowSizeInPixels(window, &width, &height);

			result = swapchain.create(
				physicalDevice.physicalDevice,
				device.device,
				surface.surface,
				*physicalDevice.graphicsQueueFamily,
				*physicalDevice.presentQueueFamily,
				static_cast<std::uint32_t>(width),
				static_cast<std::uint32_t>(height));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = imageViews.create(device.device, swapchain.images, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = depthImage.create(physicalDevice.physicalDevice, device.device, swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = shadowMap.create(physicalDevice.physicalDevice, device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = renderPass.create(device.device, swapchain.imageFormat);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = framebuffers.create(device.device, renderPass.renderPass, imageViews.views, swapchain.extent, depthImage.view);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorSetLayout.create(device.device);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = pipelineLayout.create(device.device, descriptorSetLayout.descriptorSetLayout);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			VulkanVertexInputDescription vertexInput{}; // Fixed mesh vertex layout shared by forward and shadow pipelines.
			result = shadowMap.createPipeline(descriptorSetLayout.descriptorSetLayout, shadowVertSpirvPath, vertexInput);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = vertShaderModule.create(device.device, vertSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = fragShaderModule.create(device.device, fragSpirvPath);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = graphicsPipeline.create(
				device.device,
				renderPass.renderPass,
				pipelineLayout.pipelineLayout,
				vertShaderModule.shaderModule,
				fragShaderModule.shaderModule,
				vertexInput,
				swapchain.extent);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandPool.create(device.device, *physicalDevice.graphicsQueueFamily);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = commandBuffers.create(device.device, commandPool.commandPool, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = frameSync.create(device.device, framesInFlight, static_cast<std::uint32_t>(swapchain.images.size()));
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = uniformBuffers.create(physicalDevice.physicalDevice, device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorPool.create(device.device, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			result = descriptorSets.create(device.device, descriptorPool.descriptorPool, descriptorSetLayout.descriptorSetLayout, framesInFlight);
			if (result != VK_SUCCESS) { cleanup(); return result; }

			// Bind each frame descriptor set to its matching uniform buffer and the already-created shadow map.
			for (std::uint32_t frame{}; frame < framesInFlight; ++frame) {
				result = descriptorSets.writeUniformBuffer(frame, uniformBuffers.buffers[frame].buffer, sizeof(FrameUniforms));
				if (result != VK_SUCCESS) { cleanup(); return result; }
				result = descriptorSets.writeShadowMap(frame, shadowMap.view, shadowMap.sampler);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			// Upload one GPU mesh for each object in the current CPU scene.
			for (const Object &object : scene.objects) {
				VulkanMesh &mesh = meshes.emplace_back();
				result = mesh.create(physicalDevice.physicalDevice, device.device, object.mesh);
				if (result != VK_SUCCESS) { cleanup(); return result; }
			}

			return VK_SUCCESS;
		}

		/**
			* @brief Replaces the current CPU scene before future renderer upload.
			*
			* @param nextScene Scene data prepared by the caller.
			*/
		void loadScene(Scene nextScene) { scene = std::move(nextScene); }

		/**
			* @brief Draws one swapchain frame through the per-frame synchronization objects.
			*/
		void drawFrame(VulkanReadback *readback = nullptr) {
			lastReadbackCaptureResult.reset();
			const std::size_t frameCount{frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
			if (frameCount == 0U || frameSync.imageAvailableSemaphores.size() < frameCount || frameSync.renderFinishedSemaphores.empty()) { return; }
			if (commandBuffers.commandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || swapchain.swapchain == VK_NULL_HANDLE) { return; }
			if (currentFrame >= frameCount) { currentFrame = 0U; }

			const VkFence inFlightFence{frameSync.inFlightFences[currentFrame]};
			const VkSemaphore imageAvailableSemaphore{frameSync.imageAvailableSemaphores[currentFrame]};
			if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return; }

			VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS) { return; }

			std::uint32_t imageIndex{};
			result = vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result != VK_SUCCESS) { return; }
			if (imageIndex >= frameSync.renderFinishedSemaphores.size()) { return; }
			const VkSemaphore renderFinishedSemaphore{frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.
			if (renderFinishedSemaphore == VK_NULL_HANDLE) { return; }

			result = vkResetFences(device.device, 1U, &inFlightFence);
			if (result != VK_SUCCESS) { return; }

			const Scalar aspectRatio{swapchain.extent.height == 0U ? one() : static_cast<Scalar>(swapchain.extent.width) / static_cast<Scalar>(swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
			const Vec3 lightDir{normalize(Vec3{static_cast<Scalar>(-0.5), static_cast<Scalar>(-1.0), static_cast<Scalar>(0.5)})}; ///< Matches the shader directional light.
			const Vec3 lightCenter{zeroVec3()}; ///< Origin-centered debug scene framing.
			const Vec3 lightEye{subtract(lightCenter, scale(lightDir, static_cast<Scalar>(8.0)))}; ///< Back up the light to enclose the floor and cube.
			const Scalar lightExtent{static_cast<Scalar>(4.0)}; ///< Light-space half-size covers the 4x4 floor and tall cube.
			const FrameUniforms frameUniforms{ // Shared camera matrices keep the sample cubes inside Vulkan clip space.
				.view = lookAt(Vec3{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}, Vec3{zero(), one(), zero()}, Vec3{zero(), one(), zero()}),
				.projection = perspectiveVulkan(static_cast<Scalar>(0.7853981633974483), aspectRatio, static_cast<Scalar>(0.1), static_cast<Scalar>(100.0)),
				.lightViewProj = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(lightEye, lightCenter, Vec3{zero(), one(), zero()})),
			};
			result = uniformBuffers.update(currentFrame, frameUniforms);
			if (result != VK_SUCCESS) { return; }

			result = recordCommandBuffer(currentFrame, imageIndex);
			if (result != VK_SUCCESS) { return; }

			const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
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
			if (result != VK_SUCCESS) { return; }
			if (readback != nullptr && imageIndex < swapchain.images.size()) {
				lastReadbackCaptureResult = readback->capture(swapchain.images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			}

			const VkPresentInfoKHR presentInfo{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &renderFinishedSemaphore,
				.swapchainCount = 1U,
				.pSwapchains = &swapchain.swapchain,
				.pImageIndices = &imageIndex,
			};
			result = vkQueuePresentKHR(device.presentQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result != VK_SUCCESS) { return; }

			lastRenderedImageIndex = imageIndex;
			currentFrame = static_cast<std::uint32_t>((currentFrame + 1U) % frameCount);
		}

		/**
		* @brief Releases Vulkan device resources in reverse creation order.
			*/
		void cleanup() {
			for (auto mesh = meshes.rbegin(); mesh != meshes.rend(); ++mesh) { mesh->cleanup(); }
			meshes.clear();
			descriptorSets.cleanup();
			descriptorPool.cleanup();
			uniformBuffers.cleanup();
			frameSync.cleanup();
			commandBuffers.cleanup();
			commandPool.cleanup();
			graphicsPipeline.cleanup();
			fragShaderModule.cleanup();
			vertShaderModule.cleanup();
			shadowMap.cleanupPipeline();
			pipelineLayout.cleanup();
			descriptorSetLayout.cleanup();
			framebuffers.cleanup();
			renderPass.cleanup();
			shadowMap.cleanup();
			depthImage.cleanup();
			imageViews.cleanup();
			swapchain.cleanup();
			device.cleanup();
			surface.cleanup();
			instance.cleanup();
			window = nullptr;
		}

	private:
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.

		/**
			* @brief Records one forward render pass for a swapchain image into a frame command buffer.
			*
			* @param frameIndex Index selecting the per-frame command buffer and descriptor set.
			* @param imageIndex Index selecting the swapchain framebuffer.
			* @return VK_SUCCESS when command recording succeeds, otherwise the first failing Vulkan result.
			*/
		[[nodiscard]] VkResult recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex) {
			if (frameIndex >= commandBuffers.commandBuffers.size() || frameIndex >= descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (imageIndex >= framebuffers.framebuffers.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (shadowMap.renderPass == VK_NULL_HANDLE || shadowMap.framebuffer == VK_NULL_HANDLE || shadowMap.pipeline == VK_NULL_HANDLE || shadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandBuffer commandBuffer{commandBuffers.commandBuffers[frameIndex]};
			VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
			if (result != VK_SUCCESS) { return result; }

			const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			const auto drawUploadedObjects = [&](VkPipelineLayout activePipelineLayout) {
				std::size_t objectIndex{}; // Meshes and scene objects share submission order.
				for (const VulkanMesh &mesh : meshes) {
					if (objectIndex >= scene.objects.size()) { break; }

					const VkBuffer vertexBuffers[]{mesh.vertexBuffer.buffer};
					const VkDeviceSize offsets[]{0U};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					vkCmdPushConstants(commandBuffer, activePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0U, sizeof(float) * 16U, &scene.objects[objectIndex].model);
					vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
					++objectIndex;
				}
			};
			const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}}; // Shadow depth starts at the far plane.
			const VkRenderPassBeginInfo shadowPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = shadowMap.renderPass,
				.framebuffer = shadowMap.framebuffer,
				.renderArea = {.offset = {0, 0}, .extent = {.width = ShadowMap::resolution, .height = ShadowMap::resolution}},
				.clearValueCount = 1U,
				.pClearValues = &shadowClear,
			};

			vkCmdBeginRenderPass(commandBuffer, &shadowPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMap.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(shadowMap.pipelineLayout);
			vkCmdEndRenderPass(commandBuffer);

			const std::array<VkClearValue, 2U> clearValues{{{.color = {.float32 = {0.0F, 0.0F, 0.0F, 1.0F}}}, {.depthStencil = {.depth = 1.0F, .stencil = 0U}}}}; // Index 1 clears depth to the far plane.
			const VkRenderPassBeginInfo renderPassInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = renderPass.renderPass,
				.framebuffer = framebuffers.framebuffers[imageIndex],
				.renderArea = {.offset = {0, 0}, .extent = swapchain.extent},
				.clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
				.pClearValues = clearValues.data(),
			};

			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(pipelineLayout.pipelineLayout);

			vkCmdEndRenderPass(commandBuffer);
			result = vkEndCommandBuffer(commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			return VK_SUCCESS;
		}
	};

} // namespace vve::simple
