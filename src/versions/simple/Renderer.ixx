module;
#include <new>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Types;
import VEEngine.Simple.RenderPassContract;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;
export import :RendererDebug;
export import :RendererShadowPrep;
export import :RendererResources;
export import :RendererDraw;

namespace vve::simple::detail {

	static constexpr std::string_view forward_renderer_pass{"forward.color_pass"};											///< Forward color pass node.
	static constexpr std::array forward_renderer_shadow_deps{RenderMilestone::frame_begin()};				///< Spot shadow depth dependency.
	static constexpr std::array forward_renderer_pass_deps{RenderMilestone::shadow_depth()};				///< Forward color pass dependency.
	static constexpr std::array forward_renderer_scene_done_deps{forward_renderer_pass};						///< Scene-color milestone input.
	static constexpr std::array forward_renderer_finished_deps{RenderMilestone::scene_color()};			///< Frame-finished input.
	static constexpr std::array forward_renderer_pass_contracts{															///< Minimal renderer pass contract list.
		RenderPassContract{.name = RenderMilestone::frame_begin(), .outputs = "frame inputs", .milestone = true},
		RenderPassContract{.name = RenderMilestone::shadow_depth(),
									.depends_on = forward_renderer_shadow_deps,
									.inputs = "registered render resources",
									.outputs = "spot shadow depth array"},
		RenderPassContract{.name = forward_renderer_pass,
									.depends_on = forward_renderer_pass_deps,
									.inputs = "registered render resources",
									.outputs = "forward scene color"},
		RenderPassContract{.name = RenderMilestone::scene_color(),
									.depends_on = forward_renderer_scene_done_deps,
									.outputs = "forward scene color is ready",
									.milestone = true},
		RenderPassContract{.name = RenderMilestone::frame_finished(),
									.depends_on = forward_renderer_finished_deps,
									.outputs = "frame can be presented",
									.milestone = true}};

} // namespace vve::simple::detail

/**
	* @file
	* @brief Vulkan renderer skeleton for the simple forward renderer.
	*
	* Functional objects:
	* - ShadowLightMeta records CPU-side light-to-shadow-layer bindings before shader data grows.
	* - ForwardRendererDebug supplies retained shadow diagnostics and readback helpers through the renderer debug partition.
	* - ForwardRendererShadowPrep supplies CPU shadow matrix and metadata preparation through the renderer shadow-prep partition.
	* - ForwardRendererResources supplies GPU resource creation, swapchain recreation, and teardown through the renderer resources partition.
	* - ForwardRendererDraw records per-frame shadow and color commands and submits the completed frame through the renderer draw partition.
	* - ForwardRenderer owns the current CPU scene, swapchain image stack, depth attachment, unbound shadow maps and shadow pipeline, optional object texture, default object texture, forward render pass, framebuffers, descriptor-set layout, pipeline layout, shader modules, graphics pipeline, command pool, frame command buffers, frame synchronization, per-frame uniform buffers, descriptor pool, per-frame descriptor sets, and uploaded per-object meshes needed before rendering.
	* - StubRenderer exposes the same common renderer lifetime and scene surface without owning Vulkan state.
	* - SelectedRenderer names the explicit renderer alternatives available to the simple render coordinator.
	*/
export namespace vve::simple {

	/// @brief Per-light CPU shadow binding metadata prepared by the forward renderer.
	struct ShadowLightMeta {
		std::uint32_t light_index{};								///< Source light index inside the scene light array.
		std::uint32_t light_type{};								///< Light type tag; 1 means spot and 2 means point light.
		std::uint32_t shadow_slot{};							///< Shadow resource slot selected for this light.
		std::uint32_t first_layer{};							///< First depth-array layer rendered for this light.
		std::uint32_t layer_count{};							///< Number of depth-array layers owned by this light.
		Mat4 view{identityMat4()};								///< Light view matrix used by the depth pass.
		Mat4 projection{identityMat4()};					///< Light projection matrix used by the depth pass.
		Scalar near_plane{};										///< Near clipping plane for light-space depth.
		Scalar far_plane{};										///< Far clipping plane for light-space depth.
		Scalar depth_bias{};										///< Depth compare bias mirrored by CPU diagnostics.
		std::uint32_t resolution{};							///< Square shadow-map side length in pixels.
	};

	/// @brief Minimal forward renderer owning Vulkan swapchain bring-up without draw state.
	struct ForwardRenderer : ForwardRendererDebug<ForwardRenderer>, ForwardRendererShadowPrep<ForwardRenderer>, ForwardRendererResources<ForwardRenderer>, ForwardRendererDraw<ForwardRenderer> {
		/// @brief Lightweight command-recording pass tag used by tests without introducing a render graph.
		enum class RecordedPass : std::uint8_t { directional_shadow, spot_shadow, point_shadow, forward_color };

		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanDepthImage depthImage{};         ///< Owned swapchain-sized depth attachment image and view.
		ShadowMap shadowMap{};                 ///< Owned unbound shadow-map image reserved for later shadow rendering.
		ShadowMap dirShadowArray{};            ///< Owned directional shadow-map texture array with one layer per active directional light.
		ShadowMap spotShadowMap{};             ///< Owned spot shadow-map image reserved for later shadow rendering.
		ShadowMap spotShadowArray{};           ///< Owned spot shadow-map texture array reserved for future multi-light shadows.
		ShadowMap pointShadowArray{};          ///< Owned point shadow-map texture array with six layers per shadowed point light.
		TextureImage objectTexture{};           ///< Owned optional base-color texture bound only when the loaded scene requests one.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture bound when the scene has no base-color texture.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::vector<VulkanMesh> meshes{};      ///< Owned GPU meshes uploaded from the current scene objects.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		std::vector<ShadowLightMeta> shadowLightMeta{}; ///< Per-light spot shadow metadata prepared for debug access.
		std::vector<RecordedPass> recordedPassOrder{}; ///< Last frame's command-recording pass order diagnostic.
		Vec3 cameraEye{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}; ///< World-space camera position used for the frame view matrix.
		Vec3 cameraTarget{zero(), one(), zero()}; ///< World-space point looked at by the frame view matrix.
		std::array<Mat4, kMaxShadowedSpotLights> spotLightViewProjs{}; ///< CPU spot-light matrices prepared for later multi-shadow rendering.
		std::size_t spotLightViewProjCount{}; ///< Number of active spot-light matrices copied from the current scene.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.

		~ForwardRenderer() { cleanup(); }

		/// @brief Returns the stable renderer-selection id for this concrete forward renderer.
		[[nodiscard]] RendererId id() const { return RendererId{.value = "forward"}; }

		/// @brief Declares this renderer's render passes and dependencies for graph merging.
		[[nodiscard]] std::span<const RenderPassContract> passes() const { return detail::forward_renderer_pass_contracts; }

		/// @brief Returns command-recorded pass tags from the most recent frame.
		[[nodiscard]] std::span<const RecordedPass> lastRecordedPassOrder() const { return recordedPassOrder; }

		/// @brief Releases renderer-owned resources through the existing cleanup path.
		void shutdown() { cleanup(); }

		/// @brief Stores the non-owning GUI system handle reserved for later GUI rendering integration.
		void setGuiSystem(void *gui) { guiSystem_ = gui; }

		/// @brief Stores the optional GUI command recorder used inside the forward color pass.
		void setGuiRecordSink(std::function<void(VkCommandBuffer)> sink) { guiRecord_ = std::move(sink); }

		/// @brief Enables expensive GPU-to-CPU shadow diagnostics for explicit verification runs.
		void setGpuDebugReadback(bool enabled) { gpuDebugReadback_ = enabled; }

		/// @brief Reports whether per-frame GPU debug readback is enabled.
		[[nodiscard]] bool gpuDebugReadbackEnabled() const { return gpuDebugReadback_; }

		/// @brief Reports whether the renderer currently owns a live Vulkan device.
		[[nodiscard]] bool initialized() const { return device.device != VK_NULL_HANDLE; }

		/// @brief Reports presented-frame diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t presentedFrameCount() const { return 0; }
		/// @brief Reports triangle-draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t triangleDrawCount() const { return 0; }
		/// @brief Reports triangle-vertex diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t triangleVertexCount() const { return 0; }
		/// @brief Reports scene-upload diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneUploadCount() const { return 0; }
		/// @brief Reports scene-mesh draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneMeshDrawCount() const { return 0; }
		/// @brief Reports scene-instance draw diagnostics for the forward renderer.
		[[nodiscard]] std::uint64_t sceneInstanceDrawCount() const { return 0; }
		/// @brief Reports scene-draw vertex diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t sceneDrawVertexCount() const { return 0; }
		/// @brief Reports scene-draw index diagnostics for the forward renderer.
		[[nodiscard]] std::uint32_t sceneDrawIndexCount() const { return 0; }
		/// @brief Reports the number of prepared spot shadow metadata rows.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return shadowLightMeta.size(); }
		/// @brief Returns one prepared spot shadow metadata row by retained index.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const {
			if (index >= shadowLightMeta.size()) { return {}; }
			return shadowLightMeta[index];
		}
		/// @brief Reports prepared GPU target diagnostics for the forward renderer.
		[[nodiscard]] std::size_t preparedGpuTargetCount() const { return 0; }
		std::array<float, 4> clearColor{0.0F, 0.0F, 0.0F, 1.0F}; ///< Last clear color used by the renderer.
		/// @brief Reports the last clear color used by the forward renderer.
		[[nodiscard]] std::array<float, 4> lastClearColor() const { return clearColor; }

		/// @brief Chooses FIFO in Debug builds and mailbox in optimized builds.
		[[nodiscard]] static constexpr VulkanSwapchain::PresentModePreference defaultPresentMode() {
#ifdef VVE_SIMPLE_RELEASE_PRESENT_MAILBOX
			return VulkanSwapchain::PresentModePreference::mailbox;
#else
			return VulkanSwapchain::PresentModePreference::fifo;
#endif
		}

		/**
			* @brief Replaces the current CPU scene before future renderer upload.
			*
			* @param nextScene Scene data prepared by the caller.
		*/
		void loadScene(Scene nextScene) {
			scene = std::move(nextScene);								// Scene upload invalidates per-frame shadow metadata.
			shadowLightMeta.clear();									// Metadata is rebuilt during the next frame assembly.
		}

		/// @brief Appends one backend object to the renderer-owned CPU scene mirror.
		void appendObject(Mesh backend_mesh, Mat4 model, std::optional<std::string> base_color_texture_source) {
			const bool use_texture = base_color_texture_source.has_value();
			if (use_texture) { scene.baseColorTexture = *std::move(base_color_texture_source); }
			scene.objects.push_back(Object{.mesh = std::move(backend_mesh),
													 .model = model,
													 .useBaseColorTexture = use_texture ? 1U : 0U});
		}

		/// @brief Clears the renderer-owned CPU scene through the existing scene replacement path.
		void clearScene() { loadScene(Scene{}); }

		/**
			* @brief Stores the camera eye and target used by future frame uniform updates.
			*
			* @param eye World-space camera position.
			* @param target World-space point the camera looks at.
			*/
		void setCamera(Vec3 eye, Vec3 target) {
			cameraEye = eye;
			cameraTarget = target;
		}

		/// @brief Common frame-render entry forwarding to the concrete Vulkan draw path.
		void renderFrame(VulkanReadback *readback = nullptr) { drawFrame(readback); }

	private:
		friend struct ForwardRendererResources<ForwardRenderer>;
		friend struct ForwardRendererDraw<ForwardRenderer>;
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.
		VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE}; ///< Owned Dear ImGui descriptor pool reserved for backend texture descriptors.
		void *guiSystem_{nullptr}; ///< Non-owning, type-erased GUI system pointer reserved for later GUI integration.
		std::function<void(VkCommandBuffer)> guiRecord_; ///< Optional GUI recorder invoked during the forward color pass.
		bool gpuDebugReadback_{false}; ///< False during normal rendering to avoid per-frame GPU stalls.
	};

	/// @brief No-op renderer used as an explicit placeholder for future renderer selection.
	struct StubRenderer : StubRendererDebug {
		Scene scene{}; ///< CPU scene mirror kept so scene submission has the same storage shape.

		/// @brief Returns the stable renderer-selection id for this placeholder renderer.
		[[nodiscard]] RendererId id() const { return RendererId{.value = "stub"}; }

		/// @brief Declares this renderer's render passes and dependencies for graph merging.
		[[nodiscard]] std::span<const RenderPassContract> passes() const { return detail::forward_renderer_pass_contracts; }

		/// @brief Accepts the normal renderer setup entry point without creating resources.
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow) { (void)sdlWindow; return VK_SUCCESS; }

		/// @brief Releases no resources because the stub owns no renderer backend.
		void shutdown() {}

		/// @brief Reports that the placeholder renderer never owns a live backend.
		[[nodiscard]] bool initialized() const { return false; }

		/// @brief Reports presented-frame diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t presentedFrameCount() const { return 0; }
		/// @brief Reports triangle-draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t triangleDrawCount() const { return 0; }
		/// @brief Reports triangle-vertex diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t triangleVertexCount() const { return 0; }
		/// @brief Reports scene-upload diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneUploadCount() const { return 0; }
		/// @brief Reports scene-mesh draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneMeshDrawCount() const { return 0; }
		/// @brief Reports scene-instance draw diagnostics for the stub renderer.
		[[nodiscard]] std::uint64_t sceneInstanceDrawCount() const { return 0; }
		/// @brief Reports scene-draw vertex diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t sceneDrawVertexCount() const { return 0; }
		/// @brief Reports scene-draw index diagnostics for the stub renderer.
		[[nodiscard]] std::uint32_t sceneDrawIndexCount() const { return 0; }
		/// @brief Reports the number of prepared shadow metadata rows for the stub renderer.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return 0; }
		/// @brief Returns a prepared shadow metadata row when the stub renderer has one.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const { (void)index; return {}; }
		/// @brief Reports prepared GPU target diagnostics for the stub renderer.
		[[nodiscard]] std::size_t preparedGpuTargetCount() const { return 0; }
		std::array<float, 4> clearColor{0.0F, 0.0F, 0.0F, 1.0F}; ///< Last clear color used by the renderer.
		/// @brief Reports the last clear color used by the stub renderer.
		[[nodiscard]] std::array<float, 4> lastClearColor() const { return clearColor; }

		/// @brief Replaces the stored CPU scene for parity with concrete renderer submission.
		void loadScene(Scene nextScene) { scene = std::move(nextScene); }

		/// @brief Appends one backend object to the stored CPU scene mirror.
		void appendObject(Mesh backend_mesh, Mat4 model, std::optional<std::string> base_color_texture_source) {
			const bool use_texture = base_color_texture_source.has_value();
			if (use_texture) { scene.baseColorTexture = *std::move(base_color_texture_source); }
			scene.objects.push_back(Object{.mesh = std::move(backend_mesh),
													 .model = model,
													 .useBaseColorTexture = use_texture ? 1U : 0U});
		}

		/// @brief Clears the stored CPU scene through the common scene replacement path.
		void clearScene() { loadScene(Scene{}); }

		/// @brief Accepts a frame-render request without producing GPU work.
		void renderFrame(VulkanReadback *readback = nullptr) { (void)readback; }

	};

	/// @brief Explicit renderer alternatives selectable by the simple render coordinator.
	using SelectedRenderer = std::variant<ForwardRenderer, StubRenderer>;
	static_assert(std::is_default_constructible_v<SelectedRenderer>);
	static_assert(std::is_constructible_v<SelectedRenderer, std::in_place_type_t<ForwardRenderer>>);
	static_assert(std::is_constructible_v<SelectedRenderer, std::in_place_type_t<StubRenderer>>);

} // namespace vve::simple
