module;
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer:RendererDraw;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;
import :RendererDebug;
import :RendererShadowPrep;
import :RendererResources;

/**
	* @file
	* @brief Per-frame command-buffer recording and queue presentation for the simple forward renderer.
	*
	* Functional objects:
	* - ForwardRendererDraw records shadow and forward color commands, updates frame uniforms, submits the frame, performs optional readback, and presents the acquired swapchain image.
	*/
export namespace vve::simple {

	/// @brief CRTP mixin that owns ForwardRenderer frame recording, submission, and presentation logic.
	template<typename Renderer>
	struct ForwardRendererDraw {
		/**
			* @brief Draws one swapchain frame through the per-frame synchronization objects.
			*
			* @param readback Optional swapchain-image readback sink used by deterministic debug captures.
		*/
		void drawFrame(VulkanReadback *readback = nullptr) {
			auto &renderer = static_cast<Renderer &>(*this);
			renderer.lastReadbackCaptureResult.reset();
			const auto windowExtent = renderer.currentWindowPixelExtent();
			if (windowExtent.width == 0U || windowExtent.height == 0U) { return; }
			if (const VkResult result = renderer.syncSceneResources(); result != VK_SUCCESS) { reportFrameFailure("scene sync", result); return; }
			// Compare against the extent the swapchain was requested for, not the surface-chosen one, so a driver that clamps or reports a different currentExtent does not force a recreate every frame.
			if (windowExtent.width != renderer.swapchain.requestedExtent.width || windowExtent.height != renderer.swapchain.requestedExtent.height) {
				if (const VkResult result = renderer.recreateSwapchain(windowExtent); result != VK_SUCCESS) { reportFrameFailure("swapchain recreate", result); return; }
			}

			const std::size_t frameCount{renderer.frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
			if (frameCount == 0U || renderer.frameSync.imageAvailableSemaphores.size() < frameCount || renderer.frameSync.renderFinishedSemaphores.empty()) { return; }
			if (renderer.commandBuffers.commandBuffers.size() < frameCount || renderer.device.device == VK_NULL_HANDLE || renderer.swapchain.swapchain == VK_NULL_HANDLE) { return; }
			if (renderer.currentFrame >= frameCount) { renderer.currentFrame = 0U; }

			const VkFence inFlightFence{renderer.frameSync.inFlightFences[renderer.currentFrame]};
			const VkSemaphore imageAvailableSemaphore{renderer.frameSync.imageAvailableSemaphores[renderer.currentFrame]};
			if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return; }

			VkResult result = vkWaitForFences(renderer.device.device, 1U, &inFlightFence, VK_TRUE, UINT64_MAX);
			if (result != VK_SUCCESS) { reportFrameFailure("fence wait", result); return; }

			std::uint32_t imageIndex{};
			result = vkAcquireNextImageKHR(renderer.device.device, renderer.swapchain.swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
			if (result == VK_ERROR_OUT_OF_DATE_KHR) { (void)renderer.recreateSwapchain(renderer.currentWindowPixelExtent()); return; }
			// VK_SUBOPTIMAL_KHR still acquired an image: it must be rendered and presented, otherwise the swapchain runs out of images and the next acquire blocks forever.
			if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { reportFrameFailure("image acquire", result); return; }
			if (imageIndex >= renderer.frameSync.renderFinishedSemaphores.size()) { return; }
			const VkSemaphore renderFinishedSemaphore{renderer.frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.
			if (renderFinishedSemaphore == VK_NULL_HANDLE) { return; }

			const Scalar aspectRatio{renderer.swapchain.extent.height == 0U ? one() : static_cast<Scalar>(renderer.swapchain.extent.width) / static_cast<Scalar>(renderer.swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
			const PointLight light{renderer.scene.pointLight};
			const DirectionalLight dirLight{renderer.scene.directionalLight}; ///< Directional light data uploaded for future shader use.
			const SpotLight spot{renderer.scene.spotLight}; ///< Spot light data uploaded for future shader use.
			const Vec3 dirLightDirection{normalize(dirLight.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 spotDirection{normalize(spot.direction)}; ///< Normalized world-space direction keeps uniform packing stable.
			const Vec3 lightCenter{zeroVec3()}; ///< Origin-centered legacy point-shadow framing.
			const Vec3 shadowSurfaceLightDir{normalize(subtract(light.position, lightCenter))}; ///< Point-light direction approximated by one shadow map.
			const Vec3 lightEye{light.position}; ///< Place the shadow camera at the point light for the current simple approximation.
			const Scalar lightExtent{static_cast<Scalar>(4.0)}; ///< Light-space half-size covers the 4x4 floor and tall cube.
			constexpr Scalar cameraVerticalFov{static_cast<Scalar>(0.7853981633974483)}; ///< Fixed 45-degree camera field of view.
			constexpr Scalar cameraNear{static_cast<Scalar>(0.1)}; ///< Camera near plane shared by projection and cascade splitting.
			constexpr Scalar cameraFar{static_cast<Scalar>(100.0)}; ///< Camera far plane bounds directional cascade coverage.
			const Mat4 cameraView{lookAt(renderer.cameraEye, renderer.cameraTarget, Vec3{zero(), one(), zero()})}; ///< Current camera transform shared by uniforms and cascade fitting.
			const ForwardRendererShadowFrame shadowFrame =
				renderer.prepareShadowFrame(cameraView, cameraVerticalFov, aspectRatio, cameraNear, cameraFar, spot, spotDirection);
#ifndef NDEBUG
			renderer.spotShadowDepthSampleCountStorage() = renderer.spotLightViewProjCount;
			const Vec3 spotShadowDebugPoint{zero(), zero(), zero()}; ///< Fixed world point used for CPU-only shadow diagnostics.
			constexpr float kCpuSpotShadowFactor{1.0F}; ///< Unshadowed placeholder because has_gpu is false here.
			renderer.pointShadowDepthSampleCountStorage() = shadowFrame.pointLightShadowCount;
			constexpr float kCpuPointShadowFactor{1.0F}; ///< Unshadowed placeholder because has_gpu is false here.
			// Keep one CPU-only point sample per active point light using the shader's dominant-axis face order.
			for (std::size_t pointIndex{}; pointIndex < shadowFrame.pointLightShadowCount; ++pointIndex) {
				const PointLight &activePoint = renderer.scene.pointLights[pointIndex]; ///< Source point light for this diagnostic slot.
				const Vec3 lightToPoint{subtract(spotShadowDebugPoint, activePoint.position)}; ///< Shader-matching vector from light to sample.
				const Vec3 absLightToPoint{std::abs(lightToPoint.x), std::abs(lightToPoint.y), std::abs(lightToPoint.z)}; ///< Dominant-axis selector inputs.
				const std::uint32_t faceIndex{absLightToPoint.x >= absLightToPoint.y && absLightToPoint.x >= absLightToPoint.z
																	? (lightToPoint.x >= zero() ? 0U : 1U)
																	: (absLightToPoint.y >= absLightToPoint.z ? (lightToPoint.y >= zero() ? 2U : 3U)
																													  : (lightToPoint.z >= zero() ? 4U : 5U))}; ///< Face order: +X, -X, +Y, -Y, +Z, -Z.
				const std::uint32_t pointFaceLayer{static_cast<std::uint32_t>(pointIndex * ForwardRendererShadowPrep<Renderer>::pointShadowFaceCount + faceIndex)}; ///< Dense point-face uniform index.
				const std::uint32_t selectedLayer{shadowFrame.firstPointShadowLayer + pointFaceLayer}; ///< Dense depth-array layer after spot slots.
				const Vec4 lightClip{multiply(shadowFrame.pointLightFaceViewProjs[pointFaceLayer], Vec4{spotShadowDebugPoint.x, spotShadowDebugPoint.y, spotShadowDebugPoint.z, one()})}; ///< Clip point before perspective divide.
				const Scalar invW{std::abs(lightClip.w) > static_cast<Scalar>(0.001) ? one() / lightClip.w : static_cast<Scalar>(1000.0)}; ///< Shader-equivalent small-w guard.
				const Vec3 lightNdc{lightClip.x * invW, lightClip.y * invW, lightClip.z * invW}; ///< Point-face NDC point used for CPU comparison.
				renderer.pointShadowDepthSampleStorage()[pointIndex] = RenderShadowDepthSample{.triangle_id = static_cast<std::uint32_t>(pointIndex),
																									  .face_index = faceIndex,
																									  .world = spotShadowDebugPoint,
																									  .light_ndc = lightNdc,
																									  .pixel_x = selectedLayer,
																									  .expected_depth = lightNdc.z,
																									  .bias = shadowFrame.shadowCompareBias,
																									  .shadow_factor = kCpuPointShadowFactor,
																									  .gpu_depth = -1.0F,
																									  .error = -1.0F,
																									  .has_gpu = false,
																									  .valid = true}; ///< Slot is triangle_id; layer is pixel_x until point readback exists.
			}
			for (std::size_t spotIndex{}; spotIndex < renderer.spotLightViewProjCount; ++spotIndex) {
				const Vec4 lightClip{multiply(renderer.spotLightViewProjs[spotIndex], Vec4{spotShadowDebugPoint.x, spotShadowDebugPoint.y, spotShadowDebugPoint.z, one()})}; ///< Clip point before perspective divide.
				const Scalar invW{lightClip.w != zero() ? one() / lightClip.w : zero()}; ///< Zero-w guard keeps invalid clips explicit.
				const Vec3 lightNdc{lightClip.x * invW, lightClip.y * invW, lightClip.z * invW}; ///< Shader-comparable NDC point.
				renderer.spotShadowDepthSampleStorage()[spotIndex] = RenderShadowDepthSample{.face_index = static_cast<std::uint32_t>(spotIndex),
																								 .world = spotShadowDebugPoint,
																								 .light_ndc = lightNdc,
																								 .expected_depth = lightNdc.z,
																								 .bias = shadowFrame.shadowCompareBias,
																								 .shadow_factor = kCpuSpotShadowFactor,
																								 .gpu_depth = -1.0F,
																								 .error = -1.0F,
																								 .has_gpu = false,
																								 .valid = true}; ///< Slot is face_index; GPU depth is unavailable here.
			}
#endif
			const FrameUniforms frameUniforms{ // Shared camera matrices keep the sample cubes inside Vulkan clip space.
				.view = cameraView,
				.projection = perspectiveVulkan(cameraVerticalFov, aspectRatio, cameraNear, cameraFar),
				.lightViewProj = multiply(orthoVulkan(-lightExtent, lightExtent, -lightExtent, lightExtent, static_cast<Scalar>(0.1), static_cast<Scalar>(16.0)), lookAt(lightEye, lightCenter, Vec3{zero(), one(), zero()})),
				.dirLightViewProjArray = shadowFrame.dirLightViewProjArray, ///< Flattened directional cascade matrices used by depth passes.
				.cascadeSplits = shadowFrame.cascadeSplitsFar, ///< View-space cascade far distances mirror the Slang float4.
				.spotLightViewProjs = renderer.spotLightViewProjs,
				.pointLightFaceViewProjs = shadowFrame.pointLightFaceViewProjs, ///< Point-face matrices are sourced from shadow metadata rows.
				.pointLightPositionRanges = shadowFrame.pointLightPositionRanges,
				.pointLightColorIntensities = shadowFrame.pointLightColorIntensities,
				.lightPositionRange = Vec4{light.position.x, light.position.y, light.position.z, light.range},
				.lightColorIntensity = Vec4{light.color.x, light.color.y, light.color.z, light.intensity},
				.lightShadowAmbient = Vec4{shadowSurfaceLightDir.x, shadowSurfaceLightDir.y, shadowSurfaceLightDir.z, light.ambient},
				.dirLightDirection = Vec4{dirLightDirection.x, dirLightDirection.y, dirLightDirection.z, zero()}, ///< Directional light vector with unused w.
				.dirLightColorIntensity = Vec4{dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.intensity.value}, ///< Directional tint with intensity in w.
				.dirLightShadowAmbient = Vec4{zero(), zero(), zero(), dirLight.ambient}, ///< Directional ambient term packed in w.
				.spotLightPositionRange = shadowFrame.spotLightPositionRange,
				.spotLightColorIntensity = shadowFrame.spotLightColorIntensity,
				.spotLightDirection = shadowFrame.spotLightDirection,
				.spotLightConeAmbient = shadowFrame.spotLightConeAmbient,
				.spotLightPositionRanges = shadowFrame.spotLightPositionRanges,
				.spotLightColorIntensities = shadowFrame.spotLightColorIntensities,
				.spotLightDirections = shadowFrame.spotLightDirections,
				.spotLightConeAmbients = shadowFrame.spotLightConeAmbients,
				.directionalLightDirections = shadowFrame.directionalLightDirections,
				.directionalLightColorIntensities = shadowFrame.directionalLightColorIntensities,
				.directionalLightAmbients = shadowFrame.directionalLightAmbients,
				.activeDirectionalLightCount = shadowFrame.activeDirectionalLightCount,
			};
#ifndef NDEBUG
			constexpr std::size_t kDirectionalDebugLightIndex{0U}; ///< Directional diagnostics stay pinned to the first packed light.
			constexpr std::size_t kDirectionalDebugCascadeIndex{0U}; ///< Directional diagnostics sample the nearest cascade only.
			constexpr std::size_t kDirectionalDebugLayer{kDirectionalDebugLightIndex * kNumShadowCascades + kDirectionalDebugCascadeIndex}; ///< Flattened light-zero cascade-zero layer.
			const auto directionalDebugMeta = std::ranges::find_if(renderer.shadowLightMeta, [](const auto &meta) {
				return meta.light_type == 3U && meta.light_index == kDirectionalDebugLightIndex && meta.first_layer == kDirectionalDebugLayer;
			}); ///< Metadata order also contains spot and point rows, so select by directional identity.
			renderer.directionalShadowDepthSampleCountStorage() = directionalDebugMeta != renderer.shadowLightMeta.end() ? 1U : 0U;
			const Vec3 directionalShadowDebugPoint{zero(), zero(), zero()}; ///< Fixed world point shared with spot shadow diagnostics.
			constexpr float kDirectionalShadowCompareBias{0.00005F * static_cast<float>(kDirectionalDebugCascadeIndex + 1U)}; ///< CPU mirror of the selected cascade's shader-side compare bias.
			const Vec4 dirLightClip{multiply(frameUniforms.dirLightViewProjArray[kDirectionalDebugLayer], Vec4{directionalShadowDebugPoint.x, directionalShadowDebugPoint.y, directionalShadowDebugPoint.z, one()})}; ///< Clip point in light-zero cascade zero before perspective divide.
			const Scalar dirInvW{dirLightClip.w != zero() ? one() / dirLightClip.w : zero()}; ///< Zero-w guard matches the spot debug path.
			const Vec3 dirLightNdc{dirLightClip.x * dirInvW, dirLightClip.y * dirInvW, dirLightClip.z * dirInvW}; ///< Shader-comparable directional NDC point.
			renderer.directionalShadowDepthSampleStorage()[0] = RenderShadowDepthSample{.face_index = static_cast<std::uint32_t>(kDirectionalDebugLayer),
																									 .world = directionalShadowDebugPoint,
																									 .light_ndc = dirLightNdc,
																									 .expected_depth = dirLightNdc.z,
																									 .bias = kDirectionalShadowCompareBias,
																									 .shadow_factor = 1.0F,
																		 .gpu_depth = -1.0F,
																		 .error = -1.0F,
																		 .has_gpu = false,
																		 .valid = directionalDebugMeta != renderer.shadowLightMeta.end()}; ///< Sample mirrors light-zero cascade-zero metadata and depth data.
#endif
			result = renderer.uniformBuffers.update(renderer.currentFrame, frameUniforms);
			if (result != VK_SUCCESS) { reportFrameFailure("uniform update", result); return; }

			result = recordCommandBuffer(renderer.currentFrame, imageIndex);
			if (result != VK_SUCCESS) { reportFrameFailure("command recording", result); return; }

			// Reset the fence only once the submit is certain; an unsignaled fence without a submit would block the next frame forever.
			result = vkResetFences(renderer.device.device, 1U, &inFlightFence);
			if (result != VK_SUCCESS) { reportFrameFailure("fence reset", result); return; }

			const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT}; ///< Acquire completes before the swapchain layout transition executes.
			const VkSubmitInfo submitInfo{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &imageAvailableSemaphore,
				.pWaitDstStageMask = &waitStage,
				.commandBufferCount = 1U,
				.pCommandBuffers = &renderer.commandBuffers.commandBuffers[renderer.currentFrame],
				.signalSemaphoreCount = 1U,
				.pSignalSemaphores = &renderFinishedSemaphore,
			};
			result = vkQueueSubmit(renderer.device.graphicsQueue, 1U, &submitInfo, inFlightFence);
			if (result != VK_SUCCESS) { reportFrameFailure("queue submit", result); return; }
			if (renderer.gpuDebugReadbackEnabled()) {
				if (renderer.spotLightViewProjCount != 0U) { renderer.fillSpotShadowGpuDepthSamples(); }
				if (renderer.directionalShadowDepthSampleCountStorage() != 0U) { renderer.fillDirectionalShadowGpuDepthSamples(); }
				if (renderer.pointShadowDepthSampleCountStorage() != 0U) { renderer.fillPointShadowGpuDepthSamples(); }
			}
			if (readback != nullptr && imageIndex < renderer.swapchain.images.size()) {
				renderer.lastReadbackCaptureResult = readback->capture(renderer.swapchain.images[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			}

			VkSwapchainKHR presentSwapchain = renderer.swapchain.swapchain;
			const VkPresentInfoKHR presentInfo{
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &renderFinishedSemaphore,
				.swapchainCount = 1U,
				.pSwapchains = &presentSwapchain,
				.pImageIndices = &imageIndex,
			};
			result = vkQueuePresentKHR(renderer.device.presentQueue, &presentInfo);
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				(void)renderer.recreateSwapchain(renderer.currentWindowPixelExtent());
				return;
			}
			if (result != VK_SUCCESS) { reportFrameFailure("present", result); return; }

			renderer.lastRenderedImageIndex = imageIndex;
			renderer.currentFrame = static_cast<std::uint32_t>((renderer.currentFrame + 1U) % frameCount);
		}

	private:
		/// @brief Logs a skipped frame with its Vulkan result; capped so a persistent failure does not flood the console.
		static void reportFrameFailure(const char *stage, VkResult result) {
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
		[[nodiscard]] VkResult recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex) {
			auto &renderer = static_cast<Renderer &>(*this);
			renderer.recordedPassOrder.clear();
			if (frameIndex >= renderer.commandBuffers.commandBuffers.size() || frameIndex >= renderer.descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (imageIndex >= renderer.swapchain.images.size() || imageIndex >= renderer.imageViews.ownedViews.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (renderer.shadowMap.imageView == VK_NULL_HANDLE || renderer.shadowMap.pipeline == VK_NULL_HANDLE || renderer.shadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }
			if (renderer.spotShadowMap.imageView == VK_NULL_HANDLE || renderer.spotShadowMap.pipeline == VK_NULL_HANDLE || renderer.spotShadowMap.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

			const VkCommandBuffer commandBuffer{renderer.commandBuffers.commandBuffers[frameIndex]};
			VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
			if (result != VK_SUCCESS) { return result; }

			const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
			result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			if (result != VK_SUCCESS) { return result; }

			const auto drawUploadedObjects = [&](VkPipelineLayout activePipelineLayout, std::uint32_t spotLightIndex = 0U, std::uint32_t dirLightIndex = 0U, bool shadowPass = false) {
				std::size_t objectIndex{}; // Meshes and scene objects share submission order.
				for (const VulkanMesh &mesh : renderer.meshes) {
					if (objectIndex >= renderer.scene.objects.size()) { break; }

					const VkBuffer vertexBuffers[]{mesh.vertexBuffer.buffer};
					const VkDeviceSize offsets[]{0U};
					const Object &object = renderer.scene.objects[objectIndex];
					if (!object.visible || (shadowPass && !object.castsShadow)) { ++objectIndex; continue; } // Shadow passes skip non-casting objects such as the sun.
					const ObjectPushConstants pushConstants{.model = object.model, .baseColorTextureIndex = object.baseColorTextureIndex, .spotLightIndex = spotLightIndex, .dirLightIndex = dirLightIndex, .unlit = object.unlit ? 1U : 0U};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					vkCmdPushConstants(commandBuffer, activePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(ObjectPushConstants), &pushConstants);
					vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1U, 0U, 0, 0U);
					++objectIndex;
				}
			};
			const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}}; // Shadow depth starts at the far plane.
			const VkExtent2D shadowExtent{.width = ShadowMap::resolution, .height = ShadowMap::resolution}; // Legacy single-map pass extent.
			const VkImageSubresourceRange shadowRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole single-layer shadow image.
			const VkRenderingAttachmentInfo shadowDepthAttachment{ // Dynamic shadow pass keeps old clear/store behavior.
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = renderer.shadowMap.imageView,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = shadowClear,
			};
			const VkRenderingInfo shadowRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
				.layerCount = 1U,
				.colorAttachmentCount = 0U,
				.pDepthAttachment = &shadowDepthAttachment,
			};
			const VkRenderingAttachmentInfo spotShadowDepthAttachment{ // Dynamic spot shadow pass keeps old clear/store behavior.
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = renderer.spotShadowMap.imageView,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = shadowClear,
			};
			const VkRenderingInfo spotShadowRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
				.layerCount = 1U,
				.colorAttachmentCount = 0U,
				.pDepthAttachment = &spotShadowDepthAttachment,
			};

			const bool legacyDirectionalShadowEnabled{!renderer.scene.directionalLights.empty() ? renderer.scene.directionalLights.front().enabled : renderer.scene.directionalLight.enabled}; // Legacy single shadow map mirrors the first directional light.
			if (legacyDirectionalShadowEnabled) {
				const VkImageMemoryBarrier shadowBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.shadowMap.image,
					.subresourceRange = shadowRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &shadowBeginBarrier);
				renderer.recordedPassOrder.push_back(Renderer::RecordedPass::directional_shadow);
				vkCmdBeginRendering(commandBuffer, &shadowRenderingInfo);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.shadowMap.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.shadowMap.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(renderer.shadowMap.pipelineLayout, 0U, 0U, true);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier shadowReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.shadowMap.image,
					.subresourceRange = shadowRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &shadowReadBarrier);
			}

			const std::size_t directionalLightCount{std::min(renderer.scene.directionalLights.size(), kMaxDirectionalLights)}; // Clamped directional-light count mirrors frame uniform upload.
			const std::size_t dirShadowArrayLayerCount{renderer.dirShadowArray.ownedLayerViews.size()}; // Every sampled directional layer must reach shader-read layout.
			// Clear each allocated directional layer before any draw binds the descriptor set that exposes the whole array.
			for (std::size_t dirLightIndex{}; dirLightIndex < dirShadowArrayLayerCount; ++dirLightIndex) {
				const VkImageSubresourceRange dirShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(dirLightIndex), .layerCount = 1U}; // One 2D view owns one array layer.
				const VkImageMemoryBarrier dirShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.dirShadowArray.image,
					.subresourceRange = dirShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo dirShadowArrayDepthAttachment{ // Dynamic clear pass defines the sampled array layer.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.dirShadowArray.ownedLayerViews[dirLightIndex],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo dirShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &dirShadowArrayDepthAttachment,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &dirShadowArrayBeginBarrier);
				vkCmdBeginRendering(commandBuffer, &dirShadowArrayRenderingInfo);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier dirShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.dirShadowArray.image,
					.subresourceRange = dirShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &dirShadowArrayReadBarrier);
			}

			std::size_t activeDirectionalShadowPassCount{}; // Enabled directional lights are packed into dense shadow layers by drawFrame().
			for (std::size_t dirLightIndex{}; dirLightIndex < directionalLightCount; ++dirLightIndex) { if (renderer.scene.directionalLights[dirLightIndex].enabled) { ++activeDirectionalShadowPassCount; } }
			const std::size_t dirShadowArrayPassCount{std::min(activeDirectionalShadowPassCount * kNumShadowCascades, dirShadowArrayLayerCount)}; // Every active directional light renders all four cascades.
			// Render flattened light-cascade layers after the clear loop has defined the whole sampled array.
			for (std::size_t passIndex{}; passIndex < dirShadowArrayPassCount; ++passIndex) {
				const std::size_t packedDirectionalIndex{passIndex / kNumShadowCascades}; ///< Dense enabled-light index shared with CPU preparation.
				const std::size_t cascadeIndex{passIndex % kNumShadowCascades}; ///< Local cascade index inside the packed light.
				const std::size_t dirShadowLayer{packedDirectionalIndex * kNumShadowCascades + cascadeIndex}; ///< Flattened matrix and image-array layer index.
				const VkImageSubresourceRange dirShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(dirShadowLayer), .layerCount = 1U}; // Active layer index matches the packed cascade data.
				const VkImageMemoryBarrier dirShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.dirShadowArray.image,
					.subresourceRange = dirShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo dirShadowArrayDepthAttachment{ // Dynamic draw pass keeps the legacy clear-before-draw behavior.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.dirShadowArray.ownedLayerViews[dirShadowLayer],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo dirShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &dirShadowArrayDepthAttachment,
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &dirShadowArrayBeginBarrier);
				renderer.recordedPassOrder.push_back(Renderer::RecordedPass::directional_shadow);
				vkCmdBeginRendering(commandBuffer, &dirShadowArrayRenderingInfo);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.dirShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.dirShadowArray.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(renderer.dirShadowArray.pipelineLayout, 0U, static_cast<std::uint32_t>(dirShadowLayer), true);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier dirShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.dirShadowArray.image,
					.subresourceRange = dirShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &dirShadowArrayReadBarrier);
			}

			const bool legacySpotShadowEnabled{!renderer.scene.spotLights.empty() ? renderer.scene.spotLights.front().enabled : renderer.scene.spotLight.enabled}; // Legacy single shadow map mirrors the first spot light.
			if (legacySpotShadowEnabled) {
				const VkImageMemoryBarrier spotShadowBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowMap.image,
					.subresourceRange = shadowRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowBeginBarrier);
				renderer.recordedPassOrder.push_back(Renderer::RecordedPass::spot_shadow);
				vkCmdBeginRendering(commandBuffer, &spotShadowRenderingInfo);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.spotShadowMap.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.spotShadowMap.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(renderer.spotShadowMap.pipelineLayout, 0U, 0U, true);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier spotShadowReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowMap.image,
					.subresourceRange = shadowRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowReadBarrier);
			}

			const std::size_t spotShadowArrayLayerCount{renderer.spotShadowArray.ownedLayerViews.size()}; // Every sampled array layer must reach shader-read layout.
			// Clear each allocated spot layer before any draw binds the descriptor set that exposes the whole array.
			for (std::size_t spotLightIndex{}; spotLightIndex < spotShadowArrayLayerCount; ++spotLightIndex) {
				const VkImageSubresourceRange spotShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(spotLightIndex), .layerCount = 1U}; // One 2D view owns one array layer.
				const VkImageMemoryBarrier spotShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowArray.image,
					.subresourceRange = spotShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo spotShadowArrayDepthAttachment{ // Dynamic clear pass defines the sampled array layer.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.spotShadowArray.ownedLayerViews[spotLightIndex],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo spotShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &spotShadowArrayDepthAttachment,
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowArrayBeginBarrier);
				vkCmdBeginRendering(commandBuffer, &spotShadowArrayRenderingInfo);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier spotShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowArray.image,
					.subresourceRange = spotShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowArrayReadBarrier);
			}

			std::size_t activeSpotShadowPassCount{}; // Enabled spot lights are packed into dense shadow layers by drawFrame().
			for (std::size_t spotLightIndex{}; spotLightIndex < renderer.spotLightViewProjCount; ++spotLightIndex) { if (renderer.scene.spotLights[spotLightIndex].enabled) { ++activeSpotShadowPassCount; } }
			const std::size_t spotShadowArrayPassCount{std::min(activeSpotShadowPassCount, spotShadowArrayLayerCount)}; // Active array layers receive one geometry depth pass each.
			// Render active spot lights after all sampled array layers have a defined layout.
			for (std::size_t spotLightIndex{}; spotLightIndex < spotShadowArrayPassCount; ++spotLightIndex) {
				const VkImageSubresourceRange spotShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(spotLightIndex), .layerCount = 1U}; // Active layer index matches the packed shadow data.
				const VkImageMemoryBarrier spotShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowArray.image,
					.subresourceRange = spotShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo spotShadowArrayDepthAttachment{ // Dynamic draw pass keeps the legacy clear-before-draw behavior.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.spotShadowArray.ownedLayerViews[spotLightIndex],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo spotShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &spotShadowArrayDepthAttachment,
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowArrayBeginBarrier);
				renderer.recordedPassOrder.push_back(Renderer::RecordedPass::spot_shadow);
				vkCmdBeginRendering(commandBuffer, &spotShadowArrayRenderingInfo);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.spotShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.spotShadowArray.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(renderer.spotShadowArray.pipelineLayout, static_cast<std::uint32_t>(spotLightIndex), 0U, true);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier spotShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.spotShadowArray.image,
					.subresourceRange = spotShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &spotShadowArrayReadBarrier);
			}

			const std::size_t pointShadowArrayLayerCount{renderer.pointShadowArray.ownedLayerViews.size()}; // Six point-shadow faces per light must reach shader-read layout.
			// Clear each allocated point-shadow face layer before the color pass starts.
			for (std::size_t pointShadowLayerIndex{}; pointShadowLayerIndex < pointShadowArrayLayerCount; ++pointShadowLayerIndex) {
				const VkImageSubresourceRange pointShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(pointShadowLayerIndex), .layerCount = 1U}; // One 2D view owns one cube-face layer.
				const VkImageMemoryBarrier pointShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.pointShadowArray.image,
					.subresourceRange = pointShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo pointShadowArrayDepthAttachment{ // Dynamic clear pass defines the sampled cube-face layer.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.pointShadowArray.ownedLayerViews[pointShadowLayerIndex],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo pointShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &pointShadowArrayDepthAttachment,
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &pointShadowArrayBeginBarrier);
				vkCmdBeginRendering(commandBuffer, &pointShadowArrayRenderingInfo);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier pointShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.pointShadowArray.image,
					.subresourceRange = pointShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &pointShadowArrayReadBarrier);
			}

			constexpr std::size_t pointShadowFaceCount{6U}; // One point light owns six cubemap-style shadow faces.
			const std::size_t pointShadowLightCount{std::min(renderer.scene.pointLights.size(), kMaxShadowedPointLights)}; // Fixed point-light cap keeps the array small.
			std::size_t activePointShadowLightCount{}; // Enabled point lights are packed into dense cubemap-style layers by drawFrame().
			for (std::size_t pointIndex{}; pointIndex < pointShadowLightCount; ++pointIndex) { if (renderer.scene.pointLights[pointIndex].enabled) { ++activePointShadowLightCount; } }
			const std::size_t activePointFaceCount{activePointShadowLightCount * pointShadowFaceCount}; // Active faces use dense pointIndex * 6 + faceIndex layers.
			// Render active point-light faces after every point layer has a defined shader-read transition path.
			for (std::size_t faceGlobalIndex{}; faceGlobalIndex < activePointFaceCount; ++faceGlobalIndex) {
				const std::size_t pointIndex{faceGlobalIndex / pointShadowFaceCount}; // Source point light for this face.
				const std::size_t faceIndex{faceGlobalIndex % pointShadowFaceCount}; // Local cubemap-style face index.
				const std::size_t pointFaceLayerIndex{pointIndex * pointShadowFaceCount + faceIndex}; // Matches point-light face views.
				const VkImageSubresourceRange pointShadowArrayLayerRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = static_cast<std::uint32_t>(pointFaceLayerIndex), .layerCount = 1U}; // Active face index matches the packed shadow data.
				const VkImageMemoryBarrier pointShadowArrayBeginBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.pointShadowArray.image,
					.subresourceRange = pointShadowArrayLayerRange,
				};
				const VkRenderingAttachmentInfo pointShadowArrayDepthAttachment{ // Dynamic draw pass keeps the legacy clear-before-draw behavior.
					.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = renderer.pointShadowArray.ownedLayerViews[pointFaceLayerIndex],
					.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
					.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
					.clearValue = shadowClear,
				};
				const VkRenderingInfo pointShadowArrayRenderingInfo{
					.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
					.renderArea = {.offset = {0, 0}, .extent = shadowExtent},
					.layerCount = 1U,
					.colorAttachmentCount = 0U,
					.pDepthAttachment = &pointShadowArrayDepthAttachment,
				};

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
											VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											0U, 0U, nullptr, 0U, nullptr, 1U, &pointShadowArrayBeginBarrier);
				renderer.recordedPassOrder.push_back(Renderer::RecordedPass::point_shadow);
				vkCmdBeginRendering(commandBuffer, &pointShadowArrayRenderingInfo);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pointShadowArray.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pointShadowArray.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				drawUploadedObjects(renderer.pointShadowArray.pipelineLayout, static_cast<std::uint32_t>(pointFaceLayerIndex), 0U, true);
				vkCmdEndRendering(commandBuffer);
				const VkImageMemoryBarrier pointShadowArrayReadBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.pointShadowArray.image,
					.subresourceRange = pointShadowArrayLayerRange,
				};
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
											VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &pointShadowArrayReadBarrier);
			}

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
					.image = renderer.swapchain.images[imageIndex],
					.subresourceRange = colorRange,
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = renderer.depthImage.image,
					.subresourceRange = depthRange,
				},
			}};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
										0U, 0U, nullptr, 0U, nullptr, static_cast<std::uint32_t>(beginBarriers.size()), beginBarriers.data());
			const VkRenderingAttachmentInfo colorAttachment{ // Dynamic rendering mirrors the old render-pass color clear/store ops.
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = renderer.imageViews.ownedViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = {.color = {.float32 = {skyBackgroundColor[0], skyBackgroundColor[1], skyBackgroundColor[2], skyBackgroundColor[3]}}},
			};
			const VkRenderingAttachmentInfo depthAttachment{ // Forward depth is cleared to the far plane and kept attachment-local.
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = renderer.depthImage.imageView,
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.clearValue = {.depthStencil = {.depth = 1.0F, .stencil = 0U}},
			};
			const VkRenderingInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.offset = {0, 0}, .extent = renderer.swapchain.extent},
				.layerCount = 1U,
				.colorAttachmentCount = 1U,
				.pColorAttachments = &colorAttachment,
				.pDepthAttachment = &depthAttachment,
			};

			renderer.recordedPassOrder.push_back(Renderer::RecordedPass::forward_color);
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.graphicsPipeline.pipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.pipelineLayout.pipelineLayout, 0U, 1U, &renderer.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(renderer.pipelineLayout.pipelineLayout);
			if (renderer.guiRecord_) { renderer.guiRecord_(commandBuffer); }

			vkCmdEndRendering(commandBuffer);
			const VkImageMemoryBarrier presentBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = renderer.swapchain.images[imageIndex],
				.subresourceRange = colorRange,
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
										VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &presentBarrier);
			result = vkEndCommandBuffer(commandBuffer);
			if (result != VK_SUCCESS) { return result; }

			return VK_SUCCESS;
		}
	};

} // namespace vve::simple
