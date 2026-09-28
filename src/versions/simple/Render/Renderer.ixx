module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif
#include <VVPPL.h>
#include "../shaders/simple_shared.h"

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.RenderResources;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief ForwardRenderer owns the shared Vulkan resources and CPU scene; WindowTarget owns one window's frame resources.
	*
	* The class is declared here; its larger member functions live in module implementation units:
	* - RendererResources.cpp: init, uploadSceneTextures, syncSceneResources, recreateSwapchain, cleanup, ImGui wiring.
	* - RendererShadowPrep.cpp: prepareShadowFrame packs contributing lights and builds matrices only for shadow casters.
	* - RendererDraw.cpp: drawFrame and recordCommandBuffer record shadow passes plus the forward color pass and present.
	*   acquireAction classifies swapchain results for bounded waits and CPU-only tests.
	* - RendererDebug.cpp: shadow-depth samples, optional GPU readback, and PNG capture.
	*/
export namespace vve::simple {

	/// @brief Next frame-loop action after an attempt to acquire a swapchain image.
	enum class AcquireAction : std::uint8_t {
		render, ///< An image was acquired; consume its semaphore by rendering and presenting.
		skip, ///< No image or semaphore signal is available; retry on the next frame.
		recreate, ///< The swapchain is out of date and must be rebuilt.
		fail, ///< Report a Vulkan failure without using an image.
	};
	[[nodiscard]] auto acquireAction(VkResult result) -> AcquireAction;

	/// @brief Scene draw and bind counts for the most recently recorded window, excluding GUI and post-processing.
	struct FrameDrawStats {
		std::size_t forwardDraws{};      ///< Indexed draws in the colour pass.
		std::size_t shadowDraws{};       ///< Indexed draws across all shadow layers.
		std::size_t vertexBufferBinds{}; ///< Mesh vertex/index pair binds across the scene passes.
		std::size_t pipelineBinds{};     ///< Shadow and forward graphics pipeline binds.
	};

	/// @brief Per-light CPU shadow binding metadata prepared by the forward renderer.
	struct ShadowLightMeta {
		std::uint32_t light_index{};								///< Packed light index inside the uniform array for its type.
		std::uint32_t light_type{};								///< Light type tag: 1 spot, 2 point, 3 directional.
		std::uint32_t first_layer{};							///< Layer inside this light type's depth array; one row per face or cascade.
		Mat4 view{identityMat4()};								///< Light view matrix used by the depth pass.
		Mat4 projection{identityMat4()};					///< Light projection matrix used by the depth pass.
		Scalar near_plane{};										///< Near clipping plane for light-space depth.
		Scalar far_plane{};										///< Far clipping plane for light-space depth.
	};

	/// @brief Per-window presentation, frame resources and camera; the renderer owns their shared device.
	struct WindowTarget {
		static constexpr std::uint32_t framesInFlight{2U}; ///< Independently fenced slots in this window.
		using DynamicVertices = std::pair<std::array<VulkanBuffer, framesInFlight>, std::bitset<framesInFlight>>; ///< Vertex copies and pending uploads by slot.
		WindowHandle handle{};                 ///< Window identity used for capture and close handling.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		std::uint32_t presentQueueFamily{};    ///< Queue family that can present to this surface.
		VkQueue presentQueue{VK_NULL_HANDLE};  ///< Borrowed presentation queue from the shared device.
		bool guiWindow{false};                 ///< Only the first rendered window owns the GUI pass and UNORM views.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanSwapchain swapchain{};           ///< Owned presentation images, formats and requestedExtent.
		VulkanImageViews imageViews{};         ///< Owned scene and GUI image views for swapchain images.
		VulkanImage depthImage{};              ///< Owned swapchain-sized depth attachment image and view.
		VulkanImage hdrImage{};                ///< Owned swapchain-sized RGBA16F color target the scene is rendered into.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::array<std::uint64_t, framesInFlight> submitSerials{}; ///< Last graphics submission using each frame slot, zero before first use.
		std::array<VulkanBuffer, framesInFlight> materialBuffers{}; ///< Mapped materials written only after the matching fence completes.
		std::bitset<framesInFlight> materialsDirty{}; ///< Slots awaiting the current dense material table.
		std::array<std::bitset<kMaxSceneTextures>, framesInFlight> texturesDirty{}; ///< Texture elements awaiting descriptor updates in each slot.
		std::bitset<framesInFlight> shadowsDirty{}; ///< Slots awaiting replacement shadow views and samplers.
		std::map<RenderMeshHandle, DynamicVertices> dynamicVertices{}; ///< Per-slot vertex storage allocated only for edited meshes.
		std::optional<Camera> camera{};         ///< Explicit window camera; an empty value uses the renderer's current default.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame color readback.
		std::unique_ptr<vvppl::PostProcessing> postProcess{}; ///< Owned post-processing chain applied to the finished color image.
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.

		void cleanup();
	};

	/// @brief Forward renderer owning the CPU scene mirror, every Vulkan resource, and the per-frame draw loop.
	struct ForwardRenderer {
		/// @brief Lightweight command-recording pass tag used by tests without introducing a render graph.
		enum class RecordedPass : std::uint8_t {
			directional_shadow, ///< Directional cascade depth pass.
			spot_shadow,        ///< Spotlight depth pass.
			point_shadow,       ///< Point-light face depth pass.
			forward_color,      ///< Scene colour rendered into HDR.
			post_process,       ///< Configured vvppl chain applied to HDR.
			gui,                ///< Nonempty GUI draw data composited onto the swapchain.
		};

		static constexpr std::uint32_t framesInFlight{WindowTarget::framesInFlight}; ///< Number of independent frame command buffers to allocate.
		static constexpr std::size_t pointShadowFaceCount{VVE_POINT_SHADOW_FACES}; ///< One square face per cubemap direction.
		static constexpr Scalar shadowNearPlane{VVE_SHADOW_NEAR_PLANE}; ///< Shared near plane for all shadow views.
		static constexpr float occludedShadowFactor{VVE_SHADOW_OCCLUDED_FACTOR}; ///< Shader partial-shadow floor.
		static constexpr float directionalCompareBias{VVE_DIRECTIONAL_SHADOW_BIAS}; ///< Shader-side bias of the nearest directional cascade.

		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanAllocator allocator{};           ///< Owned VMA allocator for every buffer and image below.
		ShadowMap dirShadowArray{};            ///< Grow-only directional shadow array with four layers per packed light, minimum one.
		ShadowMap spotShadowArray{};           ///< Grow-only spot shadow array with one layer per packed light, minimum one.
		ShadowMap pointShadowArray{};          ///< Grow-only point shadow array with six layers per packed light, minimum one.
		std::array<TextureImage, kMaxSceneTextures> objectTextures{}; ///< Owned material textures keyed by RenderScene texture-table index.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture filling unused texture slots.
		VkSampler materialSampler{VK_NULL_HANDLE}; ///< Shared trilinear repeat sampler, destroyed only after device idle.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanShaderModule shadowShaderModule{}; ///< Owned depth-only shadow vertex shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanGraphicsPipeline shadowPipeline{};   ///< Owned depth-only pipeline shared by every shadow layer.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		std::map<RenderMeshHandle, VulkanMesh> meshes{}; ///< One owned GPU mesh per referenced RenderMesh handle.
		std::map<RenderMaterialHandle, std::uint32_t> materialSlots_{}; ///< Stable material handle to dense GPU buffer slot.
		VulkanReadback shadowDepthReadback{};  ///< Single shadow-map layer readback shared by all light types.
		std::list<WindowTarget> targets{};      ///< Targets in window order; stable addresses preserve non-movable RAII owners.
		Scene scene{}; ///< Renderer lights; geometry, materials, and textures are read from the bound RenderScene.
		std::vector<ShadowLightMeta> shadowLightMeta{}; ///< Per-light shadow matrices and per-type array layers prepared every frame.
		std::vector<RenderShadowDepthSample> shadowDepthSamples{}; ///< Per-light samples rebuilt only with GPU readback enabled; empty otherwise.
		std::vector<RecordedPass> recordedPassOrder{}; ///< Last frame's command-recording pass order diagnostic.

		~ForwardRenderer() { cleanup(); }

		// Vulkan resource lifetime (RendererResources.cpp).
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow, WindowHandle handle = {}); ///< Creates the shared device and first window target.
		[[nodiscard]] VkResult addTarget(SDL_Window *sdlWindow, WindowHandle handle); ///< Adds a window target using the shared device and pipelines.
		[[nodiscard]] VkResult uploadSceneTextures();								///< Replaces changed textures and marks descriptor elements pending in every target.
		void rebuildSceneMaterials();											///< Rebuilds the CPU upload table and marks every target's material slots dirty.
		[[nodiscard]] VkResult syncSceneResources();								///< Brings GPU meshes and textures in line with CPU scene changes.
		[[nodiscard]] VkResult recreateSwapchain(WindowTarget &target, VkExtent2D requestedExtent);	///< Rebuilds swapchain-sized resources after a resize.
		[[nodiscard]] VkExtent2D currentWindowPixelExtent(const WindowTarget &target) const;
		void cleanup();																	///< Releases Vulkan device resources in reverse creation order.
		/// @brief Waits before external GUI or window teardown through the renderer's counted idle boundary.
		[[nodiscard]] VkResult waitIdle() { return waitDeviceIdle(); }
		void createImguiDescriptorPool();
		[[nodiscard]] ImGui_ImplVulkan_InitInfo makeImguiInitInfo() const;	///< Builds dormant Dear ImGui Vulkan backend data from the renderer-owned objects.

		// CPU shadow preparation (RendererShadowPrep.cpp).
		void prepareShadowFrame(const Mat4 &cameraView, Scalar cameraVerticalFov, Scalar cameraAspect, Scalar cameraNear, Scalar cameraFar);
		/// @brief Returns the camera, packed lights and shadow matrices most recently prepared for a window.
		[[nodiscard]] const FrameUniforms &frameUniforms() const { return frameUniforms_; }

		// Frame recording and presentation (RendererDraw.cpp).
		[[nodiscard]] bool drawFrame(WindowTarget &target, VulkanReadback *readback = nullptr);		///< Reports whether one swapchain frame was presented; the optional readback captures it for deterministic debug output.

		// Diagnostics (RendererDebug.cpp).
		[[nodiscard]] bool validationActive() const;							///< Reports whether the instance's Debug validation messenger is active.
		[[nodiscard]] std::uint64_t validationErrorCount() const;				///< Returns the instance's validation ERROR count since creation.
		void setGpuDebugReadback(bool enabled) { gpuDebugReadback_ = enabled; }	///< Enables the per-frame GPU shadow-depth readback for verification runs.
		void recordShadowDepthSamples();	///< Projects the world origin through every active shadow matrix.
		void fillShadowDepthSamplesFromGpu();												///< Reads the rendered shadow-map texel of every recorded sample.
		[[nodiscard]] auto captureFrameToPng(WindowTarget &target, const std::filesystem::path &output_path) -> std::expected<void, Error>;

		/// @brief Returns command-recorded pass tags from the most recent frame.
		[[nodiscard]] std::span<const RecordedPass> lastRecordedPassOrder() const { return recordedPassOrder; }

		/// @brief Releases renderer-owned resources through the existing cleanup path.
		void shutdown() { cleanup(); }

		/// @brief Stores GUI preparation, which reports whether the owning window has draw data.
		void setGuiPrepareSink(std::function<bool()> sink) { guiPrepare_ = std::move(sink); }

		/// @brief Stores the optional GUI command recorder used inside the GUI pass.
		void setGuiRecordSink(std::function<void(VkCommandBuffer)> sink) { guiRecord_ = std::move(sink); }

		/// @brief Stores the post processing setup, which sets and configures effects.
		void setPostProcessSetup(std::function<void(vvppl::PostProcessing &)> setup){ postProcessSetup_ = std::move(setup); }

		/// @brief Reports whether the renderer currently owns a live Vulkan device.
		[[nodiscard]] bool initialized() const { return device.device != VK_NULL_HANDLE; }

		/// @brief Reports the number of resident GPU textures, excluding the fallback texture.
		[[nodiscard]] std::size_t gpuTextureCount() const { return uploadedTextureCount_; }
		[[nodiscard]] std::uint64_t uploadedTextureGeneration(std::size_t index) const;
		[[nodiscard]] std::optional<Error> textureUploadError() const;
		/// @brief Reports completed texture upload submissions, including the initial white fallback.
		[[nodiscard]] std::size_t textureUploadSubmitCount() const { return textureUploadSubmitCount_; }
		/// @brief Reports the number of unique resident GPU meshes.
		[[nodiscard]] std::size_t gpuMeshCount() const { return meshes.size(); }
		/// @brief Reports the number of entries in the dense GPU material table.
		[[nodiscard]] std::size_t gpuMaterialCount() const { return uploadedMaterialCount_; }
		/// @brief Reports the monotonic number of mesh create or vertex-refresh uploads.
		[[nodiscard]] std::size_t gpuMeshUploadCount() const { return meshUploadCount_; }
		/// @brief Reports the monotonic number of material-buffer uploads.
		[[nodiscard]] std::size_t gpuMaterialUploadCount() const { return materialUploadCount_; }
		/// @brief Reports the lifetime count of frames skipped by a fence timeout or an unavailable swapchain image.
		[[nodiscard]] std::size_t skippedFrameCount() const { return skippedFrameCount_; }
		/// @brief Reports the lifetime count of successful forward graphics pipeline creations.
		[[nodiscard]] std::size_t forwardPipelineCreateCount() const { return forwardPipelineCreateCount_; }
		/// @brief Reports shadow layers cleared and rendered for the most recently recorded window frame.
		[[nodiscard]] std::size_t lastShadowLayerPassCount() const { return lastShadowLayerPassCount_; }
		/// @brief Reports scene draws and binds for the most recently recorded window frame.
		[[nodiscard]] FrameDrawStats lastFrameDrawStats() const { return lastFrameDrawStats_; }
		/// @brief Reports explicit device-idle waits; ordinary scene updates and shadow growth need none.
		[[nodiscard]] std::size_t deviceWaitIdleCount() const { return deviceWaitIdleCount_; }
		/// @brief Reports resources retained until their last graphics submission is known complete.
		[[nodiscard]] std::size_t retiredResourceCount() const { return retiredResources_.size(); }
		/// @brief Reports the latest graphics submission proven complete by a fence or device-idle wait.
		[[nodiscard]] std::uint64_t completedSubmitSerial() const { return completedSerial_; }

		/// @brief Reports the number of prepared spot shadow metadata rows.
		[[nodiscard]] std::size_t sceneShadowLightMetaCount() const { return shadowLightMeta.size(); }
		/// @brief Returns one prepared spot shadow metadata row by retained index.
		[[nodiscard]] std::optional<ShadowLightMeta> sceneShadowLightMeta(std::size_t index) const {
			if (index >= shadowLightMeta.size()) { return {}; }
			return shadowLightMeta[index];
		}

		/// @brief Chooses FIFO in Debug builds and mailbox in optimized builds.
		[[nodiscard]] static constexpr VulkanSwapchain::PresentModePreference defaultPresentMode() {
#ifdef VVE_SIMPLE_RELEASE_PRESENT_MAILBOX
			return VulkanSwapchain::PresentModePreference::mailbox;
#else
			return VulkanSwapchain::PresentModePreference::fifo;
#endif
		}

		/// @brief Replaces the renderer lights; geometry, materials, and textures come from the bound RenderScene.
		void loadScene(Scene nextScene) {
			scene = std::move(nextScene);
			shadowLightMeta.clear();									// Metadata is rebuilt during the next frame assembly.
		}

		/// @brief Borrows CPU resources; renderer mutation is limited to texture pixel restoration and decode bookkeeping.
		/// Restoration refreshes decoded extent/greyscale, counts attempts and caches failures through textureForUpload().
		void bindRenderScene(RenderScene &renderScene) {
			renderMeshes_ = std::addressof(renderScene.meshes());
			renderMaterials_ = std::addressof(renderScene.materials());
			renderInstances_ = std::addressof(renderScene.instances());
			renderScene_ = std::addressof(renderScene);
		}

		/// @brief Marks instance topology as changed without invalidating unrelated mesh buffers.
		void markSceneResourcesDirty() { sceneResourcesDirty_ = true; }

		/// @brief Marks the dense material buffer for one rebuild.
		void markMaterialsDirty() { sceneMaterialsDirty_ = true; }

		/// @brief Marks one existing CPU mesh for a vertex-buffer refresh.
		void markMeshDirty(RenderMeshHandle mesh) {
			sceneGeometryDirty_.insert(mesh);
			sceneResourcesDirty_ = true;
		}

		/// @brief Removes all renderer lights.
		void clearScene() { loadScene(Scene{}); }

		/// @brief Sets the default view and clip planes used by every target without an explicit camera.
		void setCamera(Camera camera) { camera_ = std::move(camera); }

		/// @brief Returns whether the concrete Vulkan draw path presented a frame.
		[[nodiscard]] bool renderFrame(WindowTarget &target, VulkanReadback *readback = nullptr) { return drawFrame(target, readback); }

		/// @brief Borrows CPU mesh data for diagnostics; invalidated by scene mutation or destruction.
		[[nodiscard]] const RenderMesh *findRenderMesh(RenderMeshHandle handle) const {
			if (renderMeshes_ == nullptr) { return nullptr; }
			const auto found = std::ranges::find(*renderMeshes_, handle, &RenderMesh::handle);
			return found == renderMeshes_->end() ? nullptr : std::addressof(*found);
		}
	private:
		[[nodiscard]] VkResult waitDeviceIdle();
		void collectRetiredResources(std::uint64_t completed);
		/// @brief Keeps a replaced owner alive through the last submission that could have referenced it.
		template <typename Resource> void retire(Resource &&resource) {
			retiredResources_.emplace_back(submitSerial_, std::move(resource));
		}
		[[nodiscard]] VkResult ensureShadowCapacity();
		[[nodiscard]] VkResult writeShadowDescriptors(WindowTarget &target, std::uint32_t frame);
		[[nodiscard]] VkResult createTargetResources(WindowTarget &target); ///< Creates one target's attachments, frame resources and post-processing chain.
		[[nodiscard]] VkResult writeSceneDescriptors(WindowTarget &target, std::uint32_t frame); ///< Updates only a completed slot's materials and changed texture elements.
		[[nodiscard]] VkResult uploadDynamicVertices(WindowTarget &target, std::uint32_t frame);
		static void reportFrameFailure(const char *stage, VkResult result);	///< Logs a skipped frame with its Vulkan result, capped to avoid flooding the console.
		[[nodiscard]] VkResult recordCommandBuffer(WindowTarget &target, std::uint32_t frameIndex, std::uint32_t imageIndex, const Mat4 &viewProjection, bool drawGui);
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> shadowTexel(Vec3 lightNdc);	///< Converts light NDC x/y to one clamped shadow-map texel.
		/// @brief Generation the RenderScene texture table holds in one slot; 0 for a free or missing slot.
		[[nodiscard]] std::uint64_t textureGeneration(std::size_t index) const {
			return renderScene_ != nullptr && index < renderScene_->textures().size() ? renderScene_->textures()[index].generation : 0U;
		}
		/// @brief Reports whether any texture slot differs from the RenderScene texture table.
		[[nodiscard]] bool texturesOutOfDate() const {
			for (std::size_t index{}; index < kMaxSceneTextures; ++index) {
				if (uploadedTextureGenerations_[index] != textureGeneration(index)) { return true; }
			}
			return false;
		}

		FrameUniforms frameUniforms_{}; ///< Camera and packed light data assembled directly for the current window's uniform upload.
		Camera camera_{.position = {Vec3{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}},
			.forward = {Vec3{zero(), static_cast<Scalar>(-5.0), static_cast<Scalar>(-9.0)}}}; ///< Default view retained across target creation and renderer shutdown.
		VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE}; ///< Owned Dear ImGui descriptor pool reserved for backend texture descriptors.
		std::function<bool()> guiPrepare_; ///< Optional GUI preparation before command recording, only for its owning window.
		std::function<void(VkCommandBuffer)> guiRecord_; ///< Optional GUI recorder invoked only when preparation produced vertices.
		std::function<void(vvppl::PostProcessing &)> postProcessSetup_; ///< Optional Post Processing setup
		const Vector<RenderMesh> *renderMeshes_{nullptr}; ///< Borrowed unique CPU meshes owned by RenderSystem.
		const Vector<RenderMaterial> *renderMaterials_{nullptr}; ///< Borrowed CPU materials owned by RenderSystem.
		const Vector<RenderInstance> *renderInstances_{nullptr}; ///< Borrowed draw instances owned by RenderSystem.
		RenderScene *renderScene_{nullptr}; ///< Borrowed owner; renderer may only restore texture pixels and update decode bookkeeping.
		std::optional<Error> textureUploadError_{}; ///< Decode error retained through Vulkan cleanup for the expected-returning caller.
		std::array<std::uint64_t, kMaxSceneTextures> uploadedTextureGenerations_{}; ///< Texture generation resident in each slot; 0 = empty.
		std::size_t uploadedTextureCount_{}; ///< Number of resident objectTextures.
		std::size_t textureUploadSubmitCount_{}; ///< Monotonic number of completed texture upload batches.
		std::vector<GpuMaterial> materials_{}; ///< Dense CPU upload table shared by all windows' material buffers.
		std::size_t uploadedMaterialCount_{}; ///< Number of live entries in the dense material table.
		std::size_t meshUploadCount_{}; ///< Monotonic number of unique-mesh create and refresh uploads.
		std::size_t materialUploadCount_{}; ///< Monotonic number of dense material-buffer uploads.
		std::size_t skippedFrameCount_{}; ///< Fence VK_TIMEOUT and acquire VK_TIMEOUT/VK_NOT_READY skips; never logged as failures.
		std::size_t forwardPipelineCreateCount_{}; ///< Successful forward pipeline creations; resizing must not increment this count.
		std::size_t lastShadowLayerPassCount_{}; ///< Actual shadow layer clears and draws in the most recently recorded window frame.
		FrameDrawStats lastFrameDrawStats_{}; ///< Scene command counts reset before recording each window frame.
		std::uint64_t submitSerial_{}; ///< Monotonic serial assigned to successful submissions on the shared graphics queue.
		std::uint64_t completedSerial_{}; ///< Highest serial covered by a completed graphics fence or device-idle wait.
		std::size_t deviceWaitIdleCount_{}; ///< Explicit device-wide waits since renderer construction.
		std::deque<std::pair<std::uint64_t, std::variant<VulkanMesh, TextureImage, VulkanBuffer, ShadowMap>>> retiredResources_{}; ///< Old owners ordered by last possible submission.
		bool sceneResourcesDirty_{true}; ///< CPU scene topology changed after the last GPU synchronization.
		bool sceneMaterialsDirty_{true}; ///< CPU material additions or removals require a dense-buffer rebuild.
		bool gpuDebugReadback_{false}; ///< False during normal rendering to avoid per-frame GPU stalls.
		std::set<RenderMeshHandle> sceneGeometryDirty_{}; ///< Existing GPU meshes requiring a vertex-buffer refresh.
	};

} // namespace vve::simple

namespace vve::simple::detail {

	/// @brief Up vector for lookAt that stays valid when the view direction is parallel to world +Y or -Y.
	[[nodiscard]] inline Vec3 stableUp(Vec3 direction) {
		const Scalar directionLength{length(direction)};
		const bool vertical = directionLength > zero() && std::abs(direction.y) > static_cast<Scalar>(0.999) * directionLength;
		return vertical ? Vec3{zero(), zero(), one()} : Vec3{zero(), one(), zero()};
	}

} // namespace vve::simple::detail
