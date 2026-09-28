module;
#include <vulkan/vulkan_core.h>
#include "../shaders/simple_shared.h"

module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/// @file
/// @brief ForwardRenderer diagnostics: shadow-depth samples, optional GPU readback, and PNG frame capture.

namespace vve::simple {

	namespace {
		/// @brief Mirrors the shader's world-space receiver offset at a perspective light's axial depth.
		[[nodiscard]] float perspectiveShadowBias(float farPlane, float axialDepth) {
			constexpr float nearPlane{VVE_SHADOW_NEAR_PLANE};
			constexpr float worldBias{VVE_PERSPECTIVE_SHADOW_WORLD_BIAS};
			const float biasedDepth = std::max(axialDepth, nearPlane + worldBias);
			const float depthScale = (farPlane * nearPlane) / std::max(farPlane - nearPlane, VVE_PERSPECTIVE_SHADOW_EPSILON);
			return std::max(depthScale * worldBias / (biasedDepth * std::max(biasedDepth - worldBias, VVE_PERSPECTIVE_SHADOW_EPSILON)), VVE_PERSPECTIVE_SHADOW_MIN_BIAS);
		}
	} // namespace

	/// @brief Reports whether this renderer has a live Debug validation messenger.
	bool ForwardRenderer::validationActive() const { return instance.validationActive(); }

	/// @brief Returns Vulkan validation ERROR callbacks since instance creation, including synchronization hazards.
	std::uint64_t ForwardRenderer::validationErrorCount() const { return instance.validationErrorCount(); }

	/// @brief Projects the world origin through every active shadow matrix and stores the CPU expectations.
	void ForwardRenderer::recordShadowDepthSamples() {
		shadowDepthSamples.clear();
		const Vec3 origin{zeroVec3()};
		const auto project = [&origin](const Mat4 &viewProj) {
			return multiply(viewProj, Vec4{origin.x, origin.y, origin.z, one()});
		};
		const auto add = [&](std::uint32_t type, std::uint32_t index, std::uint32_t face, std::uint32_t layer, Vec4 clip, float bias) {
			const Scalar invW{clip.w != zero() ? one() / clip.w : zero()};
			const Vec3 ndc{clip.x * invW, clip.y * invW, clip.z * invW};
			shadowDepthSamples.push_back(RenderShadowDepthSample{.light_type = type, .light_index = index, .face_index = face, .layer = layer,
																				  .world = origin, .light_ndc = ndc, .expected_depth = ndc.z, .bias = bias});
		};
		// Perspective clip.w is the axial depth used by the shader's receiver bias, before the divide.
		for (std::size_t spot{}; spot < frameUniforms_.activeSpotLightCount; ++spot) {
			if (frameUniforms_.spotLightDirections[spot].w != zero()) { continue; } // Ambient-only slots have no rendered depth.
			const Vec4 clip = project(frameUniforms_.shadowViewProjs[kShadowMatrixSpotBase + spot]);
			const float range = std::max(frameUniforms_.spotLightPositionRanges[spot].w, VVE_PERSPECTIVE_SHADOW_EPSILON); ///< Same spot-range guard as the fragment shader.
			add(1U, static_cast<std::uint32_t>(spot), 0U, static_cast<std::uint32_t>(spot), clip,
				perspectiveShadowBias(range, clip.w));
		}
		for (std::size_t point{}; point < frameUniforms_.activePointLightCount; ++point) {
			const Vec4 &position = frameUniforms_.pointLightPositionRanges[point];
			const Vec3 toOrigin{subtract(origin, Vec3{position.x, position.y, position.z})};
			const Vec3 magnitude{std::abs(toOrigin.x), std::abs(toOrigin.y), std::abs(toOrigin.z)};
			const std::uint32_t face{magnitude.x >= magnitude.y && magnitude.x >= magnitude.z ? (toOrigin.x >= zero() ? 0U : 1U)
												: magnitude.y >= magnitude.z ? (toOrigin.y >= zero() ? 2U : 3U)
																					 : (toOrigin.z >= zero() ? 4U : 5U)}; ///< Shader dominant-axis face order.
			const auto layer = static_cast<std::uint32_t>(point * pointShadowFaceCount + face);
			const Vec4 clip = project(frameUniforms_.shadowViewProjs[kShadowMatrixPointBase + layer]);
			add(2U, static_cast<std::uint32_t>(point), face, layer, clip, perspectiveShadowBias(position.w, clip.w));
		}
		if (frameUniforms_.activeDirectionalLightCount != 0U && frameUniforms_.directionalLightDirections[0].w == zero()) {
			add(3U, 0U, 0U, 0U, project(frameUniforms_.shadowViewProjs[kShadowMatrixDirBase]), directionalCompareBias); ///< Light zero, nearest cascade.
		}
	}

	/// @brief Reads the rendered shadow-map texel of every recorded sample when GPU readback is enabled.
	void ForwardRenderer::fillShadowDepthSamplesFromGpu() {
		if (!gpuDebugReadback_ || shadowDepthSamples.empty() || device.device == VK_NULL_HANDLE) { return; }
		if (shadowDepthReadback.extent.width == 0U || waitDeviceIdle() != VK_SUCCESS) { return; }
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
	auto ForwardRenderer::captureFrameToPng(WindowTarget &target, const std::filesystem::path &output_path) -> std::expected<void, Error> {
		if (!target.lastRenderedImageIndex) { return std::unexpected(Error::missing_object); }
		if (*target.lastRenderedImageIndex >= target.swapchain.images.size()) {
			return std::unexpected(Error::internal_error);
		}

		if (const auto parent = output_path.parent_path(); !parent.empty()) {
			auto error = std::error_code{};
			std::filesystem::create_directories(parent, error);
			if (error) { return std::unexpected(Error::io_error); }
		}

		auto readback = VulkanReadback{};
		VkResult result = readback.create(allocator, device.device, device.graphicsQueue, commandPool.commandPool,
													 target.swapchain.extent, target.swapchain.imageFormat);
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

		// Capture a newly acquired frame before presentation releases the swapchain image.
		result = waitDeviceIdle();
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
		(void)drawFrame(target, &readback); // Capture success depends on the readback, which completes before presentation.
		if (const auto error = textureUploadError()) { return std::unexpected(*error); }
		if (!target.lastReadbackCaptureResult || *target.lastReadbackCaptureResult != VK_SUCCESS) {
			return std::unexpected(Error::platform_error);
		}

		auto png_pixels = std::vector<std::byte>{};
		auto pixels = readback.pixelBytes();
		if (target.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_UNORM || target.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM) {
			// Encode linear UNORM swapchain bytes with the transfer curve expected by PNG viewers.
			png_pixels.assign(pixels.begin(), pixels.end());
			for (std::size_t offset{}; offset < png_pixels.size(); offset += 4U) {
				for (std::size_t channel{}; channel < 3U; ++channel) {
					const auto linear = static_cast<double>(std::to_integer<unsigned char>(png_pixels[offset + channel])) / 255.0;
					const auto srgb = linear <= 0.0031308 ? 12.92 * linear : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
					png_pixels[offset + channel] = static_cast<std::byte>(std::lround(std::clamp(srgb, 0.0, 1.0) * 255.0));
				}
			}
			pixels = std::span<const std::byte>{png_pixels};
		}

		const auto output = output_path.string();
		if (!writeReadbackPng(pixels, target.swapchain.extent, target.swapchain.imageFormat, output)) {
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
