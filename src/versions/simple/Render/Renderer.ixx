module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif
#include <VVPPL.h>

export module VEEngine.Simple.Renderer;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.RenderResources;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
	* @file
	* @brief Forward renderer of the simple engine: one CPU Scene, its Vulkan resources, and one frame loop.
	*
	* The class is declared here; its larger member functions live in module implementation units:
	* - RendererResources.cpp: init, uploadSceneTextures, syncSceneResources, recreateSwapchain, cleanup, ImGui wiring.
	* - RendererShadowPrep.cpp: prepareShadowFrame packs enabled lights and builds every shadow matrix.
	* - RendererDraw.cpp: drawFrame and recordCommandBuffer record shadow passes plus the forward color pass and present.
	* - RendererDebug.cpp: shadow-depth samples, optional GPU readback, and PNG capture.
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

	/// @brief Prepared CPU light and shadow data copied into the frame uniform buffer; lights are packed densely by type.
	struct ForwardRendererShadowFrame {
		std::array<Mat4, kShadowMatrixCount> shadowViewProjs{}; ///< Spot [0..), point faces [kShadowMatrixPointBase..), directional cascades [kShadowMatrixDirBase..).
		Vec4 cascadeSplitsFar{}; ///< View-space far distance of each directional shadow cascade.
		std::array<Vec4, kMaxShadowedPointLights> pointLightPositionRanges{}; ///< Point xyz positions with ranges in w.
		std::array<Vec4, kMaxShadowedPointLights> pointLightColorIntensities{}; ///< Point rgb colors with intensities in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightPositionRanges{}; ///< Spot xyz positions with ranges in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightColorIntensities{}; ///< Spot rgb colors with intensities in w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightDirections{}; ///< Spot xyz directions with unused w.
		std::array<Vec4, kMaxShadowedSpotLights> spotLightConeAmbients{}; ///< Spot inner cone cosine, outer cone cosine, unused, ambient.
		std::array<Vec4, kMaxDirectionalLights> directionalLightDirections{}; ///< Directional xyz directions with unused w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightColorIntensities{}; ///< Directional rgb colors with intensities in w.
		std::array<Vec4, kMaxDirectionalLights> directionalLightAmbients{}; ///< Directional ambient term in w.
		std::uint32_t activeDirectionalLightCount{}; ///< Enabled directional lights packed into the arrays.
		std::uint32_t activeSpotLightCount{}; ///< Enabled spot lights packed into the arrays.
		std::uint32_t activePointLightCount{}; ///< Enabled point lights packed into the arrays.
		float shadowCompareBias{0.001F}; ///< CPU mirror of the shader-side spot/point compare bias.
	};

	/// @brief Forward renderer owning the CPU scene mirror, every Vulkan resource, and the per-frame draw loop.
	struct ForwardRenderer {
		/// @brief Lightweight command-recording pass tag used by tests without introducing a render graph.
		enum class RecordedPass : std::uint8_t { directional_shadow, spot_shadow, point_shadow, forward_color };

		static constexpr std::uint32_t framesInFlight{2U}; ///< Number of independent frame command buffers to allocate.
		static constexpr std::size_t pointShadowFaceCount{6U}; ///< One square face per cubemap direction.
		static constexpr Scalar shadowNearPlane{static_cast<Scalar>(0.1)}; ///< Shared near plane for spot and point shadow views.
		static constexpr float occludedShadowFactor{0.35F}; ///< Shader partial-shadow floor.
		static constexpr float directionalCompareBias{0.00005F}; ///< Shader-side bias of the nearest directional cascade.

		VulkanInstance instance{};             ///< Owned Vulkan instance wrapper.
		VulkanSurface surface{};               ///< Owned SDL-backed Vulkan surface wrapper.
		VulkanPhysicalDevice physicalDevice{}; ///< Selected borrowed Vulkan physical device wrapper.
		VulkanDevice device{};                 ///< Owned Vulkan logical device wrapper.
		VulkanAllocator allocator{};           ///< Owned VMA allocator for every buffer and image below.
		VulkanSwapchain swapchain{};           ///< Owned swapchain wrapper for presentation images.
		VulkanImageViews imageViews{};         ///< Owned color image views for swapchain images.
		VulkanImage depthImage{};              ///< Owned swapchain-sized depth attachment image and view.
		VulkanImage hdrImage{};                ///< Owned swapchain-sized RGBA16F color target the scene is rendered into.
		ShadowMap dirShadowArray{};            ///< Owned directional shadow-map texture array with one layer per active directional light.
		ShadowMap spotShadowArray{};           ///< Owned spot shadow-map texture array with one layer per active spot light.
		ShadowMap pointShadowArray{};          ///< Owned point shadow-map texture array with six layers per shadowed point light.
		std::array<TextureImage, kMaxSceneTextures> objectTextures{}; ///< Owned base-color textures keyed by RenderScene texture-table index.
		TextureImage defaultObjectTexture{};    ///< Owned opaque-white texture filling unused texture slots.
		VulkanDescriptorSetLayout descriptorSetLayout{}; ///< Owned frame-uniform and shadow-map descriptor-set layout.
		VulkanPipelineLayout pipelineLayout{}; ///< Owned graphics pipeline layout using the frame descriptor set.
		VulkanShaderModule vertShaderModule{}; ///< Owned forward vertex shader module.
		VulkanShaderModule fragShaderModule{}; ///< Owned forward fragment shader module.
		VulkanShaderModule shadowShaderModule{}; ///< Owned depth-only shadow vertex shader module.
		VulkanGraphicsPipeline graphicsPipeline{}; ///< Owned forward graphics pipeline for swapchain rendering.
		VulkanGraphicsPipeline shadowPipeline{};   ///< Owned depth-only pipeline shared by every shadow layer.
		VulkanCommandPool commandPool{};       ///< Owned resettable command pool for the graphics queue family.
		VulkanCommandBuffers commandBuffers{}; ///< Owned primary command buffers, one for each frame in flight.
		VulkanFrameSync frameSync{};           ///< Owned per-frame semaphores and fences for rendering.
		VulkanUniformBuffers uniformBuffers{}; ///< Owned per-frame uniform buffers for camera and object data.
		VulkanBuffer materialBuffer{};        ///< Owned host-visible dense GpuMaterial storage buffer.
		VulkanDescriptorPool descriptorPool{}; ///< Owned descriptor pool for per-frame uniform and shadow-map descriptor sets.
		VulkanDescriptorSets descriptorSets{}; ///< Owned per-frame descriptor sets binding frame uniform buffers and the shadow map.
		std::map<RenderMeshHandle, VulkanMesh> meshes{}; ///< One owned GPU mesh per referenced RenderMesh handle.
		std::map<RenderMaterialHandle, std::uint32_t> materialSlots_{}; ///< Stable material handle to dense GPU buffer slot.
		VulkanReadback shadowDepthReadback{};  ///< Single shadow-map layer readback shared by all light types.
		SDL_Window *window{nullptr};           ///< Borrowed SDL window used to create the Vulkan surface.
		Scene scene{}; ///< CPU scene data kept in STL containers until renderer upload exists.
		std::vector<ShadowLightMeta> shadowLightMeta{}; ///< Per-light shadow slot/layer metadata prepared every frame.
		std::vector<RenderShadowDepthSample> shadowDepthSamples{}; ///< One sample per shadow-casting light, rebuilt every frame.
		std::vector<RecordedPass> recordedPassOrder{}; ///< Last frame's command-recording pass order diagnostic.
		Vec3 cameraEye{zero(), static_cast<Scalar>(6.0), static_cast<Scalar>(9.0)}; ///< World-space camera position used for the frame view matrix.
		Vec3 cameraTarget{zero(), one(), zero()}; ///< World-space point looked at by the frame view matrix.
		Scalar cameraVerticalFov{FovY{}.radians}; ///< Vertical field of view supplied by the facade camera.
		std::optional<std::uint32_t> lastRenderedImageIndex{}; ///< Swapchain image index from the last acquired, rendered, and presented frame.
		std::optional<VkResult> lastReadbackCaptureResult{}; ///< Result from the optional in-frame color readback.
		std::unique_ptr<vvppl::PostProcessing> postProcess{}; ///< Owned post-processing chain applied to the finished color image.

		~ForwardRenderer() { cleanup(); }

		// Vulkan resource lifetime (RendererResources.cpp).
		[[nodiscard]] VkResult init(SDL_Window *sdlWindow);					///< Creates every Vulkan object and uploads the current scene; returns the first failing result.
		[[nodiscard]] VkResult uploadSceneTextures();								///< Uploads Scene::textures into the texture slots and rebinds all frame descriptor sets.
		[[nodiscard]] VkResult uploadSceneMaterials();							///< Rebuilds the dense GPU material buffer and handle lookup.
		[[nodiscard]] VkResult syncSceneResources();								///< Brings GPU meshes and textures in line with CPU scene changes.
		[[nodiscard]] VkResult recreateSwapchain(VkExtent2D requestedExtent);	///< Rebuilds swapchain-sized resources after a resize.
		[[nodiscard]] VkExtent2D currentWindowPixelExtent() const;
		void cleanup();																	///< Releases Vulkan device resources in reverse creation order.
		void createImguiDescriptorPool();
		[[nodiscard]] ImGui_ImplVulkan_InitInfo makeImguiInitInfo() const;	///< Builds dormant Dear ImGui Vulkan backend data from the renderer-owned objects.

		// CPU shadow preparation (RendererShadowPrep.cpp).
		[[nodiscard]] ForwardRendererShadowFrame prepareShadowFrame(const Mat4 &cameraView, Scalar cameraVerticalFov, Scalar cameraAspect, Scalar cameraNear, Scalar cameraFar);

		// Frame recording and presentation (RendererDraw.cpp).
		void drawFrame(VulkanReadback *readback = nullptr);						///< Draws one swapchain frame; the optional readback captures it for deterministic debug output.

		// Diagnostics (RendererDebug.cpp).
		void setGpuDebugReadback(bool enabled) { gpuDebugReadback_ = enabled; }	///< Enables the per-frame GPU shadow-depth readback for verification runs.
		void recordShadowDepthSamples(const ForwardRendererShadowFrame &shadowFrame);	///< Projects the world origin through every active shadow matrix.
		void fillShadowDepthSamplesFromGpu();												///< Reads the rendered shadow-map texel of every recorded sample.
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error>;

		/// @brief Returns command-recorded pass tags from the most recent frame.
		[[nodiscard]] std::span<const RecordedPass> lastRecordedPassOrder() const { return recordedPassOrder; }

		/// @brief Releases renderer-owned resources through the existing cleanup path.
		void shutdown() { cleanup(); }

		/// @brief Stores the non-owning GUI system handle reserved for later GUI rendering integration.
		void setGuiSystem(void *gui) { guiSystem_ = gui; }

		/// @brief Stores the optional GUI command recorder used inside the forward color pass.
		void setGuiRecordSink(std::function<void(VkCommandBuffer)> sink) { guiRecord_ = std::move(sink); }

		/// @brief Stores the post processing setup, which sets and configures effects.
		void setPostProcessSetup(std::function<void(vvppl::PostProcessing &)> setup){ postProcessSetup_ = std::move(setup); }

		/// @brief Reports whether the renderer currently owns a live Vulkan device.
		[[nodiscard]] bool initialized() const { return device.device != VK_NULL_HANDLE; }

		/// @brief Reports the number of resident GPU textures, excluding the fallback texture.
		[[nodiscard]] std::size_t gpuTextureCount() const { return uploadedTextureCount_; }
		/// @brief Reports the number of unique resident GPU meshes.
		[[nodiscard]] std::size_t gpuMeshCount() const { return meshes.size(); }
		/// @brief Reports the number of entries in the dense GPU material table.
		[[nodiscard]] std::size_t gpuMaterialCount() const { return uploadedMaterialCount_; }
		/// @brief Reports the monotonic number of mesh create or vertex-refresh uploads.
		[[nodiscard]] std::size_t gpuMeshUploadCount() const { return meshUploadCount_; }
		/// @brief Reports the monotonic number of material-buffer uploads.
		[[nodiscard]] std::size_t gpuMaterialUploadCount() const { return materialUploadCount_; }

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

		/// @brief Replaces the current CPU scene before future renderer upload.
		void loadScene(Scene nextScene) {
			scene = std::move(nextScene);								// Scene upload invalidates per-frame shadow metadata.
			shadowLightMeta.clear();									// Metadata is rebuilt during the next frame assembly.
			 sceneGeometryDirty_.clear();
			sceneResourcesDirty_ = true;
			sceneRequiresFullUpload_ = true;
			sceneMaterialsDirty_ = true;
		}

		/// @brief Binds the unique CPU render-resource vectors owned by RenderSystem.
		void bindRenderScene(const Vector<RenderMesh> &renderMeshes, const Vector<RenderMaterial> &renderMaterials,
							 const Vector<RenderInstance> &renderInstances) {
			renderMeshes_ = std::addressof(renderMeshes);
			renderMaterials_ = std::addressof(renderMaterials);
			renderInstances_ = std::addressof(renderInstances);
		}

		/// @brief Appends one stable RenderScene texture-table entry to the backend upload view.
		void appendTexture(const RenderTexture &texture) {
			const auto found = std::ranges::find(scene.textures, texture.canonical_path,
				[](const RenderTexture *entry) -> const std::filesystem::path & { return entry->canonical_path; });
			if (found != scene.textures.end() || scene.textures.size() >= kMaxSceneTextures) { return; }
			scene.textures.push_back(std::addressof(texture));
			sceneResourcesDirty_ = true;
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

		/// @brief Clears the renderer-owned CPU scene through the existing scene replacement path.
		void clearScene() { loadScene(Scene{}); }

		/// @brief Stores the camera eye and target used by future frame uniform updates.
		void setCamera(Vec3 eye, Vec3 target, Scalar verticalFov) {
			cameraEye = eye;
			cameraTarget = target;
			cameraVerticalFov = verticalFov;
		}

		/// @brief Common frame-render entry forwarding to the concrete Vulkan draw path.
		void renderFrame(VulkanReadback *readback = nullptr) { drawFrame(readback); }

	private:
		static void reportFrameFailure(const char *stage, VkResult result);	///< Logs a skipped frame with its Vulkan result, capped to avoid flooding the console.
		[[nodiscard]] VkResult recordCommandBuffer(std::uint32_t frameIndex, std::uint32_t imageIndex, const ForwardRendererShadowFrame &shadowFrame);
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> shadowTexel(Vec3 lightNdc);	///< Converts light NDC x/y to one clamped shadow-map texel.
		[[nodiscard]] const RenderMesh *findRenderMesh(RenderMeshHandle handle) const {
			if (renderMeshes_ == nullptr) { return nullptr; }
			const auto found = std::ranges::find(*renderMeshes_, handle, &RenderMesh::handle);
			return found == renderMeshes_->end() ? nullptr : std::addressof(*found);
		}
		std::uint32_t currentFrame{0U}; ///< Index of the frame synchronization set used by the next draw.
		VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE}; ///< Owned Dear ImGui descriptor pool reserved for backend texture descriptors.
		void *guiSystem_{nullptr}; ///< Non-owning, type-erased GUI system pointer reserved for later GUI integration.
		std::function<void(VkCommandBuffer)> guiRecord_; ///< Optional GUI recorder invoked during the forward color pass.
		std::function<void(vvppl::PostProcessing &)> postProcessSetup_; ///< Optional Post Processing setup
		const Vector<RenderMesh> *renderMeshes_{nullptr}; ///< Borrowed unique CPU meshes owned by RenderSystem.
		const Vector<RenderMaterial> *renderMaterials_{nullptr}; ///< Borrowed CPU materials owned by RenderSystem.
		const Vector<RenderInstance> *renderInstances_{nullptr}; ///< Borrowed draw instances owned by RenderSystem.
		std::size_t uploadedTextureCount_{}; ///< Resident objectTextures prefix keyed by RenderScene texture-table index.
		std::size_t uploadedMaterialCount_{}; ///< Number of current entries in materialBuffer.
		std::size_t materialBufferCapacity_{}; ///< Allocated GpuMaterial capacity retained across scene shrinkage.
		std::size_t meshUploadCount_{}; ///< Monotonic number of unique-mesh create and refresh uploads.
		std::size_t materialUploadCount_{}; ///< Monotonic number of dense material-buffer uploads.
		bool sceneResourcesDirty_{true}; ///< CPU scene topology or texture changed after the last GPU synchronization.
		bool sceneRequiresFullUpload_{true}; ///< Scene replacement invalidates the renderer-owned resource views.
		bool sceneMaterialsDirty_{true}; ///< CPU material additions or removals require a dense-buffer rebuild.
		bool gpuDebugReadback_{false}; ///< False during normal rendering to avoid per-frame GPU stalls.
		std::set<RenderMeshHandle> sceneGeometryDirty_{}; ///< Existing GPU meshes requiring a vertex-buffer refresh.
	};

} // namespace vve::simple
