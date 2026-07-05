export module VEEngine.Simple:RenderSystemDebug;
import std;
import VEEngine.Simple.Error;
import VEEngine.Simple.Renderer;

/**
	* @file
	* @brief Public render-system diagnostic forwarding for retained renderer debug data.
	*
	* Functional objects:
	* - RenderSystemDebug forwards shadow-depth samples and PNG capture requests to the selected renderer.
	*/
export namespace vve::simple {

	/// @brief Shared diagnostic forwarding surface mixed into the public render system.
	template<typename System>
	struct RenderSystemDebug {
		/// @brief Returns the retained directional shadow-depth debug sample count.
		[[nodiscard]] std::size_t sceneShadowDepthSampleCount() const {
			return system().forward().sceneShadowDepthSampleCount();
		}

		/// @brief Returns one retained directional shadow-depth debug sample.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneShadowDepthSample(std::size_t index) const {
			return system().forward().sceneShadowDepthSample(index);
		}

		/// @brief Returns the retained spot shadow-depth debug sample count.
		[[nodiscard]] std::size_t sceneSpotShadowDepthSampleCount() const {
			return system().forward().sceneSpotShadowDepthSampleCount();
		}

		/// @brief Returns one retained spot shadow-depth debug sample.
		[[nodiscard]] std::optional<RenderShadowDepthSample> sceneSpotShadowDepthSample(std::size_t index) const {
			return system().forward().sceneSpotShadowDepthSample(index);
		}

		/// @brief Returns downloaded GPU depth for one retained spot shadow-depth sample.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthGpuDepth(std::size_t index) const {
			const auto sample = system().forward().sceneSpotShadowDepthSample(index);
			if (!sample || !sample->has_gpu) { return std::nullopt; }
			return sample->gpu_depth;
		}

		/// @brief Reports whether one retained spot shadow-depth sample has GPU data.
		[[nodiscard]] std::optional<bool> sceneSpotShadowDepthHasGpu(std::size_t index) const {
			const auto sample = system().forward().sceneSpotShadowDepthSample(index);
			if (!sample || !sample->has_gpu) { return std::nullopt; }
			return sample->has_gpu;
		}

		/// @brief Returns absolute CPU/GPU depth error for one retained spot shadow-depth sample.
		[[nodiscard]] std::optional<float> sceneSpotShadowDepthError(std::size_t index) const {
			const auto sample = system().forward().sceneSpotShadowDepthSample(index);
			if (!sample || !sample->has_gpu) { return std::nullopt; }
			return sample->error;
		}

		/// @brief Returns the retained point shadow-depth debug sample count.
		[[nodiscard]] std::size_t scenePointShadowDepthSampleCount() const {
			return system().forward().scenePointShadowDepthSampleCount();
		}

		/// @brief Returns one retained point shadow-depth debug sample.
		[[nodiscard]] std::optional<RenderShadowDepthSample> scenePointShadowDepthSample(std::size_t index) const {
			return system().forward().scenePointShadowDepthSample(index);
		}

		/// @brief Returns downloaded GPU depth for one retained point shadow-depth sample.
		[[nodiscard]] std::optional<float> scenePointShadowDepthGpuDepth(std::size_t index) const {
			const auto sample = system().forward().scenePointShadowDepthSample(index);
			if (!sample || !sample->has_gpu) { return std::nullopt; }
			return sample->gpu_depth;
		}

		/// @brief Reports whether one retained point shadow-depth sample has GPU data.
		[[nodiscard]] std::optional<bool> scenePointShadowDepthHasGpu(std::size_t index) const {
			const auto sample = system().forward().scenePointShadowDepthSample(index);
			if (!sample || !sample->has_gpu) { return std::nullopt; }
			return sample->has_gpu;
		}

		/// @brief Returns absolute CPU/GPU depth error for one retained point shadow-depth sample.
		[[nodiscard]] std::optional<float> scenePointShadowDepthError(std::size_t index) const {
			return system().forward().scenePointShadowDepthError(index);
		}

		/// @brief Copies the last rendered swapchain image and writes it as a PNG.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			if (!system().initialized_) { return std::unexpected(Error::not_initialized); }
			if (output_path.empty()) { return std::unexpected(Error::invalid_argument); }
			return std::visit([&](auto &renderer) { return renderer.captureFrameToPng(output_path); }, system().renderer_);
		}

	private:
		/// @brief Returns the owning render system for dependent member access.
		[[nodiscard]] auto system() -> System & { return static_cast<System &>(*this); }
		/// @brief Returns the owning render system for const dependent member access.
		[[nodiscard]] auto system() const -> const System & { return static_cast<const System &>(*this); }
	};

} // namespace vve::simple
