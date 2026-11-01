module;
#include <SDL3/SDL_video.h>
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
/// Solid items draw first, sorted by mesh, then the sky; transparent items follow with the blended pipeline, sorted far to near.

namespace vve::simple {

	namespace {
		/// @brief Resolved mesh, material and object state reused by every scene pass in one window frame.
		struct DrawItem {
			const VulkanMesh *mesh{};       ///< Stable GPU mesh owner shared by all its instances.
			VkBuffer vertices{};            ///< Static vertices or this window's completed dynamic slot.
			ObjectPushConstants object{};   ///< Model, material slot, flags and opacity passed to the shader.
			Bounds worldBounds{};           ///< World AABB enclosing all transformed object-space corners.
			bool castsShadow{};             ///< Whether this visible instance participates in shadow passes.
			bool sky{};                     ///< Drawn after solid objects and before transparent ones, never culled.
			bool transparent{};             ///< Blended after all solid objects, without depth writes.
			bool terrain{};                 ///< Tessellated height grid, drawn first with the terrain pipelines.
			Scalar distance{};              ///< Squared camera distance; transparent items draw far to near.
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

		/// @brief Encloses a camera-facing sprite in every orientation: a cube around its origin, sized by its scaled quad.
		[[nodiscard]] Bounds spriteBounds(const Bounds &box, const Mat4 &model) {
			if (!box.valid) { return {}; }
			const Scalar scale = std::max(length(Vec3{model[0][0], model[0][1], model[0][2]}), length(Vec3{model[1][0], model[1][1], model[1][2]}));
			const Scalar radius = scale * std::max(length(box.minimum.value), length(box.maximum.value));
			const Vec3 centre{model[3][0], model[3][1], model[3][2]};
			return {.minimum = {subtract(centre, Vec3{radius, radius, radius})}, .maximum = {add(centre, Vec3{radius, radius, radius})}, .valid = true};
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

	/// @brief Classifies presentation and swapchain recovery without Vulkan objects; rebuild success is a skipped frame.
	auto frameOutcome(VkResult result, FrameOutcome completed) -> std::expected<FrameOutcome, Error> {
		switch (result) {
		case VK_SUCCESS: case VK_SUBOPTIMAL_KHR: return completed;
		case VK_TIMEOUT: case VK_NOT_READY: case VK_ERROR_OUT_OF_DATE_KHR: return FrameOutcome::skipped;
		default: return std::unexpected(Error::platform_error);
		}
	}

	/// @brief Replaces the recorder without waiting: captured owners outlive their last submitted use.
	auto ForwardRenderer::setNativeDraw(NativeDrawCallback callback) -> std::expected<void,Error> {
		if (nativeDraw_) { retire(std::move(nativeDraw_)); }
		nativeDraw_ = std::move(callback);
		notifyNativeDrawChanged();
		return {};
	}
	/// @brief Explicit external edits affect captures and caster depth, never per-frame engine uploads.
	void ForwardRenderer::notifyNativeDrawChanged() { invalidateProbes(false,true); }

	/// @brief Completes the public per-pass context; failures abort recording through the existing acquired-image cleanup.
	VkResult ForwardRenderer::recordNative(WindowTarget &target, VkCommandBuffer command, NativeDrawContext context) {
		if (!nativeDraw_) { return VK_SUCCESS; }
		context.device = reinterpret_cast<std::uintptr_t>(static_cast<VkDevice>(device.device));
		context.physicalDevice = reinterpret_cast<std::uintptr_t>(static_cast<VkPhysicalDevice>(physicalDevice.physicalDevice));
		context.commandBuffer = reinterpret_cast<std::uintptr_t>(command);
		context.window = target.handle; context.frameIndex = target.currentFrame;
		context.viewProjection = multiply(context.projection,context.view);
		context.frustum = frustumPlanes(context.viewProjection);
		context.eye = Position{Vec3{inverse(context.view)[3]}};
		context.depthFormat = depthFormat;
		const bool single = context.pass == NativeDrawPass::shadow || context.pass == NativeDrawPass::probe_capture;
		context.samples = single ? VK_SAMPLE_COUNT_1_BIT : sampleCount();
		context.colorAttachmentCount = context.pass == NativeDrawPass::shadow ? 0U : context.pass == NativeDrawPass::gbuffer ? 3U : 1U;
		for (auto i : std::views::iota(0U,context.colorAttachmentCount)) { context.colorFormats[i] = gbufferFormats[i]; }
		context.samplerAnisotropy = device.samplerAnisotropy; context.tessellationShader = device.tessellationShader;
		if (!context.width) { context.width = target.swapchain.extent.width; context.height = target.swapchain.extent.height; }
		// The public sampler borrows only coefficient views, never G-buffer or depth attachments being written.
		if (backend_ == RendererBackend::hybrid_gi &&
			(context.prepare || context.pass == NativeDrawPass::forward_opaque || context.pass == NativeDrawPass::forward_transparent)) {
			context.probeIrradianceViews = {reinterpret_cast<std::uint64_t>(target.probes.coefficients.imageView),reinterpret_cast<std::uint64_t>(target.coarseProbes.coefficients.imageView)};
			context.probeOrigin = frameUniforms_.probeOrigin; context.probeControl = frameUniforms_.probeControl;
			context.probeEye = (target.camera ? *target.camera : camera_).position;
			context.probeCascades = static_cast<std::uint32_t>(frameUniforms_.viewport.w);
		}
		// Preparation may bind these immutable views after its slot fence; only later colour/capture scopes sample them.
		context.mainView = frameUniforms_.view;
		context.sunShadowView = reinterpret_cast<std::uint64_t>(dirShadowArray.imageView);
		context.sunShadowTexels = Vec4{dirShadowArray.extent.width ? 1.F/dirShadowArray.extent.width : 0.F,dirShadowArray.extent.height ? 1.F/dirShadowArray.extent.height : 0.F,
			target.probeShadow.extent.width ? 1.F/target.probeShadow.extent.width : 0.F,target.probeShadow.extent.height ? 1.F/target.probeShadow.extent.height : 0.F};
		for (std::uint32_t sun = 0; sun < frameUniforms_.activeDirectionalLightCount; ++sun) {
			if (frameUniforms_.directionalLightDirections[sun].w != zero()) { continue; }
			context.sunShadowCount = 4; context.sunShadowFirstLayer = sun * kNumShadowCascades;
			context.sunShadowSplits = frameUniforms_.cascadeSplits;
			for (std::uint32_t i = 0; i < 4; ++i) { context.sunShadowMatrices[i] = frameUniforms_.shadowViewProjs[kShadowMatrixDirBase + context.sunShadowFirstLayer + i]; }
			break;
		}
		context.captureSunShadowView = reinterpret_cast<std::uint64_t>(target.probeShadow.imageView);
		context.captureSunShadowMatrix = target.probeShadowMatrix;
		context.captureSunShadowEnabled = target.probeSun >= 0;
		try {
			if (const auto result = nativeDraw_(context); !result) {
				std::cerr << "[vve::simple] native draw: " << errorName(result.error()) << '\n';
				return VK_ERROR_INITIALIZATION_FAILED;
			}
		} catch (...) { return VK_ERROR_INITIALIZATION_FAILED; }
		return VK_SUCCESS;
	}

	/**
		* @brief Draws one swapchain frame through the per-frame synchronization objects.
		*
		* @param target Window resources and camera used for this frame.
		* @param readback Optional swapchain-image readback sink used by deterministic debug captures.
		* @return Presented, temporarily skipped, or the fatal error retained until renderer cleanup.
	*/
	auto ForwardRenderer::drawFrame(WindowTarget &target, VulkanReadback *readback) -> std::expected<FrameOutcome, Error> {
		if (frameError_) { return std::unexpected(*frameError_); }
		textureUploadError_.reset();
		const auto windowExtent = currentWindowPixelExtent(target);
		if (windowExtent.width == 0U || windowExtent.height == 0U) { return FrameOutcome::skipped; }
		// Compare against the extent the swapchain was requested for, not the surface-chosen one, so a driver that clamps or reports a different currentExtent does not force a recreate every frame.
		if (windowExtent.width != target.swapchain.requestedExtent.width || windowExtent.height != target.swapchain.requestedExtent.height) {
			const VkResult result = recreateSwapchain(target, windowExtent);
			if (!frameOutcome(result, FrameOutcome::skipped)) { return reportFrameFailure("swapchain recreate", result); }
			if (result != VK_SUCCESS) { return FrameOutcome::skipped; }
		}

		const std::size_t frameCount{target.frameSync.inFlightFences.size()}; // Existing sync count defines frames in flight.
		if (frameCount == 0U || target.frameSync.imageAvailableSemaphores.size() < frameCount || target.frameSync.renderFinishedSemaphores.empty()) { return reportFrameFailure("frame synchronization", VK_ERROR_INITIALIZATION_FAILED); }
		if (target.commandBuffers.ownedCommandBuffers.size() < frameCount || device.device == VK_NULL_HANDLE || target.swapchain.swapchain == VK_NULL_HANDLE) { return reportFrameFailure("frame resources", VK_ERROR_INITIALIZATION_FAILED); }
		const std::uint32_t frameIndex{target.currentFrame < frameCount ? target.currentFrame : 0U};

		const VkFence inFlightFence{target.frameSync.inFlightFences[frameIndex]};
		const VkSemaphore imageAvailableSemaphore{target.frameSync.imageAvailableSemaphores[frameIndex]};
		if (inFlightFence == VK_NULL_HANDLE || imageAvailableSemaphore == VK_NULL_HANDLE) { return reportFrameFailure("frame synchronization handles", VK_ERROR_INITIALIZATION_FAILED); }

		VkResult result = vkWaitForFences(device.device, 1U, &inFlightFence, VK_TRUE, kFrameFenceTimeoutNs);
		// A busy slot is retried unchanged; do not reset its fence or update its frame data.
		if (result == VK_TIMEOUT) { ++skippedFrameCount_; return FrameOutcome::skipped; }
		if (result != VK_SUCCESS) { return reportFrameFailure("fence wait", result); }
		collectRetiredResources(target.submitSerials[frameIndex]);
		collectGpuTiming(target, frameIndex);
		result = prepareGpuTiming(target);
		if (result != VK_SUCCESS) { return reportFrameFailure("GPU timing queries", result); }
		if (const VkResult sync = syncSceneResources(); sync != VK_SUCCESS) { return reportFrameFailure("scene sync", sync); }
		refreshTerrainHeights();
		result = writeSceneDescriptors(target, frameIndex);
		if (result != VK_SUCCESS) { return reportFrameFailure("scene descriptors", result); }
		result = uploadDynamicVertices(target, frameIndex);
		if (result != VK_SUCCESS) { return reportFrameFailure("vertex update", result); }
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
		if (result != VK_SUCCESS) { return reportFrameFailure("shadow capacity", result); }
		result = writeShadowDescriptors(target, frameIndex);
		if (result != VK_SUCCESS) { return reportFrameFailure("shadow descriptors", result); }
		if (gpuDebugReadback_) { recordShadowDepthSamples(); }
		else { shadowDepthSamples.clear(); }
		frameUniforms_.inverseViewProjection = inverse(multiply(frameUniforms_.projection, frameUniforms_.view));
		frameUniforms_.viewport = {static_cast<float>(target.swapchain.extent.width), static_cast<float>(target.swapchain.extent.height), 0, 0};
		const auto haze = atmosphere_.color.value;
		frameUniforms_.fogColorDensity = {haze.x, haze.y, haze.z, atmosphere_.density};
		frameUniforms_.fogHeightTime = {atmosphere_.base_height, atmosphere_.height_falloff, static_cast<float>(animation_time_.seconds), 0};
		frameUniforms_.distanceMist = {atmosphere_.distance_mist_start, atmosphere_.distance_mist_end, 0, 0};
		if (sky_.enabled) {
			// The first lit directional light is the sun; without one the sky is lit from straight above and shows no disk.
			const auto lights = std::span{frameUniforms_.directionalLightDirections}.first(frameUniforms_.activeDirectionalLightCount);
			const auto sun = std::ranges::find_if(lights, [](const Vec4 &light) { return light.w == zero(); });
			const Vec3 towardsSun = sun == lights.end() ? Vec3{zero(), one(), zero()} : normalize(Vec3{-sun->x, -sun->y, -sun->z});
			// Wind offsets wrap after 1500 km, far beyond the noise features, so long sessions keep float precision.
			const auto drift = [this](Scalar wind) { return static_cast<float>(std::fmod(-wind * animation_time_.seconds, 1.5e6)); };
			frameUniforms_.skySun = {towardsSun.x, towardsSun.y, towardsSun.z, sky_.intensity * 15.0F};
			frameUniforms_.skyParams = {sky_.haze, sun == lights.end() ? zero() : sky_.sun_size * 0.00465F, static_cast<float>(sun - lights.begin()), 0};
			frameUniforms_.cloudLayer = {sky_.cloud_base, sky_.cloud_thickness, sky_.cloud_coverage, sky_.cloud_density * 0.04F};
			frameUniforms_.cloudWind = {drift(sky_.wind.x), drift(sky_.wind.y), 0, 0};
		}
		if (backend_ == RendererBackend::hybrid_gi) {
			result = prepareProbes(target, frameIndex);
			if (result != VK_SUCCESS) { return reportFrameFailure("probe preparation", result); }
		}
		result = target.uniformBuffers.update(target.currentFrame, frameUniforms_);
		if (result != VK_SUCCESS) { return reportFrameFailure("uniform update", result); }
		// Prepare only the GUI-owning window; no draw data means no GUI pass or attachment transition.
		const bool drawGui = target.guiWindow && guiPrepare_ && guiRecord_ && guiPrepare_();

		std::uint32_t imageIndex{};
		result = vkAcquireNextImageKHR(device.device, target.swapchain.swapchain, kAcquireTimeoutNs, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		// VK_SUBOPTIMAL_KHR still acquired an image: it must be rendered and presented, otherwise the swapchain runs out of images and the next acquire blocks forever.
		switch (acquireAction(result)) {
		case AcquireAction::render: break;
		case AcquireAction::skip: ++skippedFrameCount_; return FrameOutcome::skipped; // No semaphore was signaled, so there is nothing to consume.
		case AcquireAction::recreate: target.swapchain.requestedExtent = {}; return FrameOutcome::skipped;
		case AcquireAction::fail: return reportFrameFailure("image acquire", result);
		}

		// Once an image is acquired, every early exit must consume its semaphore; the rebuilt swapchain then releases the image.
		// signalFence re-signals the in-flight fence when it was already reset, so the next wait on it does not block forever.
		const auto abandonAcquiredImage = [&](const char *stage, VkResult failed, VkFence signalFence = VK_NULL_HANDLE) {
			const auto failure = reportFrameFailure(stage, failed);
			const VkPipelineStageFlags consumeStage{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
			const VkSubmitInfo consume{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1U,
				.pWaitSemaphores = &imageAvailableSemaphore,
				.pWaitDstStageMask = &consumeStage,
			};
			if (const VkResult consumed = vkQueueSubmit(device.graphicsQueue, 1U, &consume, signalFence); consumed != VK_SUCCESS) {
				(void)reportFrameFailure("acquired semaphore cleanup", consumed);
			}
			target.swapchain.requestedExtent = {};
			return failure;
		};
		if (imageIndex >= target.frameSync.renderFinishedSemaphores.size() || target.frameSync.renderFinishedSemaphores[imageIndex] == VK_NULL_HANDLE) {
			return abandonAcquiredImage("present semaphore lookup", VK_ERROR_INITIALIZATION_FAILED);
		}
		const VkSemaphore renderFinishedSemaphore{target.frameSync.renderFinishedSemaphores[imageIndex]}; // Present-wait semaphore follows the acquired swapchain image.

		result = recordCommandBuffer(target, target.currentFrame, imageIndex, multiply(frameUniforms_.projection, frameUniforms_.view), drawGui);
		if (result != VK_SUCCESS) { return abandonAcquiredImage("command recording", result); }

		// Reset the fence only once the submit is certain; an unsignaled fence without a submit would block the next frame forever.
		result = vkResetFences(device.device, 1U, &inFlightFence);
		if (result != VK_SUCCESS) { return abandonAcquiredImage("fence reset", result); }

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
		if (result != VK_SUCCESS) { return abandonAcquiredImage("queue submit", result, inFlightFence); }
		target.submitSerials[frameIndex] = ++submitSerial_;
		if (gpuTimingEnabled && target.gpuQueries) target.timingWritten[frameIndex].set(WindowTarget::timingQueries - 1);
		if (submitSerial_ == 1U) {
			std::clog << "[vve::simple] renderer=" << std::array{"forward", "deferred", "hybrid-gi"}[std::to_underlying(backend_)]
				<< " gbuffer=" << std::ranges::count(recordedPassOrder, RecordedPass::gbuffer)
				<< " deferred_lighting=" << std::ranges::count(recordedPassOrder, RecordedPass::deferred_lighting)
				<< " samples=" << static_cast<unsigned>(sampleCount()) << '\n';
		}
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
		const auto outcome = frameOutcome(result, FrameOutcome::presented);
		if (!outcome) { return reportFrameFailure("present", result); }
		// Defer recovery until the next draw, so an accepted suboptimal image is counted even if rebuilding later fails.
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			target.swapchain.requestedExtent = {};
		}
		if (*outcome == FrameOutcome::presented) { target.lastRenderedImageIndex = imageIndex; }
		target.currentFrame = static_cast<std::uint32_t>((target.currentFrame + 1U) % frameCount);
		return outcome;
	}

	/// @brief Logs the stage and exact Vulkan result; safe pre-acquire asset errors remain retryable, Vulkan failures persist.
	auto ForwardRenderer::reportFrameFailure(const char *stage, VkResult result) -> std::unexpected<Error> {
		std::cerr << "[vve::simple] frame failed: " << stage << " failed with VkResult " << static_cast<std::int32_t>(result) << '\n';
		// Decode and capacity rejection happen before image acquisition or upload submission and leave frame sync reusable.
		if (textureUploadError_) { return std::unexpected(*textureUploadError_); }
		if (!frameError_) { frameError_ = Error::platform_error; }
		return std::unexpected(*frameError_);
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
		if (gpuTimingEnabled && target.gpuQueries) {
			target.timingWritten[frameIndex].reset();
			vkCmdResetQueryPool(commandBuffer, target.gpuQueries, frameIndex * WindowTarget::timingQueries, WindowTarget::timingQueries);
			writeGpuTimestamp(target, commandBuffer, 12);
		}
		const NativeDrawContext mainContext{.pass = usesDeferred() ? NativeDrawPass::gbuffer : NativeDrawPass::forward_opaque,
			.view = frameUniforms_.view,.projection = frameUniforms_.projection};
		if (nativeDraw_) {
			auto prepare = mainContext; prepare.prepare = true;
			if (const auto r = recordNative(target,commandBuffer,prepare); r != VK_SUCCESS) { return r; }
		}
		// Debug builds wrap every pass in a labelled region, which RenderDoc's event browser shows as a tree.
		debugLabels.startFrame();
		if constexpr (VulkanDebugLabels::compiled) {
			const char *title = target.window != nullptr ? SDL_GetWindowTitle(target.window) : nullptr;
			debugLabels.begin(commandBuffer, std::format("Frame: {}", title != nullptr && *title != '\0' ? title : "window"), DebugLabelKind::forward);
		}

		std::vector<DrawItem> items{};
		const Vec3 eye{(target.camera ? *target.camera : camera_).position.value}; // Sorts transparent items by distance.
		// Resolve scene handles and world boxes once, before any pass traverses the sorted draw list.
		if (renderInstances_ != nullptr) {
			items.reserve(renderInstances_->size());
			for (const RenderInstance &instance : *renderInstances_) {
				if (!instance.visible || (instance.sky && sky_.enabled)) { continue; } // The physical sky replaces a panorama.
				const auto uploaded = meshes.find(instance.mesh);
				const auto material = materialSlots_.find(instance.material);
				if (uploaded == meshes.end() || material == materialSlots_.end()) { continue; }
				const VulkanMesh &mesh = uploaded->second;
				const auto dynamic = target.dynamicVertices.find(instance.mesh);
				const bool blendedMaterial{materials_[material->second].emissiveFactor.w == static_cast<Scalar>(AlphaMode::blend)};
				const Vec3 position{instance.world_transform[3][0], instance.world_transform[3][1], instance.world_transform[3][2]};
				const bool terrain{instance.terrain != nullptr};
				const std::uint32_t flags{(instance.unlit ? objectFlag::unlit : 0U) | (instance.sky ? objectFlag::sky : 0U) |
					(instance.sprite ? objectFlag::sprite : 0U) | (instance.waves.w != zero() ? objectFlag::water : 0U) | (terrain ? objectFlag::terrain : 0U)};
				const bool transparent{!terrain && (blendedMaterial || instance.opacity < one() || instance.sprite)};
				Vec4 waves{instance.waves};
				if (terrain) {
					// Offsets, columns and rows are exact float integers below 2^24; the shaders round them back.
					const auto offset = terrainOffsets_.find(instance.terrain->id);
					if (offset == terrainOffsets_.end() || offset->second >= (1U << 24U)) { continue; }
					waves = Vec4{static_cast<Scalar>(offset->second), static_cast<Scalar>(instance.terrain->columns),
						static_cast<Scalar>(instance.terrain->rows), instance.terrain->cell_size};
				}
				items.push_back({.mesh = &mesh,
					.vertices = dynamic == target.dynamicVertices.end() ? mesh.vertexBuffer.buffer : dynamic->second.first[frameIndex].buffer,
					.object = {.model = instance.world_transform, .materialIndex = material->second, .flags = flags, .opacity = instance.opacity, .waves = waves},
					.worldBounds = instance.sky ? Bounds{} : instance.sprite ? spriteBounds(mesh.localBox, instance.world_transform) // Invalid bounds are never culled.
						: worldBounds(mesh.localBox, instance.world_transform),
					.castsShadow = instance.casts_shadow && !transparent && !instance.sky, .sky = instance.sky, .transparent = transparent,
					.terrain = terrain, .distance = lengthSquared(subtract(position, eye))});
				if (instance.waves.w != zero() && items.back().worldBounds.valid) {
					items.back().worldBounds.minimum.value.y -= instance.waves.x;
					items.back().worldBounds.maximum.value.y += instance.waves.x;
				}
			}
		}
		if (sky_.enabled) {
			items.push_back({.mesh = &skyMesh, .vertices = skyMesh.vertexBuffer.buffer, .object = {.model = identityMat4(), .flags = objectFlag::sky}, .sky = true});
		}
		// Order: terrain, solid (by mesh), sky, transparent (far to near); terrain first needs one pipeline switch per pass.
		const auto group = [](const DrawItem &item) { return item.transparent ? 3 : item.sky ? 2 : item.terrain ? 0 : 1; };
		std::ranges::sort(items, [&group](const DrawItem &left, const DrawItem &right) {
			if (group(left) != group(right)) { return group(left) < group(right); }
			return left.transparent ? left.distance > right.distance : std::less<const VulkanMesh *>{}(left.mesh, right.mesh);
		});
		const VulkanMesh *boundMesh{};
		const auto drawUploadedObjects = [&](std::uint32_t shadowMatrixIndex, bool shadowPass, const Mat4 &clip, RecordedPass scenePass = RecordedPass::forward_color, bool opaqueOnly = false) {
			const auto planes = frustumPlanes(clip);
			bool blending{}; // The colour pass switches to the blended pipeline at the first transparent item.
			// Callers bind the pass pipeline; terrain items switch to its tessellated twin and back.
			const VkPipeline passPipeline = shadowPass ? shadowPipeline.pipeline : scenePass == RecordedPass::gbuffer ? gbufferPipeline.pipeline : scenePass == RecordedPass::probe_capture ? probePipeline.pipeline : graphicsPipeline.pipeline;
			const VkPipeline terrainPassPipeline = shadowPass ? terrainShadowPipeline.pipeline
				: scenePass == RecordedPass::gbuffer ? terrainGbufferPipeline.pipeline : scenePass == RecordedPass::probe_capture ? probeTerrainPipeline.pipeline : terrainPipeline.pipeline;
			bool terrainBound{};
			// Bound mesh state survives dynamic-rendering boundaries and pipeline changes.
			for (const auto &item : items) {
				const bool opaque = !item.sky && !item.transparent;
				if (opaqueOnly && !opaque) { continue; }
				// Probe faces keep the shared sky draw after opaque geometry; only forward-only surfaces are excluded.
				if (!shadowPass && ((scenePass == RecordedPass::gbuffer && !opaque) || (scenePass == RecordedPass::probe_capture && item.transparent) ||
					(scenePass == RecordedPass::forward_transparency && opaque))) { continue; }
				if ((shadowPass && !item.castsShadow) || !intersectsFrustum(item.worldBounds, planes)) { continue; }
				if (item.terrain && terrainPassPipeline == VK_NULL_HANDLE) {
					if (!terrainWarned_) {
						std::cerr << "[vve::simple] terrain skipped: the device does not support tessellation shaders\n";
						terrainWarned_ = true;
					}
					continue;
				}
				if (item.terrain != terrainBound) {
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, item.terrain ? terrainPassPipeline : passPipeline);
					++lastFrameDrawStats_.pipelineBinds;
					terrainBound = item.terrain;
				}
				if (!shadowPass && item.transparent && !blending) {
					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline.pipeline);
					if (backend_ == RendererBackend::hybrid_gi) {
						// Only shared probe coefficients are sampled here; transparency never samples opaque AO or depth descriptors.
						const std::array sets{target.descriptorSets.descriptorSets[frameIndex], target.deferredSet};
						vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout.pipelineLayout, 0, 2, sets.data(), 0, nullptr);
					}
					++lastFrameDrawStats_.pipelineBinds;
					blending = true;
				}
				if (boundMesh != item.mesh) {
					const VkDeviceSize offset{};
					vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, &item.vertices, &offset);
					vkCmdBindIndexBuffer(commandBuffer, item.mesh->indexBuffer.buffer, 0U, VK_INDEX_TYPE_UINT32);
					boundMesh = item.mesh;
					++lastFrameDrawStats_.vertexBufferBinds;
				}
				auto pushConstants = item.object;
				pushConstants.shadowMatrixIndex = shadowMatrixIndex;
				vkCmdPushConstants(commandBuffer, pipelineLayout.pipelineLayout, objectPushConstantStages, 0U, sizeof(ObjectPushConstants), &pushConstants);
				vkCmdDrawIndexed(commandBuffer, item.mesh->indexCount, 1U, 0U, 0, 0U);
				++(shadowPass ? lastFrameDrawStats_.shadowDraws : lastFrameDrawStats_.forwardDraws);
			}
			// The next pass or shadow layer expects its own pipeline, as bound before this call.
			if (terrainBound) {
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, passPipeline);
				++lastFrameDrawStats_.pipelineBinds;
			}
		};
		// Only shadow-casting layers are cleared and drawn; inactive layers retain their shader-read layout.
		const VkClearValue shadowClear{.depthStencil = {.depth = 1.0F, .stencil = 0U}};
		bool shadowPipelineBound{}; // Bind state belongs to this command buffer, independently of frame diagnostics.
		const auto recordShadowLayer = [&](const ShadowMap &map, std::uint32_t layer, std::uint32_t matrixIndex, RecordedPass pass, std::string_view label,
			VkDescriptorSet captureSet = VK_NULL_HANDLE, bool initialized = true, bool draw = true) -> VkResult {
			debugLabels.begin(commandBuffer, label, DebugLabelKind::shadow);
			// One shadow pipeline/set and viewport serve every light type and layer.
			if (!shadowPipelineBound || pass == RecordedPass::probe_shadow) {
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.pipeline);
				++lastFrameDrawStats_.pipelineBinds;
				const VkViewport viewport{.width = static_cast<float>(map.extent.width), .height = static_cast<float>(map.extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
				const VkRect2D scissor{.extent = map.extent};
				vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
				vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
				const auto set = captureSet != VK_NULL_HANDLE ? captureSet : target.descriptorSets.descriptorSets[frameIndex];
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &set, 0U, nullptr);
				shadowPipelineBound = true;
			}
			const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .baseArrayLayer = layer, .layerCount = 1U};
			const VkImageMemoryBarrier beginBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = initialized ? VK_ACCESS_SHADER_READ_BIT : 0U,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
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
				.renderArea = {.offset = {0, 0}, .extent = map.extent},
				.layerCount = 1U,
				.colorAttachmentCount = 0U,
				.pDepthAttachment = &depthAttachment,
			};
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			++lastShadowLayerPassCount_;
			recordedPassOrder.push_back(pass);
			if (draw) drawUploadedObjects(matrixIndex, true, pass == RecordedPass::probe_shadow ? target.probeShadowMatrix : frameUniforms_.shadowViewProjs[matrixIndex]);
			if (draw && nativeDraw_) {
				NativeDrawContext context{.pass = NativeDrawPass::shadow,.width = map.extent.width,.height = map.extent.height};
				context.probeShadow = pass == RecordedPass::probe_shadow;
				if (context.probeShadow) {
					context.view = target.probeShadowView; context.projection = target.probeShadowProjection;
					context.lightType = 3; context.lightIndex = static_cast<std::uint32_t>(target.probeSun);
				} else {
					context.lightType = pass == RecordedPass::directional_shadow ? 3U : pass == RecordedPass::point_shadow ? 2U : 1U;
					const auto meta = std::ranges::find_if(shadowLightMeta,[&](const auto &m) { return m.light_type == context.lightType && m.first_layer == layer; });
					if (meta != shadowLightMeta.end()) { context.view = meta->view; context.projection = meta->projection; context.lightIndex = meta->light_index; }
					context.cascadeIndex = context.lightType == 3 ? layer % kNumShadowCascades : 0U;
					context.faceIndex = context.lightType == 2 ? layer % pointShadowFaceCount : 0U;
				}
				const auto r = recordNative(target,commandBuffer,context);
				boundMesh = nullptr; shadowPipelineBound = false; // External binding state cannot be reused by the next layer.
				if (r != VK_SUCCESS) { vkCmdEndRendering(commandBuffer); return r; }
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
			debugLabels.end(commandBuffer);
			return VK_SUCCESS;
		};
		const auto recordShadowMap = [&](const ShadowMap &map, std::uint32_t activeLights, std::uint32_t layersPerLight,
			std::size_t matrixBase, RecordedPass pass, std::span<const Vec4> directions = {}) -> VkResult {
			// Ambient-only lights retain a packed lighting slot but have no shadow passes.
			for (std::uint32_t light{}; light < activeLights; ++light) {
				if (!directions.empty() && directions[light].w != zero()) { continue; }
				for (std::uint32_t face{}; face < layersPerLight; ++face) {
					const std::uint32_t layer{light * layersPerLight + face};
					std::string label{};
					if constexpr (VulkanDebugLabels::compiled) {
						label = pass == RecordedPass::directional_shadow ? std::format("Shadow: directional light {} cascade {}", light, face)
							: pass == RecordedPass::point_shadow ? std::format("Shadow: point light {} face {}", light, std::array{"+X", "-X", "+Y", "-Y", "+Z", "-Z"}[face % 6U])
							: std::format("Shadow: spot light {}", light);
					}
					if (const auto r = recordShadowLayer(map, layer, static_cast<std::uint32_t>(matrixBase + layer), pass, label); r != VK_SUCCESS) { return r; }
				}
			}
			return VK_SUCCESS;
		};
		debugLabels.begin(commandBuffer, "Shadow maps", DebugLabelKind::shadow);
		if (const auto r = recordShadowMap(dirShadowArray, frameUniforms_.activeDirectionalLightCount, kNumShadowCascades, kShadowMatrixDirBase, RecordedPass::directional_shadow, frameUniforms_.directionalLightDirections); r != VK_SUCCESS) { return r; }
		if (const auto r = recordShadowMap(spotShadowArray, frameUniforms_.activeSpotLightCount, 1U, kShadowMatrixSpotBase, RecordedPass::spot_shadow, frameUniforms_.spotLightDirections); r != VK_SUCCESS) { return r; }
		if (const auto r = recordShadowMap(pointShadowArray, frameUniforms_.activePointLightCount, pointShadowFaceCount, kShadowMatrixPointBase, RecordedPass::point_shadow); r != VK_SUCCESS) { return r; }
		debugLabels.end(commandBuffer);

		if (backend_ == RendererBackend::hybrid_gi) {
			// The same caster list, masked material shader and pipelines are culled against the probe sun bounds.
			const auto &capture = target.probes.updated ? target.probes : target.coarseProbes;
			const bool drawSun = capture.updated && target.probeSun >= 0;
			if (target.probeShadowDirty && (drawSun || !target.probeShadowInitialized)) {
				const auto set = drawSun ? capture.sets.descriptorSets[frameIndex * IrradianceProbes::views] : VK_NULL_HANDLE;
				if (const auto r = recordShadowLayer(target.probeShadow, 0, target.probeSun >= 0 ? kShadowMatrixDirBase + target.probeSun * kNumShadowCascades : 0,
					RecordedPass::probe_shadow, "Shadow: probe volume sun", set, target.probeShadowInitialized, drawSun); r != VK_SUCCESS) { return r; }
				target.probeShadowInitialized = true;
				if (drawSun) { target.probeShadowGeneration = probeShadowGeneration_; ++target.probeShadowRenders; }
				if (drawSun || target.probeSun < 0) { target.probeShadowDirty = false; }
			}
			recordedPassOrder.push_back(RecordedPass::probe_capture);
			const auto r = recordProbes(target, commandBuffer, [&](const NativeDrawContext &context) {
				drawUploadedObjects(0, false, multiply(context.projection,context.view), RecordedPass::probe_capture);
				const auto recorded = recordNative(target,commandBuffer,context);
				if (nativeDraw_) { boundMesh = nullptr; }
				return recorded;
			});
			if (r != VK_SUCCESS) { return r; }
		}

		const VkImageSubresourceRange colorRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole swapchain image.
		const VkImageSubresourceRange depthRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1U, .layerCount = 1U}; // Whole depth image.
		
		const bool multisampled = sampleCount() != VK_SAMPLE_COUNT_1_BIT;
		// HDR was read by the previous blit; multisample colour was written by the previous forward pass.
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
		auto colourBeginBarriers = std::array{hdrBeginBarrier, hdrBeginBarrier};
		colourBeginBarriers[1].image = target.msaaImage.image;
		colourBeginBarriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0U, 0U, nullptr, 0U, nullptr,
			multisampled ? 2U : 1U, colourBeginBarriers.data());

		// The depth image is shared by both frames in flight: wait for the previous frame's depth writes.
		const bool deferred = usesDeferred();
		const VkImageMemoryBarrier depthBeginBarriers{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | (deferred ? VK_ACCESS_SHADER_READ_BIT : 0U),
			.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = target.depthImage.image,
			.subresourceRange = depthRange,
		};
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
									VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
									0U, 0U, nullptr, 0U, nullptr, 1U, &depthBeginBarriers);
									
		const VkRenderingAttachmentInfo colorAttachment{ // Dynamic rendering mirrors the old render-pass color clear/store ops.
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = multisampled ? target.msaaImage.imageView : target.hdrImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = multisampled ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
			.resolveImageView = multisampled ? target.hdrImage.imageView : VK_NULL_HANDLE,
			.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.color = {.float32 = {static_cast<float>(backgroundColor_.value.x), static_cast<float>(backgroundColor_.value.y),
				static_cast<float>(backgroundColor_.value.z), 1.0F}}}, // Background behind the scene and the sky.
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

		// Match the current attachments; the projection still supplies the Vulkan Y flip.
		const VkViewport viewport{.x = 0.0F, .y = 0.0F, .width = static_cast<float>(target.swapchain.extent.width), .height = static_cast<float>(target.swapchain.extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
		const VkRect2D scissor{.offset = {0, 0}, .extent = target.swapchain.extent};
		vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
		vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &target.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
		if (deferred) {
			// Reuse the shared sorted draw list, rendering only opaque geometry into the material attachments.
			std::array<VkImageMemoryBarrier, 3U> begin{};
			std::array<VkRenderingAttachmentInfo, 3U> attributes{};
			for (auto i : std::views::iota(0U, 3U)) {
				begin[i] = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_SHADER_READ_BIT, .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = target.gbufferImages[i].image, .subresourceRange = colorRange};
				attributes[i] = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
					.imageView = target.gbufferImages[i].imageView, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
			}
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0U, 0U, nullptr, 0U, nullptr, 3U, begin.data());
			auto geometryDepth = depthAttachment;
			geometryDepth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			auto geometry = renderingInfo;
			geometry.colorAttachmentCount = 3U;
			geometry.pColorAttachments = attributes.data();
			geometry.pDepthAttachment = &geometryDepth;
			recordedPassOrder.push_back(RecordedPass::gbuffer);
			debugLabels.begin(commandBuffer, "G-buffer: base + roughness, normal + metalness, emission + AO, depth", DebugLabelKind::geometry);
			vkCmdBeginRendering(commandBuffer, &geometry);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline.pipeline);
			++lastFrameDrawStats_.pipelineBinds;
			drawUploadedObjects(0U, false, viewProjection, RecordedPass::gbuffer);
			if (const auto r = recordNative(target,commandBuffer,mainContext); r != VK_SUCCESS) { vkCmdEndRendering(commandBuffer); return r; }
			if (nativeDraw_) { boundMesh = nullptr; }
			vkCmdEndRendering(commandBuffer);
			debugLabels.end(commandBuffer);
			if (nativeDraw_) { vkCmdSetViewport(commandBuffer,0,1,&viewport); vkCmdSetScissor(commandBuffer,0,1,&scissor); }

			// Publish every stored sample and its corresponding depth before the fullscreen shader reads them.
			std::array<VkImageMemoryBarrier, 4U> reads{};
			for (auto i : std::views::iota(0U, 4U)) {
				reads[i] = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = static_cast<VkAccessFlags>(i < 3U ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = i < 3U ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = i < 3U ? target.gbufferImages[i].image : target.depthImage.image,
					.subresourceRange = i < 3U ? colorRange : depthRange};
			}
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0U, 0U, nullptr, 0U, nullptr, 4U, reads.data());
			if (backend_ == RendererBackend::hybrid_gi) { recordAo(target, commandBuffer); }
			auto litColour = colorAttachment;
			litColour.resolveMode = VK_RESOLVE_MODE_NONE;
			litColour.resolveImageView = VK_NULL_HANDLE;
			litColour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			auto lighting = renderingInfo;
			lighting.pColorAttachments = &litColour;
			lighting.pDepthAttachment = nullptr;
			recordedPassOrder.push_back(RecordedPass::deferred_lighting);
			debugLabels.begin(commandBuffer, "Deferred lighting", DebugLabelKind::lighting);
			writeGpuTimestamp(target, commandBuffer, 10);
			vkCmdBeginRendering(commandBuffer, &lighting);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				backend_ == RendererBackend::hybrid_gi && giSettings.ao_enabled ? aoLightingPipeline.pipeline : lightingPipeline.pipeline);
			++lastFrameDrawStats_.pipelineBinds;
			const std::array sets{target.descriptorSets.descriptorSets[frameIndex], target.deferredSet};
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredPipelineLayout.pipelineLayout, 0U, 2U, sets.data(), 0U, nullptr);
			vkCmdDraw(commandBuffer, 3U, 1U, 0U, 0U);
			vkCmdEndRendering(commandBuffer);
			writeGpuTimestamp(target, commandBuffer, 11);
			debugLabels.end(commandBuffer);

			// Restore opaque depth for the sky and transparency; retain individual HDR samples until their final resolve.
			auto depthRestore = reads[3];
			depthRestore.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			depthRestore.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			depthRestore.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			depthRestore.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			auto colourLoad = hdrBeginBarrier;
			colourLoad.image = multisampled ? target.msaaImage.image : target.hdrImage.image;
			colourLoad.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			colourLoad.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			colourLoad.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			const std::array composeBarriers{depthRestore, colourLoad};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0U, 0U, nullptr, 0U, nullptr, 2U, composeBarriers.data());
			auto composeColour = colorAttachment;
			composeColour.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			auto composeDepth = depthAttachment;
			composeDepth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			auto compose = renderingInfo;
			compose.pColorAttachments = &composeColour;
			compose.pDepthAttachment = &composeDepth;
			recordedPassOrder.push_back(RecordedPass::forward_transparency);
			debugLabels.begin(commandBuffer, "Sky and transparency", DebugLabelKind::forward);
			vkCmdBeginRendering(commandBuffer, &compose);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
			++lastFrameDrawStats_.pipelineBinds;
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.pipelineLayout, 0U, 1U, &target.descriptorSets.descriptorSets[frameIndex], 0U, nullptr);
			drawUploadedObjects(0U, false, viewProjection, RecordedPass::forward_transparency);
			auto transparentContext = mainContext; transparentContext.pass = NativeDrawPass::forward_transparent;
			if (const auto r = recordNative(target,commandBuffer,transparentContext); r != VK_SUCCESS) { vkCmdEndRendering(commandBuffer); return r; }
			vkCmdEndRendering(commandBuffer);
			debugLabels.end(commandBuffer);
		} else {
			recordedPassOrder.push_back(RecordedPass::forward_color);
			debugLabels.begin(commandBuffer, "Forward colour", DebugLabelKind::forward);
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
			++lastFrameDrawStats_.pipelineBinds;
			drawUploadedObjects(0U, false, viewProjection, RecordedPass::forward_color, bool(nativeDraw_));
			if (nativeDraw_) {
				if (const auto r = recordNative(target,commandBuffer,mainContext); r != VK_SUCCESS) { vkCmdEndRendering(commandBuffer); return r; }
				// Restore the state needed by engine sky/transparency after the external solid draw.
				boundMesh = nullptr;
				vkCmdSetViewport(commandBuffer,0,1,&viewport); vkCmdSetScissor(commandBuffer,0,1,&scissor);
				vkCmdBindPipeline(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,graphicsPipeline.pipeline);
				vkCmdBindDescriptorSets(commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout.pipelineLayout,0,1,&target.descriptorSets.descriptorSets[frameIndex],0,nullptr);
				drawUploadedObjects(0,false,viewProjection,RecordedPass::forward_transparency);
				auto transparentContext = mainContext; transparentContext.pass = NativeDrawPass::forward_transparent;
				if (const auto r = recordNative(target,commandBuffer,transparentContext); r != VK_SUCCESS) { vkCmdEndRendering(commandBuffer); return r; }
			}
			vkCmdEndRendering(commandBuffer);
			debugLabels.end(commandBuffer);
		}

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

		
		debugLabels.begin(commandBuffer, target.postProcess ? "Post-processing" : "HDR to swapchain blit", DebugLabelKind::post);
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
		debugLabels.end(commandBuffer);

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
			debugLabels.begin(commandBuffer, "GUI", DebugLabelKind::gui);
			vkCmdBeginRendering(commandBuffer, &guiRenderingInfo);
			guiRecord_(commandBuffer);
			vkCmdEndRendering(commandBuffer);
			debugLabels.end(commandBuffer);
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
		if constexpr (VulkanDebugLabels::compiled) { debugLabels.end(commandBuffer); } // Frame
		writeGpuTimestamp(target, commandBuffer, 13);
		result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS) { return result; }

		return VK_SUCCESS;
	}

} // namespace vve::simple
