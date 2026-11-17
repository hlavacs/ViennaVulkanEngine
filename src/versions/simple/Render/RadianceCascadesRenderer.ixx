module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_raii.hpp>
export module VVEngine.Simple.RadianceCascadesRenderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;
import VVEngine.Simple.RenderResources;

/// @file
/// @brief Standalone world-space radiance cascades renderer (Sannikov, "Radiance Cascades").
///
/// Functional objects:
/// - RadianceCascadesSettings: every size and count that research runs tune.
/// - RadianceCascadesRenderer: independent of the forward and deferred renderers. It owns its Vulkan device with the
///   ray tracing extensions, one window's swapchain, the cascade atlases, and records the frame stages. The cascade
///   layout functions are pure and need no GPU. Larger member functions live in RadianceCascadesRenderer.cpp.
///
/// Design: probes sit on a regular 3D grid inside a box fitted to the scene. Each probe stores an octahedral map of
/// radiance intervals (rgb) and transparency (a). From cascade i to i + 1 the probe spacing, the direction map side
/// and the interval length are multiplied by the scale settings; with the defaults (2, 2, 2) this is the paper's
/// 3D law, where every cascade needs half the memory of the one before. The scene is voxelized on the CPU into a
/// radiance volume; rays are traced with the Vulkan ray tracing pipeline against the scene triangles.
///
/// Implemented so far: settings, layout, device bring-up, acceleration structures over the bound RenderScene, and a
/// frame that traces one camera ray per pixel (material colour, no lighting) into the HDR output image and blits it
/// to the swapchain. Without traceable geometry the frame is the background colour. The cascade stages are empty.

export namespace vve::simple {

	/// @brief Tunable sizes of the cascade hierarchy and the voxel volume; lengths are world metres.
	struct RadianceCascadesSettings {
		std::uint32_t cascade_count{4U};										///< Number of cascades; the last one also receives the sky.
		Scalar base_probe_spacing{static_cast<Scalar>(0.5)};				///< Distance between neighbouring cascade 0 probes.
		std::uint32_t base_direction_side{4U};								///< Octahedral map side of cascade 0 (4 x 4 = 16 directions).
		Scalar base_interval_length{static_cast<Scalar>(0.5)};			///< Cascade 0 traces [0, this); later cascades continue from there.
		std::uint32_t spatial_scale{2U};										///< Probe spacing multiplier per cascade.
		std::uint32_t angular_scale{2U};										///< Direction map side multiplier per cascade (2 means 4x the directions).
		Scalar interval_scale{static_cast<Scalar>(2)};						///< Interval length multiplier per cascade.
		Scalar voxel_size{static_cast<Scalar>(0.25)};						///< Edge length of one voxel of the radiance volume.
	};

	/// @brief Standalone radiance cascades renderer for one window; not for production use.
	class RadianceCascadesRenderer {
	public:
		RadianceCascadesRenderer();
		~RadianceCascadesRenderer();

		RadianceCascadesRenderer(const RadianceCascadesRenderer &) = delete;
		RadianceCascadesRenderer &operator=(const RadianceCascadesRenderer &) = delete;
		// The Vulkan owners below are not movable.
		RadianceCascadesRenderer(RadianceCascadesRenderer &&) = delete;
		RadianceCascadesRenderer &operator=(RadianceCascadesRenderer &&) = delete;

		// Settings; they size the GPU resources, so they can only change while the renderer is not initialized.
		[[nodiscard]] auto setSettings(RadianceCascadesSettings settings) -> std::expected<void, Error>;
		[[nodiscard]] const RadianceCascadesSettings &settings() const noexcept;

		// Cascade layout, shared by resource creation and the shaders; pure, so tests need no GPU.
		[[nodiscard]] Scalar probeSpacing(std::uint32_t cascade) const;
		[[nodiscard]] std::uint32_t directionSide(std::uint32_t cascade) const;
		[[nodiscard]] std::array<std::uint32_t, 3> probeCount(std::uint32_t cascade, Vec3 volumeExtent) const;
		[[nodiscard]] VkExtent2D atlasExtent(std::uint32_t cascade, Vec3 volumeExtent) const;
		[[nodiscard]] Scalar intervalStart(std::uint32_t cascade) const;
		[[nodiscard]] Scalar intervalEnd(std::uint32_t cascade) const;

		// Lifecycle (RadianceCascadesRenderer.cpp). The window must stay alive until shutdown.
		[[nodiscard]] auto init(SDL_Window *window) -> std::expected<void, Error>;
		auto cleanup() -> void;
		auto shutdown() -> void;
		[[nodiscard]] bool initialized() const noexcept;

		// Scene (RadianceCascadesRenderer.cpp). The scene is treated as static: call rebuildScene after changing it.
		auto bindRenderScene(const RenderScene &scene) -> void;
		[[nodiscard]] auto rebuildScene() -> std::expected<void, Error>;
		[[nodiscard]] std::size_t tracedInstanceCount() const noexcept;
		auto setCamera(Camera camera) -> void;
		[[nodiscard]] const Camera &camera() const noexcept;

		// Per-frame work (RadianceCascadesRenderer.cpp).
		[[nodiscard]] auto renderFrame() -> std::expected<void, Error>;
		[[nodiscard]] auto execute() -> std::expected<void, Error>;
		auto waitIdle() -> void;
		[[nodiscard]] std::uint64_t renderedFrameCount() const noexcept;
		auto setBackgroundColor(LinearColor color) -> void;
		[[nodiscard]] LinearColor backgroundColor() const noexcept;

		// Diagnostics.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error>;
		[[nodiscard]] VkExtent2D extent() const noexcept;
		[[nodiscard]] bool validationActive() const;
		[[nodiscard]] std::uint64_t validationErrorCount() const;

	private:
		static constexpr std::uint32_t framesInFlight{2U};			///< Independently fenced frame slots.

		[[nodiscard]] VkExtent2D windowPixelExtent() const;
		[[nodiscard]] VkResult createSwapchainResources(VkExtent2D extent);
		[[nodiscard]] VkResult syncSwapchain(VkExtent2D extent);
		[[nodiscard]] VkResult createPipeline();
		[[nodiscard]] VkResult buildScene();
		[[nodiscard]] auto buildStructure(VkAccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR &geometry,
			std::uint32_t primitives) -> std::expected<VkAccelerationStructureKHR, VkResult>;
		void destroyScene();
		void writeDescriptors();
		[[nodiscard]] auto drawFrame(VulkanReadback *readback) -> std::expected<bool, Error>;
		void recordFrame(VkCommandBuffer command, std::uint32_t imageIndex);
		void record(VkCommandBuffer command);
		void recordInject(VkCommandBuffer command);
		void recordTrace(VkCommandBuffer command, std::uint32_t cascade);
		void recordMerge(VkCommandBuffer command, std::uint32_t cascade);
		void recordResolve(VkCommandBuffer command);

		RadianceCascadesSettings settings_{};							///< Sizes used by the layout and the GPU resources.
		LinearColor background_color_{.value = Vec3{static_cast<Scalar>(0.45), static_cast<Scalar>(0.70), one()}}; ///< Colour of pixels no geometry covers.
		// Declaration order is creation order; cleanup releases in reverse.
		SDL_Window *window_{nullptr};									///< Borrowed window that provides the surface.
		VulkanInstance instance_{};										///< Own instance; nothing is shared with the forward renderer.
		VulkanSurface surface_{};											///< Surface of window_.
		VulkanPhysicalDevice physical_device_{};						///< Device that advertises the ray tracing extensions.
		VulkanDevice device_{};											///< Logical device with acceleration structures and ray tracing pipelines enabled.
		VulkanAllocator allocator_{};									///< VMA allocator with buffer device addresses enabled.
		VulkanCommandPool command_pool_{};								///< Pool of the frame command buffers and one-time submits.
		VulkanCommandBuffers command_buffers_{};						///< One command buffer per frame slot.
		VulkanSwapchain swapchain_{};									///< Presentation images of window_.
		VulkanImage output_{};											///< Linear HDR image every stage renders into; blitted to the swapchain.
		VulkanFrameSync frame_sync_{};									///< Fences and semaphores of the frame slots.
		VkDescriptorSetLayout set_layout_{VK_NULL_HANDLE};			///< Scene structure, output image and materials.
		VulkanDescriptorPool descriptor_pool_{};						///< Owns descriptor_set_.
		VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};			///< The one set every stage binds.
		VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};			///< set_layout_ plus the frame push constants.
		VkPipeline pipeline_{VK_NULL_HANDLE};							///< Ray tracing pipeline of the primary rays.
		VulkanBuffer shader_table_{};									///< Shader binding table: ray generation, miss and hit group handles.
		std::array<VkStridedDeviceAddressRegionKHR, 3> shader_regions_{}; ///< Ray generation, miss and hit regions inside shader_table_.
		const RenderScene *scene_{nullptr};							///< Borrowed CPU scene; read only by rebuildScene and init.
		std::vector<VulkanBuffer> scene_buffers_{};					///< Vertex, index, structure storage and instance buffers of the scene.
		std::vector<VkAccelerationStructureKHR> bottom_levels_{};	///< One structure per RenderScene mesh; null for meshes without triangles.
		VkAccelerationStructureKHR top_level_{VK_NULL_HANDLE};	///< Structure over the traced instances; null when there are none.
		VulkanBuffer materials_{};										///< Material rows in RenderScene order, plus a white fallback.
		std::size_t traced_instances_{};								///< Instances inside top_level_.
		Camera camera_{.position = {Vec3{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}},
			.forward = {Vec3{zero(), static_cast<Scalar>(-5.0), static_cast<Scalar>(-9.0)}}}; ///< View of the primary rays; the default matches the forward renderer.
		std::vector<VulkanImage> cascades_{};							///< One 2D array atlas per cascade: a layer per probe z slice.
		std::uint32_t current_frame_{0U};								///< Frame slot used by the next frame.
		bool initialized_{false};										///< Tracks whether the renderer has been initialized.
		std::uint64_t rendered_frames_{0};							///< Lifetime count of presented frames.
	};

	/// @brief Default constructor; no GPU work happens before init.
	inline RadianceCascadesRenderer::RadianceCascadesRenderer() = default;

	/// @brief Destructor releasing any resources.
	inline RadianceCascadesRenderer::~RadianceCascadesRenderer() {
		cleanup();													// Ensure resources are reset on destruction.
	}

	/// @brief Replaces the settings after checking that every count is positive and every length is finite and positive.
	/// @return Error::already_initialized while GPU resources exist, Error::invalid_argument for malformed values.
	inline auto RadianceCascadesRenderer::setSettings(RadianceCascadesSettings settings) -> std::expected<void, Error> {
		if (initialized_) { return std::unexpected(Error::already_initialized); }
		const auto length = [](Scalar value) { return std::isfinite(value) && value > zero(); };
		if (settings.cascade_count == 0U || settings.base_direction_side == 0U || settings.spatial_scale == 0U ||
			settings.angular_scale == 0U || !length(settings.base_probe_spacing) || !length(settings.base_interval_length) ||
			!length(settings.interval_scale) || !length(settings.voxel_size)) {
			return std::unexpected(Error::invalid_argument);
		}
		settings_ = settings;
		return {};
	}

	/// @brief Returns the settings in use.
	inline const RadianceCascadesSettings &RadianceCascadesRenderer::settings() const noexcept {
		return settings_;
	}

	/// @brief Metres between neighbouring probes of a cascade.
	inline Scalar RadianceCascadesRenderer::probeSpacing(std::uint32_t cascade) const {
		Scalar spacing{settings_.base_probe_spacing};
		for (std::uint32_t step{}; step < cascade; ++step) { spacing *= static_cast<Scalar>(settings_.spatial_scale); }
		return spacing;
	}

	/// @brief Side of a probe's octahedral direction map in a cascade.
	inline std::uint32_t RadianceCascadesRenderer::directionSide(std::uint32_t cascade) const {
		std::uint32_t side{settings_.base_direction_side};
		for (std::uint32_t step{}; step < cascade; ++step) { side *= settings_.angular_scale; }
		return side;
	}

	/// @brief Probes per axis: the first sits on the volume minimum, the last on or beyond the maximum.
	inline std::array<std::uint32_t, 3> RadianceCascadesRenderer::probeCount(std::uint32_t cascade, Vec3 volumeExtent) const {
		const Scalar spacing{probeSpacing(cascade)};
		const auto count = [spacing](Scalar extent) { return static_cast<std::uint32_t>(std::ceil(extent / spacing)) + 1U; };
		return {count(volumeExtent.x), count(volumeExtent.y), count(volumeExtent.z)};
	}

	/// @brief Size of one atlas layer: a directionSide x directionSide tile per probe in x and y; z selects the layer.
	inline VkExtent2D RadianceCascadesRenderer::atlasExtent(std::uint32_t cascade, Vec3 volumeExtent) const {
		const auto probes = probeCount(cascade, volumeExtent);
		const std::uint32_t side{directionSide(cascade)};
		return {.width = probes[0] * side, .height = probes[1] * side};
	}

	/// @brief Distance in metres at which the rays of a cascade start; cascade 0 starts at the probe.
	inline Scalar RadianceCascadesRenderer::intervalStart(std::uint32_t cascade) const {
		Scalar start{zero()};
		Scalar length{settings_.base_interval_length};
		for (std::uint32_t step{}; step < cascade; ++step) { start += length; length *= settings_.interval_scale; }
		return start;
	}

	/// @brief Distance in metres at which the rays of a cascade end; equals the start of the next cascade.
	inline Scalar RadianceCascadesRenderer::intervalEnd(std::uint32_t cascade) const {
		return intervalStart(cascade + 1U);
	}

	/// @brief Shuts down the renderer.
	inline auto RadianceCascadesRenderer::shutdown() -> void {
		cleanup();													// Forward shutdown to cleanup.
	}

	/// @brief Reports whether the renderer is currently initialized.
	/// @return True if initialized, false otherwise.
	inline bool RadianceCascadesRenderer::initialized() const noexcept {
		return initialized_;										// Return active initialization flag.
	}

	/// @brief Records the lighting stages: light the voxels, trace every cascade, merge from the last to the first, shade the pixels.
	inline void RadianceCascadesRenderer::record(VkCommandBuffer command) {
		recordInject(command);
		for (std::uint32_t cascade{}; cascade < settings_.cascade_count; ++cascade) { recordTrace(command, cascade); }
		for (std::uint32_t cascade{settings_.cascade_count - 1U}; cascade-- > 0U;) { recordMerge(command, cascade); }
		recordResolve(command);
	}

	/// @brief Writes voxel radiance: emissive plus albedo times the direct light that a shadow ray finds unoccluded.
	inline void RadianceCascadesRenderer::recordInject(VkCommandBuffer) {
		// TODO: ray generation shader, one shadow ray per voxel and light.
	}

	/// @brief Traces the ray interval of every probe direction of one cascade into its atlas.
	inline void RadianceCascadesRenderer::recordTrace(VkCommandBuffer, std::uint32_t) {
		// TODO: ray generation shader over the atlas; closest hit reads voxel radiance, miss returns transparency 1.
	}

	/// @brief Merges cascade + 1 into cascade: L = L_near + transparency_near * L_far (paper equation 11).
	inline void RadianceCascadesRenderer::recordMerge(VkCommandBuffer, std::uint32_t) {
		// TODO: compute shader; trilinear probe interpolation, average of the child directions.
	}

	/// @brief Convenience execution alias for renderFrame.
	/// @return Result of renderFrame().
	inline auto RadianceCascadesRenderer::execute() -> std::expected<void, Error> {
		return renderFrame();									// Forward execution request to renderFrame.
	}

	/// @brief Returns the number of frames presented since construction.
	/// @return Lifetime presented frame count.
	inline std::uint64_t RadianceCascadesRenderer::renderedFrameCount() const noexcept {
		return rendered_frames_;								// Return frame counter.
	}

	/// @brief Sets the colour of pixels that no geometry covers.
	inline auto RadianceCascadesRenderer::setBackgroundColor(LinearColor color) -> void { background_color_ = color; }

	/// @brief Returns the colour of pixels that no geometry covers; the default is the forward renderer's light blue.
	inline LinearColor RadianceCascadesRenderer::backgroundColor() const noexcept { return background_color_; }

	/// @brief Borrows the CPU scene; it must outlive the renderer or be replaced before the next rebuildScene or init.
	inline auto RadianceCascadesRenderer::bindRenderScene(const RenderScene &scene) -> void { scene_ = std::addressof(scene); }

	/// @brief Number of scene instances the rays can hit; sky, sprites, terrain and hidden instances are left out.
	inline std::size_t RadianceCascadesRenderer::tracedInstanceCount() const noexcept { return traced_instances_; }

	/// @brief Sets the view of the primary rays.
	inline auto RadianceCascadesRenderer::setCamera(Camera camera) -> void { camera_ = std::move(camera); }

	/// @brief Returns the view of the primary rays.
	inline const Camera &RadianceCascadesRenderer::camera() const noexcept { return camera_; }

	/// @brief Size of the swapchain images in pixels; zero before init.
	inline VkExtent2D RadianceCascadesRenderer::extent() const noexcept { return swapchain_.extent; }

	/// @brief Reports whether the Debug validation messenger of the renderer's own instance is active.
	inline bool RadianceCascadesRenderer::validationActive() const { return instance_.validationActive(); }

	/// @brief Validation ERROR count since init; kept after shutdown.
	inline std::uint64_t RadianceCascadesRenderer::validationErrorCount() const { return instance_.validationErrorCount(); }

} // namespace vve::simple
