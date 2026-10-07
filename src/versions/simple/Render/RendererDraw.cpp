module;
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.RenderResources;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Vulkan;

/// @file
/// @brief Per-window recording: DrawItem resolves scene data once; box and frustum helpers conservatively cull each pass.

namespace vve::simple {

	namespace {
		/// @brief Resolved mesh, material and object state reused by every scene pass in one window frame.
		struct DrawItem {
			const VulkanMesh *mesh{};       ///< Stable GPU mesh owner shared by all its instances.
			VkBuffer vertices{};            ///< Static vertices or this window's completed dynamic slot.
			ObjectPushConstants object{};   ///< Model, material slot and unlit flag passed to the shader.
			Bounds worldBounds{};           ///< World AABB enclosing all transformed object-space corners.
			bool castsShadow{};             ///< Whether this visible instance participates in shadow passes.
		};

		/// @brief Encloses an affine-transformed mesh, including rotations and negative/nonuniform scales.
		[[nodiscard]] Bounds worldBounds(const Bounds &box, const Mat4 &model) {
			if (!box.valid) { return {}; }
			Bounds result{};
			// All eight corners are needed; transforming only minimum and maximum loses rotated extrema.
			for (const auto corner : std::views::iota(0U, 8U)) {
				const Vec4 local{corner & 1U ? box.maximum.value.x : box.minimum.value.x,
					corner & 2U ? box.maximum.value.y : box.minimum.value.y,
					corner & 4U ? box.maximum.value.z : box.minimum.value.z, one()};
				const auto position = multiply(model, local);
				const Vec3 point{position.x, position.y, position.z};
				if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) { return {}; }
				result.minimum.value = result.valid ? min(result.minimum.value, point) : point;
				result.maximum.value = result.valid ? max(result.maximum.value, point) : point;
				result.valid = true;
			}
			return result;
		}

		/// @brief Extracts inward planes from Vulkan clip inequalities: -w <= x,y <= w and 0 <= z <= w.
		[[nodiscard]] std::array<Vec4, 6U> frustumPlanes(const Mat4 &matrix) {
			std::array<Vec4, 4U> rows{};
			// Math matrices use column-major indexing; planes are combinations of their rows.
			for (const auto row : std::views::iota(0, 4)) { rows[row] = {matrix[0][row], matrix[1][row], matrix[2][row], matrix[3][row]}; }
			return {add(rows[3], rows[0]), subtract(rows[3], rows[0]), add(rows[3], rows[1]),
				subtract(rows[3], rows[1]), rows[2], subtract(rows[3], rows[2])};
		}

		/// @brief Rejects only boxes strictly outside a plane; touching, intersecting and unknown boxes stay drawable.
		[[nodiscard]] bool intersectsFrustum(const Bounds &box, const std::array<Vec4, 6U> &planes) {
			if (!box.valid) { return true; }
			// The support corner maximizes signed distance; all other corners lie outside if it does.
			for (const auto &plane : planes) {
				const Scalar x = plane.x * (plane.x >= zero() ? box.maximum.value.x : box.minimum.value.x);
				const Scalar y = plane.y * (plane.y >= zero() ? box.maximum.value.y : box.minimum.value.y);
				const Scalar z = plane.z * (plane.z >= zero() ? box.maximum.value.z : box.minimum.value.z);
				const Scalar tolerance = 32 * std::numeric_limits<Scalar>::epsilon() * (std::abs(x) + std::abs(y) + std::abs(z) + std::abs(plane.w) + one());
				if (x + y + z + plane.w < -tolerance) { return false; }
			}
			return true;
		}
	} // namespace

	constexpr std::uint64_t kFrameFenceTimeoutNs{1'000'000'000ULL}; ///< Wait at most one second for the in-flight frame slot.
	constexpr std::uint64_t kAcquireTimeoutNs{100'000'000ULL}; ///< Wait at most 100 ms for presentation to release an image.

	/// @brief Classifies acquisition results without touching Vulkan objects or frame state.
	auto acquireAction(VkResult result) -> AcquireAction {
		switch (result) {
		case VK_SUCCESS: case VK_SUBOPTIMAL_KHR: return AcquireAction::render;
		case VK_TIMEOUT: case VK_NOT_READY: return AcquireAction::skip;
		case VK_ERROR_OUT_OF_DATE_KHR: return AcquireAction::recreate;
		default: return AcquireAction::fail;
		}
	}

	/**
		* @brief Draws one swapchain frame through the per-frame synchronization objects.
		*
		* @param target Window resources and camera used for this frame.
		* @param readback Optional swapchain-image readback sink used by deterministic debug captures.
		* @return True only when presentation succeeds or reports a suboptimal swapchain.
	*/
	bool ForwardRenderer::drawFrame(WindowTarget &target, VulkanReadback *readback) {
		textureUploadError_.reset();
		const auto windowExtent = currentWindowPixelExtent(target);
		if (windowExtent.width == 0U || windowExtent.height == 0U) { return false; }
		// Compare against the extent the swapchain was requested for, not the surface-chosen one, so a driver that clamps or reports a different currentExtent does not force a recreate every frame.
		if (windowExtent.width != target.swapchain.requestedExtent.width || windowExtent.height != target.swapchain.requestedExtent.height) {
			if (const VkResult result = recreateSwapchain(target, windowExtent); result != VK_SUCCESS) { reportFrameFailure("swapchain recreate", result); return false; }
		}

		const std::size_t frameCount{target.frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
		if (frameCount == 0U || target.frameSync.imageAvailableSemaphores.size() < frameCount || target.frameSync.renderFinishedSemaphores.empty()) { return false; }
		if (target.commandBuffers.ownedCommandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || target.swapchain.swapchain == VK_NULL_HANDLE) { return false; }
		const std::uint32_t frameIndex{target.currentFrame < frameCount ? target.currentFrame : 0U};

		const VkFence inFlightFence{target.frameSync.inFlightFences[frameIndex]};
		const VkSemaphore imageAvailableSemaphore{target.frameSync.imageAvailableSemaphores[frameIndex]};
		if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return false; }

		VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, kFrameFenceTimeoutNs);
		// A busy slot is retried unchanged; do not reset its fence or update its frame data.
		if (result == VK_TIMEOUT) { ++skippedFrameCount_; return false; }
		if (result != VK_SUCCESS) { reportFrameFailure("fence wait", result); return false; }
		collectRetiredResources(target.submitSerials[frameIndex]);
		if (const VkResult sync = syncSceneResources(); sync != VK_SUCCESS) { reportFrameFailure("scene sync", sync); return false; }
		result = writeSceneDescriptors(target, frameIndex);
		if (result != VK_SUCCESS) { reportFrameFailure("scene descriptors", result); return false; }
		result = uploadDynamicVertices(target, frameIndex);
		if (result != VK_SUCCESS) { reportFrameFailure("vertex update", result); return false; }
		target.currentFrame = frameIndex;
		target.lastReadbackCaptureResult.reset();

		// Frame data only needs the frame slot, which the fence wait freed; preparing it before the acquire keeps failures away from acquired images.
		const Scalar aspectRatio{target.swapchain.extent.height == 0U ? one() : static_cast<Scalar>(target.swapchain.extent.width) / static_cast<Scalar>(target.swapchain.extent.height)}; ///< Live swapchain aspect with a zero-height guard.
		const Camera &camera = target.camera ? *target.camera : camera_;
		const Scalar cameraNear{camera.clip.near_plane}; ///< Camera near plane shared by projection and cascade splitting.
		const Scalar cameraFar{camera.clip.far_plane}; ///< Camera far plane limits directional cascade coverage.
		const Mat4 cameraView{lookAt(camera.position.value, add(camera.position.value, camera.forward.value), detail::stableUp(camera.forward.value))}; // The same window view drives uniforms and cascade fitting.
		prepareShadowFrame(cameraView, camera.fov_y.radians, aspectRatio, cameraNear, cameraFar);
		result = ensureShadowCapacity();
		if (result != VK_SUCCESS) { reportFrameFailure("shadow capacity", result); return false; }
		result = writeShadowDescriptors(target, frameIndex);
		if (result != VK_SUCCESS) { reportFrameFailure("shadow descriptors", result); return false; }
		if (gpuDebugReadback_) { recordShadowDepthSamples(); }
		else { shadowDepthSamples.clear(); }
		result = target.uniformBuffers.update(target.currentFrame, frameUniforms_);
		if (result != VK_SUCCESS) { reportFrameFailure("uniform update", result); return false; }
		// Prepare only the GUI-owning window; no draw data means no GUI pass or attachment transition.
		const bool drawGui = target.guiWindow && guiPrepare_ && guiRecord_ && guiPrepare_();

		std::uint32_t imageIndex{};
		result = vkAcquireNextImageKHR(device.device, target.swapchain.swapchain, kAcquireTimeoutNs, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		// VK_SUBOPTIMAL_KHR still acquired an image: it must be rendered and presented, otherwise the swapchain runs out of images and the next acquire blocks forever.
		switch (acquireAction(result)) {
		case AcquireAction::render: break;
		case AcquireAction::skip: ++skippedFrameCount_; return false; // No semaphore was signaled, so there is nothing to consume.
		case AcquireAction::recreate: (void)recreateSwapchain(target, currentWindowPixelExtent(target)); return false;
		case AcquireAction::fail: reportFrameFailure("image acquire", result); return false;
		}

		// Once an image is acquired, every early exit must consume its semaphore; the rebuilt swapchain then releases the image.
		// signalFence re-signals the in-flight fence when it was already reset, so the next wait on it does not block forever.
		const auto abandonAcquiredImage = [&](const char *stage, VkResult failed, VkFence signalFence = VK_NULL_HANDLE) {
			reportFrameFailure(stage, failed);
			const VkPipelineStageFlags consumeStage{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
			const VkSubmitInfo consume{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &imageAvailableSemaphore,
				.pWaitDstStageMask = &consumeStage,
			};
			(void)vkQueueSubmit(device.graphicsQueue, 1U, &consume, signalFence);
			target.swapchain.requestedExtent = {};
		};
		if (imageIndex >= target.frameSync.renderFinishedSemaphores.size() || target.frameSync.renderFinishedSemaphores[imageIndex] == VK_NULL_HANDLE) {
			abandonAcquiredImage("present semaphore lookup", VK_ERROR_INITIALIZATION_FAILED);
			return false;
		}
		const VkSemaphore renderFinishedSemaphore{target.frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.

		result = recordCommandBuffer(target, target.currentFrame, imageIndex, multiply(frameUniforms_.projection, frameUniforms_.view), drawGui);
		if (result != VK_SUCCESS) { abandonAcquiredImage("command recording", result); return false; }

		// Reset the fence only once the submit is certain; an unsignaled fence without a submit would block the next frame forever.
		result = vkResetFences(device.device, 1U, &inFlightFence);
		if (result != VK_SUCCESS) { abandonAcquiredImage("fence reset", result); return false; }

		const VkPipelineStageFlags waitStage{VK_PIPELINE_STAGE_TRANSFER_BIT}; ///< Only the swapchain transition and blit wait for acquire; shadow and HDR work can proceed.
		const VkCommandBuffer commandBuffer{target.commandBuffers.ownedCommandBuffers[target.currentFrame]}; // Borrow from the frame slot owner for submission.
		const VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &imageAvailableSemaphore,
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1U,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 1U,
			.pSignalSemaphores = &renderFinishedSemaphore,
		};
		result = vkQueueSubmit(device.graphicsQueue, 1U, &submitInfo, inFlightFence);
		if (result != VK_SUCCESS) { abandonAcquiredImage("queue submit", result, inFlightFence); return false; }
		target.submitSerials[frameIndex] = ++submitSerial_;
		fillShadowDepthSamplesFromGpu();
		if (readback != nullptr && imageIndex < target.swapchain.images.size()) {
			target.lastReadbackCaptureResult = readback->capture(target.swapchain.images[imageIndex], 0U, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}

		VkSwapchainKHR presentSwapchain = target.swapchain.swapchain;
		const VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1U,
			.pWaitSemaphores = &renderFinishedSemaphore,
			.swapchainCount = 1U,
			.pSwapchains = &presentSwapchain,
			.pImageIndices = &imageIndex,
		};
		result = vkQueuePresentKHR(target.presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			(void)recreateSwapchain(target, currentWindowPixelExtent(target));
			return result == VK_SUBOPTIMAL_KHR;
		}
		if (result != VK_SUCCESS) { reportFrameFailure("present", result); return false; }

		target.lastRenderedImageIndex = imageIndex;
		target.currentFrame = static_cast<std::uint32_t>((target.currentFrame + 1U) % frameCount);
		return true;
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
		* @param target Window attachments, commands and descriptors used for this frame.
		* @param frameIndex Index selecting the per-frame command buffer and descriptor set.
		* @param imageIndex Index of the acquired swapchain image.
		* @param viewProjection Camera clip transform already used by this frame's uniforms.
		* @param drawGui Whether GUI preparation produced vertices for this window.
		* @return VK_SUCCESS when command recording succeeds, otherwise the first failing Vulkan result.
		*/
	VkResult ForwardRenderer::recordCommandBuffer(WindowTarget &target, std::uint32_t frameIndex, std::uint32_t imageIndex, const Mat4 &viewProjection, bool drawGui) {
		recordedPassOrder.clear();
		lastShadowLayerPassCount_ = 0U;
		lastFrameDrawStats_ = {};
		if (frameIndex >= target.commandBuffers.ownedCommandBuffers.size() || frameIndex >= target.descriptorSets.descriptorSets.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
		if (imageIndex >= target.swapchain.images.size() || imageIndex >= target.imageViews.ownedViews.size()) { return VK_ERROR_INITIALIZATION_FAILED; }
		if (shadowPipeline.pipeline == VK_NULL_HANDLE || pipelineLayout.pipelineLayout == VK_NULL_HANDLE) { return VK_ERROR_INITIALIZATION_FAILED; }

		const VkCommandBuffer commandBuffer{target.commandBuffers.ownedCommandBuffers[frameIndex]};
		VkResult result = vkResetCommandBuffer(commandBuffer, 0U);
		if (result != VK_SUCCESS) { return result; }

		const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		if (result != VK_SUCCESS) { return result; }

		std::vector<DrawItem> items{};
		// Resolve scene handles and world boxes once, before any pass traverses the sorted draw list.
		if (renderInstances_ != nullptr) {
			items.reserve(renderInstances_->size());
			for (const RenderInstance &instance : *renderInstances_) {
				if (!instance.visible) { continue; }
				const auto uploaded = meshes.find(instance.mesh);
				const auto material = materialSlots_.find(instance.material);
				if (uploaded == meshes.end() || material == materialSlots_.end()) { continue; }
				const VulkanMesh &mesh = uploaded->second;
				const auto dynamic = target.dynamicVertices.find(instance.mesh);
				items.push_back({.mesh = &mesh,
					.vertices = dynamic == target.dynamicVertices.end() ? mesh.vertexBuffer.buffer : dynamic->second.first[frameIndex].buffer,
					.object = {.model = instance.world_transform, .materialIndex = material->second, .unlit = instance.unlit ? 1U : 0U},
					.worldBounds = worldBounds(mesh.localBox, instance.world_transform), .castsShadow = instance.casts_shadow});
			}
		}
		std::ranges::sort(items, std::less<const VulkanMesh *>{}, &DrawItem::mesh);
		const VulkanMesh *boundMesh{};
		const auto drawUploadedObjects = [&](std::uint32_t shadowMatrixIndex, bool shadowPass, const Mat4 &clip) {
			const auto planes = frustumPlanes(clip);
			// Bound mesh state survives dynamic-rendering boundaries and pipeline changes.
			for (const auto &item : items) {
				if ((shadowPass && !item.castsShadow) || !intersectsFrustum(item.worldBounds, planes)) { continue; }
				if (boundMesh != item.mesh) {
					const VkDeviceSize offset{};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, &item.vertices, &offset);
					vkCmdBindIndexBuffer(commandBuffer, item.mesh->indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					boundMesh = item.mesh;
					++lastFrameDrawStats_.vertexBufferBinds;
				}
				auto pushConstants = item.object;
				pushConstants.shadowMatrixIndex = shadowMatrixIndex;
				vkCmdPushConstants(commandBuffer, pipelineLayout.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0U, sizeof(ObjectPushConstants), &pushConstants);
				vkCmdDrawIndexed(commandBuffer, item.mesh->indexCount, 1U, 0U, 0, 0U);
				++(shadowPass ? lastFrameDrawStats_.shadowDraws : lastFrameDrawStats_.forwardDraws);
			}
		};
		// Only shadow-casting layers are cleared and drawn; inactive layers retain their shader-read layout.
		const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}};
		bool shadowPipelineBound{}; // Bind state belongs to this command buffer, independently of frame diagnostics.
		const auto recordShadowLayer = [&](const ShadowMap &map, std::uint32_t layer, std::uint32_t matrixIndex, RecordedPass pass) {
			// One shadow pipeline/set and viewport serve every light type and layer.
			if (!shadowPipelineBound) {
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.pipeline);
				++lastFrameDrawStats_.pipelineBinds;
				const VkViewport viewport{.width = static_cast<float>(ShadowMap::resolution), .height = static_cast<float>(ShadowMap::resolution), .minDepth = 0.0F, .maxDepth = 1.0F};
				const VkRect2D scissor{.extent = {ShadowMap::resolution, ShadowMap::resolution}};
				vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
				vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &target.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
				shadowPipelineBound = true;
			}
			const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = layer, .layerCount = 1U};
			const VkImageMemoryBarrier beginBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = map.image,
				.subresourceRange = range,
			};
			// The arrays are shared by all windows and frames: wait until the previous draw has finished sampling this layer.
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
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
			++lastShadowLayerPassCount_;
			recordedPassOrder.push_back(pass);
			drawUploadedObjects(matrixIndex, true, frameUniforms_.shadowViewProjs[matrixIndex]);
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
		const auto recordShadowMap = [&](const ShadowMap &map, std::uint32_t activeLights, std::uint32_t layersPerLight,
			std::size_t matrixBase, RecordedPass pass, std::span<const Vec4> directions = {}) {
			// Ambient-only lights retain a packed lighting slot but have no shadow passes.
			for (std::uint32_t light{}; light < activeLights; ++light) {
				if (!directions.empty() && directions[light].w != zero()) { continue; }
				for (std::uint32_t face{}; face < layersPerLight; ++face) {
					const std::uint32_t layer{light * layersPerLight + face};
					recordShadowLayer(map, layer, static_cast<std::uint32_t>(matrixBase + layer), pass);
				}
			}
		};
		recordShadowMap(dirShadowArray, frameUniforms_.activeDirectionalLightCount, kNumShadowCascades, kShadowMatrixDirBase, RecordedPass::directional_shadow, frameUniforms_.directionalLightDirections);
		recordShadowMap(spotShadowArray, frameUniforms_.activeSpotLightCount, 1U, kShadowMatrixSpotBase, RecordedPass::spot_shadow, frameUniforms_.spotLightDirections);
		recordShadowMap(pointShadowArray, frameUniforms_.activePointLightCount, pointShadowFaceCount, kShadowMatrixPointBase, RecordedPass::point_shadow);

		constexpr std::array<float, 4U> skyBackgroundColor{0.45F, 0.70F, 1.00F, 1.00F}; // Sky background for the forward color pass.
		const VkImageSubresourceRange colorRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole swapchain image.
		const VkImageSubresourceRange depthRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole depth image.
		
		// HDR image was last read by the final blit of the previous frame
		const VkImageMemoryBarrier hdrBeginBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.hdrImage.image,
			.subresourceRange = colorRange,
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							0U, 0U, nullptr, 0U, nullptr, 1U, &hdrBeginBarrier);

		// The depth image is shared by both frames in flight: wait for the previous frame's depth writes.
		const VkImageMemoryBarrier depthBeginBarriers{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.depthImage.image,
			.subresourceRange = depthRange,
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
									VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
									0U, 0U, nullptr, 0U, nullptr, 1U, &depthBeginBarriers);
									
		const VkRenderingAttachmentInfo colorAttachment{ // Dynamic rendering mirrors the old render-pass color clear/store ops.
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = target.hdrImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.color = {.float32 = {skyBackgroundColor[0], skyBackgroundColor[1], skyBackgroundColor[2], skyBackgroundColor[3]}}},
		};
		const VkRenderingAttachmentInfo depthAttachment{ // Forward depth is cleared to the far plane and kept attachment-local.
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = target.depthImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = {.depthStencil = {.depth = 1.0F, .stencil = 0U}},
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {.offset = {0, 0}, .extent = target.swapchain.extent},
			.layerCount = 1U,
			.colorAttachmentCount = 1U,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
		};

		recordedPassOrder.push_back(RecordedPass::forward_color);
		vkCmdBeginRendering(commandBuffer, &renderingInfo);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
		++lastFrameDrawStats_.pipelineBinds;
		// Match the current attachments; the projection still supplies the Vulkan Y flip.
		const VkViewport viewport{.x = 0.0F, .y = 0.0F, .width = static_cast<float>(target.swapchain.extent.width), .height = static_cast<float>(target.swapchain.extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
		const VkRect2D scissor{.offset = {0, 0}, .extent = target.swapchain.extent};
		vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
		vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &target.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
		drawUploadedObjects(0U, false, viewProjection);

		vkCmdEndRendering(commandBuffer);

		// Both images need Layout GENERAL because HDR image gets read and Swapchain image gets written
		const std::array<VkImageMemoryBarrier, 2U> postBarriers{{
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_GENERAL, // VVPPL Library needs General
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = target.hdrImage.image,
				.subresourceRange = colorRange,
			},
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // old content is being deleted
				.newLayout = VK_IMAGE_LAYOUT_GENERAL, // VVPPL Library gives General
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = target.swapchain.images[imageIndex],
				.subresourceRange = colorRange,
			}
		}};

		// TRANSFER chains the swapchain layout transition after the acquire semaphore wait.
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_TRANSFER_BIT, 0U, 0U, nullptr, 0U, nullptr,
							static_cast<std::uint32_t>(postBarriers.size()), postBarriers.data());

		
		if (target.postProcess) {
			recordedPassOrder.push_back(RecordedPass::post_process);
			target.postProcess->apply(commandBuffer, target.hdrImage.image, target.swapchain.images[imageIndex], frameIndex);
		} else {
			// Same blit the library would do, without the library (rendered HDR image to Swapchain Image)
			VkImageBlit blit{};
			blit.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0U, .baseArrayLayer = 0U, .layerCount = 1U};
			blit.srcOffsets[1] = {static_cast<std::int32_t>(target.swapchain.extent.width),
										 static_cast<std::int32_t>(target.swapchain.extent.height), 1};
			blit.dstSubresource = blit.srcSubresource;
			blit.dstOffsets[1] = blit.srcOffsets[1];
			vkCmdBlitImage(commandBuffer, target.hdrImage.image, VK_IMAGE_LAYOUT_GENERAL,
								target.swapchain.images[imageIndex], VK_IMAGE_LAYOUT_GENERAL, 1U, &blit, VK_FILTER_NEAREST);
		}

		// ImGui stays on its owning window and loads the attachment only when vertices need drawing.
		if (drawGui) {
			const VkImageMemoryBarrier guiBarrier {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = target.swapchain.images[imageIndex],
				.subresourceRange = colorRange,
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								0U, 0U, nullptr, 0U, nullptr, 1U, &guiBarrier);

			const VkRenderingAttachmentInfo guiAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = target.imageViews.ownedGuiViews.empty() ? target.imageViews.ownedViews[imageIndex] : target.imageViews.ownedGuiViews[imageIndex], // Avoid encoding ImGui's sRGB colours twice.
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			};
			const VkRenderingInfo guiRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = {.offset = {0, 0}, .extent = target.swapchain.extent},
				.layerCount = 1U,
				.colorAttachmentCount = 1U,
				.pColorAttachments = &guiAttachment,
			};
			recordedPassOrder.push_back(RecordedPass::gui);
			vkCmdBeginRendering(commandBuffer, &guiRenderingInfo);
			guiRecord_(commandBuffer);
			vkCmdEndRendering(commandBuffer);
		}

		const VkImageMemoryBarrier presentBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = static_cast<VkAccessFlags>(drawGui ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT),
			.oldLayout = drawGui ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.swapchain.images[imageIndex],
			.subresourceRange = colorRange,
		};
		vkCmdPipelineBarrier(commandBuffer, drawGui ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
									VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0U, 0U, nullptr, 0U, nullptr, 1U, &presentBarrier);
		result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS) { return result; }

		return VK_SUCCESS;
	}

} // namespace vve::simple
