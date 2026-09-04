module;
#include <new>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer:RendererDebug;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;
import :RendererShadowPrep;

/**
	* @file
	* @brief Shadow-depth diagnostics and PNG capture for the simple forward renderer.
	*
	* Functional objects:
	* - RenderShadowDepthSample compares one CPU-projected world point against the rendered shadow-map depth.
	* - ForwardRendererDebug records one sample per shadow-casting light each frame, optionally reads the matching
	*   GPU texel back, and captures the presented frame as a PNG.
	*/
export namespace vve::simple {

	/// @brief One CPU/GPU shadow-depth comparison point; the world point is the origin for every light.
	struct RenderShadowDepthSample {
		std::uint32_t light_type{};				///< ShadowLightMeta light type: 1 spot, 2 point, 3 directional.
		std::uint32_t light_index{};				///< Dense index of the light inside its packed shadow slots.
		std::uint32_t face_index{};				///< Point-light cube face (+X,-X,+Y,-Y,+Z,-Z), 0 for other lights.
		std::uint32_t layer{};						///< Layer of the light type's shadow array that this sample reads.
		Vec3 world{zeroVec3()};						///< World-space sample point.
		Vec3 light_ndc{zeroVec3()};					///< Sample point in light normalized device coordinates.
		std::uint32_t pixel_x{};					///< Shadow-map texel x, valid when has_gpu is true.
		std::uint32_t pixel_y{};					///< Shadow-map texel y, valid when has_gpu is true.
		float expected_depth{};						///< CPU light-space depth (light_ndc.z).
		float bias{};									///< Shader-side compare bias mirrored on the CPU.
		float shadow_factor{1.0F};					///< 0.35 when the GPU texel occludes the point, otherwise 1.
		float gpu_depth{-1.0F};						///< Depth read back from the shadow map, valid when has_gpu is true.
		float error{-1.0F};							///< Absolute difference between expected_depth and gpu_depth.
		bool has_gpu{};								///< True once the GPU texel was read back.
	};

	/// @brief Diagnostics mixed into the forward renderer: per-light shadow samples and frame capture.
	template<typename Renderer>
	struct ForwardRendererDebug {
		static constexpr float occludedShadowFactor{0.35F};			///< Shader partial-shadow floor.
		static constexpr float directionalCompareBias{0.00005F};	///< Shader-side bias of the nearest directional cascade.

		std::vector<RenderShadowDepthSample> shadowDepthSamples{};	///< One sample per shadow-casting light, rebuilt every frame.
		VulkanReadback shadowDepthReadback{};						///< Single shadow-map layer readback shared by all light types.
		std::optional<VkResult> lastReadbackCaptureResult{};			///< Result from the optional in-frame color readback.
		bool gpuDebugReadback_{false};										///< False during normal rendering to avoid per-frame GPU stalls.

		/// @brief Enables the per-frame GPU shadow-depth readback for verification runs.
		void setGpuDebugReadback(bool enabled) { gpuDebugReadback_ = enabled; }

		/// @brief Projects the world origin through every active shadow matrix and stores the CPU expectations.
		void recordShadowDepthSamples(const ForwardRendererShadowFrame &shadowFrame) {
			auto &renderer = static_cast<Renderer &>(*this);
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
				const auto layer = static_cast<std::uint32_t>(point * ForwardRendererShadowPrep<Renderer>::pointShadowFaceCount + face);
				add(2U, static_cast<std::uint32_t>(point), face, layer, project(shadowFrame.shadowViewProjs[kShadowMatrixPointBase + layer]), shadowFrame.shadowCompareBias);
			}
			if (shadowFrame.activeDirectionalLightCount != 0U) {
				add(3U, 0U, 0U, 0U, project(shadowFrame.shadowViewProjs[kShadowMatrixDirBase]), directionalCompareBias); ///< Light zero, nearest cascade.
			}
		}

		/// @brief Reads the rendered shadow-map texel of every recorded sample when GPU readback is enabled.
		void fillShadowDepthSamplesFromGpu() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (!gpuDebugReadback_ || shadowDepthSamples.empty() || renderer.device.device == VK_NULL_HANDLE) { return; }
			if (shadowDepthReadback.extent.width == 0U || vkDeviceWaitIdle(renderer.device.device) != VK_SUCCESS) { return; }
			for (RenderShadowDepthSample &sample : shadowDepthSamples) {
				const ShadowMap &map = sample.light_type == 1U ? renderer.spotShadowArray : sample.light_type == 2U ? renderer.pointShadowArray : renderer.dirShadowArray;
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
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			auto &renderer = static_cast<Renderer &>(*this);
			if (!renderer.lastRenderedImageIndex) { return std::unexpected(Error::missing_object); }
			if (*renderer.lastRenderedImageIndex >= renderer.swapchain.images.size()) {
				return std::unexpected(Error::internal_error);
			}

			if (const auto parent = output_path.parent_path(); !parent.empty()) {
				auto error = std::error_code{};
				std::filesystem::create_directories(parent, error);
				if (error) { return std::unexpected(Error::io_error); }
			}

			auto readback = VulkanReadback{};
			VkResult result = readback.create(renderer.allocator, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool,
														 renderer.swapchain.extent, renderer.swapchain.imageFormat);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

			// Capture a newly acquired frame before presentation releases the swapchain image.
			result = vkDeviceWaitIdle(renderer.device.device);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
			renderer.renderFrame(&readback);
			if (!renderer.lastReadbackCaptureResult || *renderer.lastReadbackCaptureResult != VK_SUCCESS) {
				return std::unexpected(Error::platform_error);
			}

			const auto output = output_path.string();
			if (!writeReadbackPng(readback.pixelBytes(), renderer.swapchain.extent, renderer.swapchain.imageFormat, output)) {
				return std::unexpected(Error::io_error);
			}
			return {};
		}

	private:
		/// @brief Converts light NDC x/y to one clamped shadow-map texel.
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> shadowTexel(Vec3 lightNdc) {
			const auto toTexel = [](Scalar ndc) {
				if (!std::isfinite(ndc)) { return 0U; }
				const Scalar uv = std::clamp(ndc * static_cast<Scalar>(0.5) + static_cast<Scalar>(0.5), zero(), one());
				return static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(uv * static_cast<Scalar>(ShadowMap::resolution)), ShadowMap::resolution - 1U));
			};
			return {toTexel(lightNdc.x), toTexel(lightNdc.y)};
		}
	};

} // namespace vve::simple
