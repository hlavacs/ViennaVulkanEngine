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
/// @brief ForwardRenderer diagnostics: shadow-depth samples, optional GPU readback, and PNG frame capture.

namespace vve::simple {

	/// @brief Projects the world origin through every active shadow matrix and stores the CPU expectations.
	void ForwardRenderer::recordShadowDepthSamples(const ForwardRendererShadowFrame &shadowFrame) {
		shadowDepthSamples.clear();
		const Vec3 origin{zeroVec3()};
		const auto project = [&origin](const Mat4 &viewProj) {
			const Vec4 clip{multiply(viewProj, Vec4{origin.x, origin.y, origin.z, one()})};
			const Scalar invW{clip.w != zero() ? one() / clip.w : zero()};
			return Vec3{clip.x * invW, clip.y * invW, clip.z * invW};
		};
		const auto add = [&](std::uint32_t type, std::uint32_t index, std::uint32_t face, std::uint32_t layer, Vec3 ndc, float bias) {
			shadowDepthSamples.push_back(RenderShadowDepthSample{.light_type = type, .light_index = index, .face_index = face, .layer = layer,
																				  .world = origin, .light_ndc = ndc, .expected_depth = ndc.z, .bias = bias});
		};
		for (std::size_t spot{}; spot < shadowFrame.activeSpotLightCount; ++spot) {
			add(1U, static_cast<std::uint32_t>(spot), 0U, static_cast<std::uint32_t>(spot), project(shadowFrame.shadowViewProjs[kShadowMatrixSpotBase + spot]), shadowFrame.shadowCompareBias);
		}
		for (std::size_t point{}; point < shadowFrame.activePointLightCount; ++point) {
			const Vec4 &position = shadowFrame.pointLightPositionRanges[point];
			const Vec3 toOrigin{subtract(origin, Vec3{position.x, position.y, position.z})};
			const Vec3 magnitude{std::abs(toOrigin.x), std::abs(toOrigin.y), std::abs(toOrigin.z)};
			const std::uint32_t face{magnitude.x >= magnitude.y && magnitude.x >= magnitude.z ? (toOrigin.x >= zero() ? 0U : 1U)
												: magnitude.y >= magnitude.z ? (toOrigin.y >= zero() ? 2U : 3U)
																					 : (toOrigin.z >= zero() ? 4U : 5U)}; ///< Shader dominant-axis face order.
			const auto layer = static_cast<std::uint32_t>(point * pointShadowFaceCount + face);
			add(2U, static_cast<std::uint32_t>(point), face, layer, project(shadowFrame.shadowViewProjs[kShadowMatrixPointBase + layer]), shadowFrame.shadowCompareBias);
		}
		if (shadowFrame.activeDirectionalLightCount != 0U) {
			add(3U, 0U, 0U, 0U, project(shadowFrame.shadowViewProjs[kShadowMatrixDirBase]), directionalCompareBias); ///< Light zero, nearest cascade.
		}
	}

	/// @brief Reads the rendered shadow-map texel of every recorded sample when GPU readback is enabled.
	void ForwardRenderer::fillShadowDepthSamplesFromGpu() {
		if (!gpuDebugReadback_ || shadowDepthSamples.empty() || device.device == VK_NULL_HANDLE) { return; }
		if (shadowDepthReadback.extent.width == 0U || vkDeviceWaitIdle(device.device) != VK_SUCCESS) { return; }
		for (RenderShadowDepthSample &sample : shadowDepthSamples) {
			const ShadowMap &map = sample.light_type == 1U ? spotShadowArray : sample.light_type == 2U ? pointShadowArray : dirShadowArray;
			if (map.image == VK_NULL_HANDLE || sample.layer >= map.layerViews.size()) { continue; }
			if (shadowDepthReadback.capture(map.image, sample.layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != VK_SUCCESS) { continue; }
			const auto [x, y] = shadowTexel(sample.light_ndc);
			const std::optional<float> depth = shadowDepthReadback.depthAt(x, y);
			if (!depth) { continue; }
			sample.pixel_x = x;
			sample.pixel_y = y;
			sample.gpu_depth = *depth;
			sample.has_gpu = true;
			sample.error = std::abs(sample.expected_depth - sample.gpu_depth);
			sample.shadow_factor = sample.light_ndc.z - sample.bias > sample.gpu_depth ? occludedShadowFactor : 1.0F;
		}
	}

	/// @brief Copies the last presented swapchain image and writes it as a deterministic PNG.
	auto ForwardRenderer::captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
		if (!lastRenderedImageIndex) { return std::unexpected(Error::missing_object); }
		if (*lastRenderedImageIndex >= swapchain.images.size()) {
			return std::unexpected(Error::internal_error);
		}

		if (const auto parent = output_path.parent_path(); !parent.empty()) {
			auto error = std::error_code{};
			std::filesystem::create_directories(parent, error);
			if (error) { return std::unexpected(Error::io_error); }
		}

		auto readback = VulkanReadback{};
		VkResult result = readback.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool,
													 swapchain.extent, swapchain.imageFormat);
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

		// Capture a newly acquired frame before presentation releases the swapchain image.
		result = vkDeviceWaitIdle(device.device);
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
		renderFrame(&readback);
		if (!lastReadbackCaptureResult || *lastReadbackCaptureResult != VK_SUCCESS) {
			return std::unexpected(Error::platform_error);
		}

		const auto output = output_path.string();
		if (!writeReadbackPng(readback.pixelBytes(), swapchain.extent, swapchain.imageFormat, output)) {
			return std::unexpected(Error::io_error);
		}
		return {};
	}

	/// @brief Converts light NDC x/y to one clamped shadow-map texel.
	std::pair<std::uint32_t, std::uint32_t> ForwardRenderer::shadowTexel(Vec3 lightNdc) {
		const auto toTexel = [](Scalar ndc) {
			if (!std::isfinite(ndc)) { return 0U; }
			const Scalar uv = std::clamp(ndc * static_cast<Scalar>(0.5) + static_cast<Scalar>(0.5), zero(), one());
			return static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(uv * static_cast<Scalar>(ShadowMap::resolution)), ShadowMap::resolution - 1U));
		};
		return {toTexel(lightNdc.x), toTexel(lightNdc.y)};
	}

} // namespace vve::simple
