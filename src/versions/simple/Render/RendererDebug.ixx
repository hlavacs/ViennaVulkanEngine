module;
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer:RendererDebug;
import std;
import VEEngine.Types;
import VEEngine.Simple.Math;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief Retained renderer diagnostics and readback helpers for the simple forward renderer.
	*
	* Functional objects:
	* - RenderDebugSample carries CPU/GPU per-vertex diagnostics for public debug access.
	* - RenderShadowDepthSample carries CPU and optional GPU shadow-depth diagnostics.
	* - ForwardRendererDebug owns shadow readback state, retained shadow samples, PNG capture, and diagnostic accessors.
	* - StubRendererDebug exposes empty diagnostic accessors for the non-Vulkan renderer.
	*/
export namespace vve::simple {

	/// @brief Empty debug-sample type kept so the current facade can still compile against simple.
	struct RenderDebugSample {
		std::uint32_t vertex_id{};														///< Source vertex id.
		Vec3 world{zeroVec3()};															///< Stub world-space position.
		Vec4 clip{};																		///< Stub clip-space position.
		Vec4 light_clip{};																///< Stub directional-light clip position.
		Vec4 spot_light_clip{};															///< Stub spot-light clip position.
		Vec4 point_light_clip{};														///< Stub point-light clip position.
		Vec3 ndc{zeroVec3()};															///< Stub normalized device coordinate.
		Vec3 light_ndc{zeroVec3()};													///< Stub directional-light NDC.
		Vec3 spot_light_ndc{zeroVec3()};												///< Stub spot-light NDC.
		Vec3 point_light_ndc{zeroVec3()};											///< Stub point-light NDC.
		Vec3 normal{zeroVec3()};														///< Stub normal.
		Vec3 direction_to_light{zeroVec3()};										///< Stub light direction.
		Vec3 ambient_lighting{zeroVec3()};											///< Stub ambient term.
		Vec3 direct_lighting{zeroVec3()};											///< Stub direct-light term.
		Vec3 point_lighting{zeroVec3()};												///< Stub point-light term.
		Vec3 spot_lighting{zeroVec3()};												///< Stub spot-light term.
		Vec3 final_lighting{zeroVec3()};												///< Stub final-light term.
		float depth{};																		///< Stub depth value.
		float light_depth{};																///< Stub directional-light depth.
		float spot_light_depth{};														///< Stub spot-light depth.
		float point_light_depth{};														///< Stub point-light depth.
		float sampled_shadow_depth{};													///< Stub sampled shadow depth.
		float shadow_depth_delta{};													///< Stub shadow delta.
		float shadow_bias{};																///< Stub shadow bias.
		float shadow_factor{};															///< Stub shadow factor.
		float sampled_spot_shadow_depth{};											///< Stub sampled spot shadow depth.
		float spot_shadow_depth_delta{};												///< Stub spot shadow delta.
		float spot_shadow_bias{};														///< Stub spot shadow bias.
		float spot_shadow_factor{};													///< Stub spot shadow factor.
		float sampled_point_shadow_depth{};											///< Stub sampled point shadow depth.
		float point_shadow_depth_delta{};											///< Stub point shadow delta.
		float point_shadow_bias{};														///< Stub point shadow bias.
		float point_shadow_factor{};													///< Stub point shadow factor.
		std::uint32_t point_shadow_face{};											///< Stub point shadow face.
		float n_dot_l{};																	///< Stub Lambert term.
		bool inside_light{};																///< Stub directional-light inclusion.
		bool inside_spot_light{};														///< Stub spot-light inclusion.
		bool inside_point_light{};														///< Stub point-light inclusion.
		bool valid{};																		///< Stub samples are never valid.
	};

	/// @brief Empty shadow proof type kept so the current facade can still compile against simple.
	struct RenderShadowDepthSample {
		std::uint32_t triangle_id{};													///< Source triangle id.
		std::uint32_t face_index{};													///< Shadow face id.
		Vec3 world{zeroVec3()};															///< World-space sample point.
		Vec3 light_ndc{zeroVec3()};													///< Light-space sample point.
		std::uint32_t pixel_x{};														///< Shadow-map x texel.
		std::uint32_t pixel_y{};														///< Shadow-map y texel.
		float expected_depth{};															///< CPU expected depth.
		float bias{};																		///< CPU compare bias.
		float shadow_factor{};															///< CPU shadow factor.
		float gpu_depth{};																///< GPU depth, absent in simple stubs.
		float error{};																		///< Absolute mismatch.
		bool has_gpu{};																	///< False for simple stubs.
		bool valid{};																		///< Stub samples are never valid.
	};

	/// @brief Shared debug/readback surface mixed into the concrete forward renderer.
	template<typename Renderer>
	struct ForwardRendererDebug {
		VulkanDepthReadback spotShadowDepthReadback{}; ///< Owned one-layer spot shadow depth readback for debug samples.
		VulkanDepthReadback dirShadowDepthReadback{}; ///< Owned one-layer directional shadow depth readback for debug samples.
		VulkanDepthReadback pointShadowDepthReadback{}; ///< Owned one-layer point shadow depth readback for debug samples.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame readback capture.

		/// @brief Reports the number of retained debug samples for the forward renderer.
		[[nodiscard]] std::size_t sceneDebugSampleCount() const { return 0; }
		/// @brief Returns a CPU debug sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneCpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns a GPU debug sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneGpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns clip-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugClipError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns depth debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point light-space debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns lighting debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-sample debug error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained directional shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t sceneShadowDepthSampleCount() const { return directionalShadowDepthSampleCountStorage(); }
		/// @brief Returns a directional shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const {
			if (index >= directionalShadowDepthSampleCountStorage()) { return {}; }
			return directionalShadowDepthSampleStorage()[index];
		}
		/// @brief Returns directional shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained spot shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t sceneSpotShadowDepthSampleCount() const { return spotShadowDepthSampleCountStorage(); }
		/// @brief Returns a spot shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneSpotShadowDepthSample(std::size_t index) const {
			if (index >= spotShadowDepthSampleCountStorage()) { return {}; }
			return spotShadowDepthSampleStorage()[index];
		}
		/// @brief Returns spot shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained point shadow-depth samples for the forward renderer.
		[[nodiscard]] std::size_t scenePointShadowDepthSampleCount() const { return pointShadowDepthSampleCountStorage(); }
		/// @brief Returns a point shadow-depth sample when the forward renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> scenePointShadowDepthSample(std::size_t index) const {
			if (index >= pointShadowDepthSampleCountStorage()) { return {}; }
			return pointShadowDepthSampleStorage()[index];
		}
		/// @brief Returns point shadow-depth error diagnostics retained by the forward renderer.
		[[nodiscard]] std::optional<float> scenePointShadowDepthError(std::size_t index) const {
			if (index >= pointShadowDepthSampleCountStorage()) { return {}; }
			return pointShadowDepthSampleStorage()[index].error;
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
			VkResult result = readback.create(renderer.physicalDevice.physicalDevice, renderer.device.device,
														 renderer.device.graphicsQueue, renderer.commandPool.commandPool,
														 renderer.swapchain.extent, renderer.swapchain.imageFormat);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

			result = vkDeviceWaitIdle(renderer.device.device);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
			const auto image = renderer.swapchain.images[*renderer.lastRenderedImageIndex];
			result = readback.capture(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }

			const auto output = output_path.string();
			if (!writeReadbackPng(readback.pixelBytes(), renderer.swapchain.extent, renderer.swapchain.imageFormat, output)) {
				return std::unexpected(Error::io_error);
			}
			return {};
		}

		[[nodiscard]] static std::array<RenderShadowDepthSample, 1U> &directionalShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, 1U> samples{}; ///< CPU-only directional shadow sample.
			return samples;
		}

		[[nodiscard]] static std::size_t &directionalShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only directional shadow samples.
			return count;
		}

		[[nodiscard]] static std::array<RenderShadowDepthSample, kMaxShadowedSpotLights> &spotShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, kMaxShadowedSpotLights> samples{}; ///< CPU-only spot shadow samples.
			return samples;
		}

		[[nodiscard]] static std::size_t &spotShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only spot shadow samples.
			return count;
		}

		[[nodiscard]] static std::array<RenderShadowDepthSample, kMaxShadowedPointLights> &pointShadowDepthSampleStorage() {
			static std::array<RenderShadowDepthSample, kMaxShadowedPointLights> samples{}; ///< CPU-only point shadow samples.
			return samples;
		}

		[[nodiscard]] static std::size_t &pointShadowDepthSampleCountStorage() {
			static std::size_t count{}; ///< Number of valid CPU-only point shadow samples.
			return count;
		}

		/**
			* @brief Reads one rendered spot shadow texel per active debug sample into the retained diagnostics.
			*/
		void fillSpotShadowGpuDepthSamples() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (renderer.device.device == VK_NULL_HANDLE || renderer.spotShadowArray.image == VK_NULL_HANDLE) { return; }
			if (spotShadowDepthReadback.extent.width == 0U || spotShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(renderer.device.device) != VK_SUCCESS) { return; }

			const std::size_t sampleCount{std::min({renderer.spotLightViewProjCount, spotShadowDepthSampleCountStorage(), renderer.spotShadowArray.layerFramebuffers.size()})}; // Only rendered spot slots are read back.
			for (std::size_t spotIndex{}; spotIndex < sampleCount; ++spotIndex) {
				RenderShadowDepthSample &sample = spotShadowDepthSampleStorage()[spotIndex]; // Existing CPU sample owns NDC and expected depth.
				const auto texel = spotShadowDebugTexel(sample.light_ndc);
				if (spotShadowDepthReadback.capture(renderer.spotShadowArray.image, static_cast<std::uint32_t>(spotIndex)) != VK_SUCCESS) { continue; }

				const std::optional<float> depth = spotShadowDepthReadback.depthAt(texel.first, texel.second);
				if (!depth) { continue; }
				sample.pixel_x = texel.first;
				sample.pixel_y = texel.second;
				sample.gpu_depth = *depth;
				sample.has_gpu = true;
				sample.error = std::abs(sample.gpu_depth - sample.expected_depth);
			}
		}

		/**
			* @brief Reads the rendered directional shadow texel into the retained diagnostics.
			*/
		void fillDirectionalShadowGpuDepthSamples() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (renderer.device.device == VK_NULL_HANDLE || renderer.dirShadowArray.image == VK_NULL_HANDLE) { return; }
			if (dirShadowDepthReadback.extent.width == 0U || dirShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(renderer.device.device) != VK_SUCCESS) { return; }

			RenderShadowDepthSample &sample = directionalShadowDepthSampleStorage()[0]; // Single directional light uses layer zero.
			const auto texel = spotShadowDebugTexel(sample.light_ndc);
			if (dirShadowDepthReadback.capture(renderer.dirShadowArray.image, 0U) != VK_SUCCESS) { return; }

			const std::optional<float> depth = dirShadowDepthReadback.depthAt(texel.first, texel.second);
			if (!depth) { return; }
			sample.pixel_x = texel.first;
			sample.pixel_y = texel.second;
			sample.gpu_depth = *depth;
			sample.has_gpu = true;
			sample.error = std::abs(sample.gpu_depth - sample.expected_depth);
		}

		/**
			* @brief Reads one rendered point shadow texel per active debug sample into the retained diagnostics.
			*/
		void fillPointShadowGpuDepthSamples() {
			auto &renderer = static_cast<Renderer &>(*this);
			if (renderer.device.device == VK_NULL_HANDLE || renderer.pointShadowArray.image == VK_NULL_HANDLE) { return; }
			if (pointShadowDepthReadback.extent.width == 0U || pointShadowDepthReadback.extent.height == 0U) { return; }
			if (vkDeviceWaitIdle(renderer.device.device) != VK_SUCCESS) { return; }

			constexpr std::uint32_t firstPointShadowLayer{static_cast<std::uint32_t>(kMaxShadowedSpotLights)}; // CPU samples store the combined spot-plus-point layer.
			const std::size_t sampleCount{std::min(pointShadowDepthSampleCountStorage(), renderer.pointShadowArray.layerFramebuffers.size())}; // Only retained point-light samples are read.
			for (std::size_t pointIndex{}; pointIndex < sampleCount; ++pointIndex) {
				RenderShadowDepthSample &sample = pointShadowDepthSampleStorage()[pointIndex]; // Existing CPU sample owns selected face, layer, and NDC.
				if (sample.pixel_x < firstPointShadowLayer) { continue; }
				const std::uint32_t pointArrayLayer{sample.pixel_x - firstPointShadowLayer}; // Point texture array stores only point faces.
				if (pointArrayLayer >= renderer.pointShadowArray.layerFramebuffers.size()) { continue; }

				const auto texel = spotShadowDebugTexel(sample.light_ndc);
				if (pointShadowDepthReadback.capture(renderer.pointShadowArray.image, pointArrayLayer) != VK_SUCCESS) { continue; }

				const std::optional<float> depth = pointShadowDepthReadback.depthAt(texel.first, texel.second);
				if (!depth) { continue; }
				sample.pixel_y = texel.second;
				sample.gpu_depth = *depth;
				sample.error = sample.expected_depth - sample.gpu_depth;
				sample.has_gpu = true;
				sample.shadow_factor = sample.light_ndc.z - sample.bias > sample.gpu_depth ? 0.35F : 1.0F;
			}
		}

		/**
			* @brief Converts shader shadow-map NDC coordinates to one clamped integer texel.
			*
			* @param lightNdc Existing debug point in light normalized device coordinates.
			* @return X and Y texel coordinates inside the fixed shadow-map resolution.
			*/
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> spotShadowDebugTexel(Vec3 lightNdc) {
			const auto toTexel = [](Scalar ndc) {
				if (!std::isfinite(ndc)) { return 0U; }
				const Scalar uv = std::clamp(ndc * static_cast<Scalar>(0.5) + static_cast<Scalar>(0.5), zero(), one());
				const Scalar scaled = uv * static_cast<Scalar>(ShadowMap::resolution);
				return static_cast<std::uint32_t>(std::min<std::uint64_t>(static_cast<std::uint64_t>(scaled), ShadowMap::resolution - 1U));
			};
			return {toTexel(lightNdc.x), toTexel(lightNdc.y)};
		}
	};

	/// @brief Empty debug/readback surface mixed into the stub renderer.
	struct StubRendererDebug {
		/// @brief Reports the number of retained debug samples for the stub renderer.
		[[nodiscard]] std::size_t sceneDebugSampleCount() const { return 0; }
		/// @brief Returns a CPU debug sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneCpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns a GPU debug sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderDebugSample> sceneGpuDebugSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns clip-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugClipError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns depth debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point light-space debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointLightSpaceError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns lighting debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugLightingError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugSpotShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-sample debug error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneDebugPointShadowSampleError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained directional shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t sceneShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a directional shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns directional shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained spot shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t sceneSpotShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a spot shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneSpotShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns spot shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthError(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports the number of retained point shadow-depth samples for the stub renderer.
		[[nodiscard]] std::size_t scenePointShadowDepthSampleCount() const { return 0; }
		/// @brief Returns a point shadow-depth sample when the stub renderer has retained one.
		[[nodiscard]] std::optional<RenderShadowDepthSample> scenePointShadowDepthSample(std::size_t index) const { (void)index; return {}; }
		/// @brief Returns point shadow-depth error diagnostics retained by the stub renderer.
		[[nodiscard]] std::optional<float> scenePointShadowDepthError(std::size_t index) const { (void)index; return {}; }

		/// @brief Reports that the stub has no swapchain image to capture.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			(void)output_path;
			return std::unexpected(Error::missing_object);
		}
	};

} // namespace vve::simple
