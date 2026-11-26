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
/// - RadianceCascadesView: what the primary rays show; everything except shaded is a debug view.
/// - RadianceCascadesVoxels: CPU voxelization of the traced scene (average albedo, emission and face normal per voxel).
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
/// Implemented so far: settings, layout, device bring-up, CPU voxelization uploaded as RGBA32F 2D array images
/// (layer = z), acceleration structures over the bound RenderScene, and a frame that traces one camera ray per pixel
/// (material colour or a voxel debug view, no lighting) into the HDR output image and blits it to the swapchain.
/// The probe volume is the voxel volume; one RGBA16F atlas per cascade is created with the scene, and every frame
/// traces each cascade's intervals into it (voxel emission at hits, transparency 1 for misses; the last cascade's
/// rays reach the sky, the background colour), then merges the cascades from the last to the first in place.
/// Without traceable geometry the frame is the background colour. The inject stage is empty.

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

	/// @brief What the primary rays show.
	enum class RadianceCascadesView {
		shaded,       ///< The rendered image (currently the material base colour).
		voxel_albedo, ///< Albedo of the voxel at each hit point; magenta marks a hit in an empty voxel.
		cascade_atlas, ///< One layer of one cascade atlas (setDebugAtlas) as the frame left it, merged unless setMerging(false):
		               ///< radiance where the transparency is below one half, else the background.
	};

	/// @brief Voxelized scene: a regular grid whose voxel (0, 0, 0) has its minimum corner at origin.
	/// Voxel (x, y, z) is element x + size[0] * (y + size[1] * z) of every array. A voxel is occupied when a traced
	/// triangle touches it, including along its faces; occupied voxels store averages over those triangles.
	struct RadianceCascadesVoxels {
		Vec3 origin{zeroVec3()};										///< World position of the minimum corner of voxel (0, 0, 0).
		Scalar voxel_size{};											///< Edge length of one voxel.
		std::array<std::uint32_t, 3> size{};							///< Voxels per axis; all zero for a scene without traced triangles.
		std::vector<Vec4> albedo{};										///< Average base colour; alpha is 1 for occupied voxels, else 0.
		std::vector<Vec4> emissive{};									///< Average emitted radiance factor; alpha unused.
		std::vector<Vec4> normal{};										///< Normalized average face normal; zero when empty or when normals cancel.
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
		[[nodiscard]] bool atlasesFit(Vec3 volumeExtent, std::uint32_t maxDimension, std::uint32_t maxLayers) const;
		[[nodiscard]] static Vec3 octahedralDirection(std::uint32_t x, std::uint32_t y, std::uint32_t side);

		// Voxelization (RadianceCascadesRenderer.cpp); pure CPU, so tests need no GPU.
		static constexpr std::uint32_t maxVoxelsPerAxis{2048U};		///< Guaranteed minimum of maxImageArrayLayers; also below maxImageDimension2D.
		static constexpr std::uint64_t maxVoxels{1ULL << 22U};		///< 4 Mi voxels, 64 MiB per RGBA32F volume image.
		[[nodiscard]] static auto voxelize(const RenderScene &scene, Scalar voxelSize) -> std::expected<RadianceCascadesVoxels, Error>;
		[[nodiscard]] static bool traced(const RenderInstance &instance) noexcept;

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
		auto setView(RadianceCascadesView view) -> void;
		[[nodiscard]] RadianceCascadesView view() const noexcept;
		auto setDebugAtlas(std::uint32_t cascade, std::uint32_t layer) -> void;
		auto setMerging(bool merging) -> void;
		[[nodiscard]] bool merging() const noexcept;
		[[nodiscard]] std::array<std::uint32_t, 3> voxelGrid() const noexcept;
		[[nodiscard]] Vec3 probeVolumeExtent() const noexcept;
		[[nodiscard]] std::array<std::uint32_t, 3> cascadeAtlasSize(std::uint32_t cascade) const noexcept;

		// Per-frame work (RadianceCascadesRenderer.cpp).
		[[nodiscard]] auto renderFrame() -> std::expected<void, Error>;
		[[nodiscard]] auto execute() -> std::expected<void, Error>;
		auto waitIdle() -> void;
		[[nodiscard]] std::uint64_t renderedFrameCount() const noexcept;
		auto setBackgroundColor(LinearColor color) -> void;
		[[nodiscard]] LinearColor backgroundColor() const noexcept;

		// Diagnostics.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error>;
		[[nodiscard]] auto readCascadeAtlas(std::uint32_t cascade) -> std::expected<std::vector<Vec4>, Error>;
		[[nodiscard]] VkExtent2D extent() const noexcept;
		[[nodiscard]] bool validationActive() const;
		[[nodiscard]] std::uint64_t validationErrorCount() const;

	private:
		static constexpr std::uint32_t framesInFlight{2U};			///< Independently fenced frame slots.

		[[nodiscard]] static Vec3 volumeExtent(std::array<std::uint32_t, 3> grid, Scalar voxelSize) noexcept;
		[[nodiscard]] VkExtent2D windowPixelExtent() const;
		[[nodiscard]] VkResult createSwapchainResources(VkExtent2D extent);
		[[nodiscard]] VkResult syncSwapchain(VkExtent2D extent);
		[[nodiscard]] VkResult createPipeline();
		[[nodiscard]] VkResult buildScene(const RadianceCascadesVoxels &voxels);
		[[nodiscard]] VkResult uploadVoxels(const RadianceCascadesVoxels &voxels);
		[[nodiscard]] VkResult createCascades();
		[[nodiscard]] auto buildStructure(VkAccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR &geometry,
			std::uint32_t primitives) -> std::expected<VkAccelerationStructureKHR, VkResult>;
		void destroyScene();
		void writeDescriptors();
		[[nodiscard]] auto drawFrame(VulkanReadback *readback) -> std::expected<bool, Error>;
		void recordFrame(VkCommandBuffer command, std::uint32_t imageIndex);
		void pushFrameConstants(VkCommandBuffer command, Vec4 stage, bool sky);
		void bindSets(VkCommandBuffer command, std::uint32_t nearCascade, std::uint32_t farCascade);
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
		VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};			///< The one set 0 every stage binds.
		VkDescriptorSetLayout cascade_set_layout_{VK_NULL_HANDLE};	///< Set 1: the atlas of one cascade.
		VulkanDescriptorPool cascade_pool_{};							///< Owns cascade_sets_.
		std::vector<VkDescriptorSet> cascade_sets_{};				///< One set 1 per cascade; written while the atlases exist.
		VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};			///< set_layout_, cascade_set_layout_ twice (near and far cascade) and the frame push constants.
		VkPipeline pipeline_{VK_NULL_HANDLE};							///< Ray tracing pipeline of the primary rays, the cascade rays and the merge.
		VulkanBuffer shader_table_{};									///< Shader binding table: ray generation, miss and hit group handles.
		std::array<VkStridedDeviceAddressRegionKHR, 5> shader_regions_{}; ///< Primary, cascade and merge ray generation, then miss and hit regions; miss and hit hold primary then cascade.
		const RenderScene *scene_{nullptr};							///< Borrowed CPU scene; read only by rebuildScene and init.
		std::vector<VulkanBuffer> scene_buffers_{};					///< Vertex, index, structure storage and instance buffers of the scene.
		std::vector<VkAccelerationStructureKHR> bottom_levels_{};	///< One structure per RenderScene mesh; null for meshes without triangles.
		VkAccelerationStructureKHR top_level_{VK_NULL_HANDLE};	///< Structure over the traced instances; null when there are none.
		VulkanBuffer materials_{};										///< Material rows in RenderScene order, plus a white fallback.
		std::array<VulkanImage, 3> voxels_{};							///< Albedo, emissive and normal volumes as RGBA32F 2D arrays, layer = z.
		Vec3 voxel_origin_{zeroVec3()};									///< RadianceCascadesVoxels::origin of the uploaded volume.
		Scalar voxel_size_{};											///< RadianceCascadesVoxels::voxel_size of the uploaded volume.
		std::array<std::uint32_t, 3> voxel_grid_{};						///< RadianceCascadesVoxels::size of the uploaded volume.
		RadianceCascadesView view_{RadianceCascadesView::shaded};		///< What the primary rays show.
		std::array<std::uint32_t, 2> debug_atlas_{};					///< Cascade and layer of the atlas view; clamped when recorded.
		bool merging_{true};											///< Whether frames merge the cascades; off leaves the raw intervals in the atlases.
		std::size_t traced_instances_{};								///< Instances inside top_level_.
		Camera camera_{.position = {Vec3{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}},
			.forward = {Vec3{zero(), static_cast<Scalar>(-5.0), static_cast<Scalar>(-9.0)}}}; ///< View of the primary rays; the default matches the forward renderer.
		std::vector<VulkanImage> cascades_{};							///< One RGBA16F 2D array atlas per cascade: a layer per probe z slice; empty without a scene.
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

	/// @brief Reports whether every cascade atlas of a volume fits the device's 2D image size and array layer limits.
	/// Sizes are computed in double precision, so scale settings that would wrap the 32-bit layout values are refused.
	inline bool RadianceCascadesRenderer::atlasesFit(Vec3 volumeExtent, std::uint32_t maxDimension, std::uint32_t maxLayers) const {
		double spacing{settings_.base_probe_spacing};
		double side{static_cast<double>(settings_.base_direction_side)};
		for (std::uint32_t cascade{}; cascade < settings_.cascade_count; ++cascade) {
			const auto probes = [spacing](Scalar extent) { return std::ceil(static_cast<double>(extent) / spacing) + 1.0; };
			if (probes(volumeExtent.x) * side > maxDimension || probes(volumeExtent.y) * side > maxDimension || probes(volumeExtent.z) > maxLayers) {
				return false;
			}
			spacing *= settings_.spatial_scale;
			side *= settings_.angular_scale;
		}
		return true;
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

	/// @brief Selects what the primary rays show; takes effect with the next frame.
	inline auto RadianceCascadesRenderer::setView(RadianceCascadesView view) -> void { view_ = view; }

	/// @brief Returns what the primary rays show.
	inline RadianceCascadesView RadianceCascadesRenderer::view() const noexcept { return view_; }

	/// @brief Selects the cascade and layer the atlas view shows; values past the last are clamped when a frame is recorded.
	inline auto RadianceCascadesRenderer::setDebugAtlas(std::uint32_t cascade, std::uint32_t layer) -> void { debug_atlas_ = {cascade, layer}; }

	/// @brief Turns the merge on (the default) or off; off is a debug mode that leaves each atlas with its raw intervals.
	inline auto RadianceCascadesRenderer::setMerging(bool merging) -> void { merging_ = merging; }

	/// @brief Reports whether frames merge the cascades.
	inline bool RadianceCascadesRenderer::merging() const noexcept { return merging_; }

	/// @brief Direction of texel (x, y) of a side x side octahedral map, taken at the texel centre; the map centre is +y.
	/// Mirrors octahedralDirection in simple_radiance_cascades.slang.
	inline Vec3 RadianceCascadesRenderer::octahedralDirection(std::uint32_t x, std::uint32_t y, std::uint32_t side) {
		const auto coordinate = [side](std::uint32_t texel) {
			return (static_cast<Scalar>(texel) + static_cast<Scalar>(0.5)) / static_cast<Scalar>(side) * static_cast<Scalar>(2) - one();
		};
		const Scalar ex{coordinate(x)}, ez{coordinate(y)};
		Vec3 v{ex, one() - std::abs(ex) - std::abs(ez), ez};
		if (v.y < zero()) {
			const Scalar folded_x{(one() - std::abs(v.z)) * (v.x >= zero() ? one() : -one())};
			const Scalar folded_z{(one() - std::abs(v.x)) * (v.z >= zero() ? one() : -one())};
			v.x = folded_x;
			v.z = folded_z;
		}
		return normalize(v);
	}

	/// @brief Voxels per axis of the uploaded volume; zero without a scene or before init.
	inline std::array<std::uint32_t, 3> RadianceCascadesRenderer::voxelGrid() const noexcept { return voxel_grid_; }

	/// @brief Size in metres of the probe volume, which is the voxel volume; its minimum corner is the voxel origin.
	inline Vec3 RadianceCascadesRenderer::probeVolumeExtent() const noexcept { return volumeExtent(voxel_grid_, voxel_size_); }

	/// @brief Size in metres of a voxel grid; the one expression the probe layout and the atlas limit check share.
	inline Vec3 RadianceCascadesRenderer::volumeExtent(std::array<std::uint32_t, 3> grid, Scalar voxelSize) noexcept {
		return scale(Vec3{static_cast<Scalar>(grid[0]), static_cast<Scalar>(grid[1]), static_cast<Scalar>(grid[2])}, voxelSize);
	}

	/// @brief Width, height and layer count of a cascade's atlas; zero without a scene, before init or past the last cascade.
	inline std::array<std::uint32_t, 3> RadianceCascadesRenderer::cascadeAtlasSize(std::uint32_t cascade) const noexcept {
		if (cascade >= cascades_.size()) { return {}; }
		const auto &atlas = cascades_[cascade];
		return {atlas.extent.width, atlas.extent.height, atlas.layerCount};
	}

	/// @brief Instances the rays and the voxelizer see: visible ordinary meshes, not the sky, sprites or terrain.
	inline bool RadianceCascadesRenderer::traced(const RenderInstance &instance) noexcept {
		return instance.visible && !instance.sky && !instance.sprite && !instance.terrain;
	}

	/// @brief Size of the swapchain images in pixels; zero before init.
	inline VkExtent2D RadianceCascadesRenderer::extent() const noexcept { return swapchain_.extent; }

	/// @brief Reports whether the Debug validation messenger of the renderer's own instance is active.
	inline bool RadianceCascadesRenderer::validationActive() const { return instance_.validationActive(); }

	/// @brief Validation ERROR count since init; kept after shutdown.
	inline std::uint64_t RadianceCascadesRenderer::validationErrorCount() const { return instance_.validationErrorCount(); }

} // namespace vve::simple
