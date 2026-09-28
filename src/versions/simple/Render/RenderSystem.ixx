module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif
#include <VVPPL.h>

export module VEEngine.Simple:RenderSystem;
import std;
export import VEEngine.Simple.Types;
import :Window;
import VEEngine.Simple.Vulkan;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;
export import VEEngine.Simple.RenderResources;

/// @file
/// @brief Simple render coordinator: renderer backend ownership and scene mirroring.

namespace vve::simple::detail {

	/// @brief Inner (full-intensity) spot cone used when the caller only gives the outer cone.
	[[nodiscard]] inline auto defaultInnerCone(SpotConeAngle outer) -> SpotConeAngle {
		return SpotConeAngle{.radians = std::min(ForwardSpotLight{}.innerConeAngle.radians, outer.radians * static_cast<Scalar>(0.5))};
	}

	/// @brief Builds the backend model matrix from the public transform contract.
	[[nodiscard]] inline auto modelMatrix(Transform transform) -> Mat4 {
		const auto q = transform.rotation.value;
		auto rotation = identityMat4();
		rotation[0][0] = one() - static_cast<Scalar>(2) * (q.y * q.y + q.z * q.z);
		rotation[0][1] = static_cast<Scalar>(2) * (q.x * q.y + q.w * q.z);
		rotation[0][2] = static_cast<Scalar>(2) * (q.x * q.z - q.w * q.y);
		rotation[1][0] = static_cast<Scalar>(2) * (q.x * q.y - q.w * q.z);
		rotation[1][1] = one() - static_cast<Scalar>(2) * (q.x * q.x + q.z * q.z);
		rotation[1][2] = static_cast<Scalar>(2) * (q.y * q.z + q.w * q.x);
		rotation[2][0] = static_cast<Scalar>(2) * (q.x * q.z + q.w * q.y);
		rotation[2][1] = static_cast<Scalar>(2) * (q.y * q.z - q.w * q.x);
		rotation[2][2] = one() - static_cast<Scalar>(2) * (q.x * q.x + q.y * q.y);
		auto model = translate(identityMat4(), transform.translation.value);
		model = multiply(model, rotation);
		return scale(model, transform.scale.value);
	}

} // namespace vve::simple::detail

export namespace vve::simple {

	/// @brief Non-owning read callbacks for imported asset scenes owned by the engine.
	/// Geometry shares ownership; index spans borrow catalog storage until mutation or destruction. Callbacks must not mutate it.
	struct ImportedAssetReadAccess {
		std::function<std::expected<Vector<NodeHandle>, Error>(SceneHandle)> scene_nodes{};						///< Lists scene nodes.
		std::function<std::expected<NodeHandle, Error>(SceneHandle)> scene_root_node{};							///< Returns the root node.
		std::function<std::expected<Vector<NodeHandle>, Error>(SceneHandle, NodeHandle)> scene_node_children{};	///< Lists child nodes.
		std::function<std::expected<Transform, Error>(NodeHandle)> node_transform{};								///< Returns local transform.
		std::function<std::expected<Vector<MeshHandle>, Error>(NodeHandle)> node_meshes{};						///< Lists meshes attached to a node.
		std::function<std::expected<MaterialHandle, Error>(MeshHandle)> mesh_material{};							///< Returns the mesh material.
		std::function<std::expected<LinearColor, Error>(MaterialHandle)> material_base_color{};				///< Returns the material base-color factor.
		std::function<std::expected<Vector<MaterialTextureSource>, Error>(MaterialHandle)> material_texture_sources{}; ///< Lists typed canonical material textures.
		std::function<std::expected<MaterialFactors, Error>(MaterialHandle)> material_factors{};				///< Returns imported roughness, metalness, and emissive factors.
		std::function<std::expected<Vector<LightHandle>, Error>(SceneHandle)> scene_lights{};					///< Lists scene lights.
		std::function<std::expected<LightDescriptor, Error>(LightHandle)> light_data{};							///< Returns imported light data.
		std::function<std::expected<Vector<CameraHandle>, Error>(SceneHandle)> scene_cameras{};				///< Lists scene cameras.
		std::function<std::expected<CameraDescriptor, Error>(CameraHandle)> camera_data{};						///< Returns imported camera data.
		std::function<std::expected<std::shared_ptr<const std::vector<RenderVertex>>, Error>(MeshHandle)> mesh_geometry{}; ///< Shares immutable imported vertices.
		std::function<std::expected<std::span<const std::uint32_t>, Error>(MeshHandle)> mesh_indices{};					///< Returns mesh indices.
	};


	/// @brief simple render facade coordinating the renderer backend and CPU render scene.
	class RenderSystem {
	public:
		RenderSystem();
		explicit RenderSystem(ImportedAssetReadAccess imported_assets, const WindowSystem *windows = nullptr);
		// renderer_ keeps pointers into scene_, so a copied or moved RenderSystem would render the old object's scene.
		RenderSystem(const RenderSystem &) = delete;
		RenderSystem(RenderSystem &&) = delete;
		RenderSystem &operator=(const RenderSystem &) = delete;
		RenderSystem &operator=(RenderSystem &&) = delete;
		[[nodiscard]] auto instantiateScene(SceneHandle scene, SceneInstantiationOptions options = {})	-> std::expected<RenderSceneInstanceHandle, Error>;

		// Object state, cameras, and lights mirrored into the renderer CPU scene (RenderSystemScene.cpp).
		[[nodiscard]] auto setObjectUnlit(RenderObjectHandle handle, bool unlit)										-> std::expected<void, Error>;
		[[nodiscard]] auto setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow)					-> std::expected<void, Error>;
		[[nodiscard]] auto setObjectVisible(RenderObjectHandle handle, bool visible)									-> std::expected<void, Error>;
		[[nodiscard]] auto objectVisible(RenderObjectHandle handle) const												-> std::expected<bool, Error>;
		[[nodiscard]] auto setObjectTransform(RenderObjectHandle handle, Transform transform)					-> std::expected<void, Error>;
		[[nodiscard]] auto objectTransform(RenderObjectHandle handle) const											-> std::expected<Transform, Error>;
		auto setCamera(Camera camera) -> void;
		[[nodiscard]] auto setCamera(WindowHandle window, Camera camera) -> std::expected<void, Error>;
		[[nodiscard]] auto clearCamera(WindowHandle window) -> std::expected<void, Error>;
		auto clearLights() -> void;
		auto setDirectionalLight(Direction direction, LinearColor color, LightIntensity intensity, LinearColor ambient) -> void;
		auto addDirectionalLight(Direction direction, LinearColor color, LightIntensity intensity, LinearColor ambient) -> void;
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range, LinearColor ambient) -> void;
		auto addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
		auto addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range, LinearColor ambient) -> void;
		auto setSpotLight(Position position, Direction direction, LinearColor color, LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void;
		auto setSpotLight(Position position, Direction direction, LinearColor color, LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void;
		auto addSpotLight(Position position, Direction direction, LinearColor color, LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void;
		auto addSpotLight(Position position, Direction direction, LinearColor color, LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void;

		// Primitive objects, object removal, and loaded-scene lifecycle (RenderSystemObjects.cpp).
		[[nodiscard]] auto removeObject(RenderObjectHandle handle)																-> std::expected<void, Error>;
		[[nodiscard]] auto sceneInstanceObjects(RenderSceneInstanceHandle instance) const						-> std::expected<Vector<RenderObjectHandle>, Error>;
		[[nodiscard]] auto objectSourceScene(RenderObjectHandle handle) const										-> std::expected<RenderSceneInstanceHandle, Error>;
		[[nodiscard]] auto objectSourceNode(RenderObjectHandle handle) const											-> std::expected<NodeHandle, Error>;
		[[nodiscard]] auto removeSceneInstance(RenderSceneInstanceHandle instance)									-> std::expected<void, Error>;
		[[nodiscard]] auto removeScene(SceneHandle handle)																	-> std::expected<void, Error>;
		[[nodiscard]] auto purgeUnusedAssets()																						-> std::size_t;
		[[nodiscard]] auto addPlane(Vec2 half_extent, LinearColor color, Transform transform = {})			-> std::expected<RenderObjectHandle, Error>;
		[[nodiscard]] auto addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color, Transform transform = {}) -> std::expected<RenderObjectHandle, Error>;
		[[nodiscard]] auto addTriangleMesh(Vector<Vec3> positions, Vector<std::uint32_t> indices, LinearColor color, Transform transform = {}) -> std::expected<RenderObjectHandle, Error>;
		[[nodiscard]] auto setObjectMeshPositions(RenderObjectHandle handle, Vector<Vec3> positions)			-> std::expected<void, Error>;
		[[nodiscard]] auto addTexturedCuboid(Vec3 minimum, Vec3 maximum, std::filesystem::path base_color_texture, Transform transform = {}) -> std::expected<RenderObjectHandle, Error>;
		auto clearScene()																												-> void;
		auto loadScene(Scene scene)																									-> SceneHandle;

		[[nodiscard]] auto sceneTextureCount() const -> std::size_t;
		[[nodiscard]] auto textureDecodeCount() const -> std::size_t;
		[[nodiscard]] auto sceneTexturePixelBytes() const -> std::size_t;
		[[nodiscard]] auto sceneTextureIsLinear(std::size_t index) const -> std::expected<bool, Error>;
		[[nodiscard]] auto gpuTextureCount() const -> std::size_t;
		[[nodiscard]] auto gpuMeshCount() const -> std::size_t;
		[[nodiscard]] auto gpuMaterialCount() const -> std::size_t;
		[[nodiscard]] auto gpuMeshUploadCount() const -> std::size_t;
		[[nodiscard]] auto gpuMaterialUploadCount() const -> std::size_t;
		[[nodiscard]] auto renderMaterials() const -> const Vector<RenderMaterial> &;

		auto waitIdle() -> void;
		auto setGuiPrepareSink(std::function<bool()> sink)														-> void;
		auto setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)												-> void;
		auto setPostProcessSetup(std::function<void(vvppl::PostProcessing &)> setup)											-> void;
		[[nodiscard]] static bool supportsRenderer(const RendererId &id);
		[[nodiscard]] auto initialize(WindowSystem &windows)												-> std::expected<void, Error>;
		[[nodiscard]] auto makeGuiInitInfo() const																			-> std::optional<ImGui_ImplVulkan_InitInfo>;
		[[nodiscard]] auto forward()																								-> ForwardRenderer &;
		[[nodiscard]] auto forward() const																						-> const ForwardRenderer &;
		auto shutdown()																													-> void;
		[[nodiscard]] auto initialized() const																					-> bool;
		[[nodiscard]] auto sceneMeshCount() const																					-> std::size_t;
		[[nodiscard]] auto sceneMaterialCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneDirectionalLightCount() const																-> std::size_t;
		[[nodiscard]] auto scenePointLightCount() const																		-> std::size_t;
		[[nodiscard]] auto sceneSpotLightCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneCameraCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneInstanceCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneVertexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneIndexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneShadowLightMetaCount() const														-> std::size_t;
		[[nodiscard]] auto sceneShadowLightMeta(std::size_t index) const										-> std::optional<ShadowLightMeta>;
		[[nodiscard]] auto shadowDepthSamples() const																	-> std::span<const RenderShadowDepthSample>;
		auto setGpuDebugReadback(bool enabled)																			-> void;
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path)								-> std::expected<void, Error>;
		[[nodiscard]] auto captureFrameToPng(WindowHandle window, const std::filesystem::path &output_path) -> std::expected<void, Error>;
		[[nodiscard]] auto hasSceneCamera() const																					-> bool;
		[[nodiscard]] auto hasSceneDirectionalLight() const																	-> bool;
		[[nodiscard]] auto hasScenePointLight() const																			-> bool;
		[[nodiscard]] auto hasSceneSpotLight() const																				-> bool;
		[[nodiscard]] auto renderFrame(WindowSystem &windows)																	-> std::expected<void, Error>;
		[[nodiscard]] auto renderedFrameCount() const																			-> std::uint64_t;
		[[nodiscard]] auto renderingFramesPerSecond() const																-> double;
		[[nodiscard]] auto lastRenderedWindowCount() const																		-> std::size_t;

	private:
		/// @brief Primitive geometry families with independently cached local extents.
		enum class PrimitiveShape {
			plane, ///< Flat XZ quad.
			cuboid ///< Six textured faces.
		};
		[[nodiscard]] auto acquirePrimitiveMaterial(LinearColor color, RenderTextureIndex texture = kNoTexture) -> RenderMaterialHandle;
		[[nodiscard]] auto acquirePrimitiveMesh(PrimitiveShape shape, Vec3 minimum, Vec3 maximum) -> RenderMeshHandle;
		[[nodiscard]] auto updateCamera(WindowHandle window, std::optional<Camera> camera) -> std::expected<void, Error>;
		[[nodiscard]] auto registerRenderObject(RenderInstanceHandle instance)								-> RenderObjectHandle;
		auto addImportedLight(const LightDescriptor &light, std::uint64_t owner)								-> void;
		auto removeImportedLights(std::uint64_t owner)																-> void;
		auto rollbackSceneInstance(RenderSceneInstanceHandle instance)												-> void;
		[[nodiscard]] auto findRenderObject(RenderObjectHandle handle) const
			-> std::optional<RenderInstanceHandle>;
		auto eraseRenderObject(RenderObjectHandle handle)														-> void;
		[[nodiscard]] auto importedSceneNodes(SceneHandle scene) const									-> Vector<NodeHandle>;
		[[nodiscard]] auto importedSceneWorldTransforms(SceneHandle scene) const
			-> Vector<std::tuple<NodeHandle, Transform, Mat4>>;
		[[nodiscard]] auto importedSceneMeshInstances(SceneHandle scene) const
			-> Vector<std::tuple<NodeHandle, MeshHandle, MaterialHandle, Transform, Mat4>>;
		[[nodiscard]] auto acquireRenderMesh(MeshHandle imported_mesh)									-> std::optional<RenderMeshHandle>;
		[[nodiscard]] auto acquireRenderMaterial(MaterialHandle imported_material)						-> RenderMaterialHandle;

		RenderScene scene_{};															///< Active CPU render scene.
		ForwardRenderer renderer_{};													///< Forward renderer backend.
		ImportedAssetReadAccess imported_assets_{};								///< Borrowed asset-scene queries.
		const WindowSystem *window_system_{nullptr};							///< Borrowed window owner for camera assignments before lazy renderer initialization.
		std::map<WindowHandle, Camera> pending_cameras_{};					///< Camera overrides waiting to move into their window targets.
		std::unordered_map<MeshHandle, RenderMeshHandle, HandleHash<MeshHandle>> imported_render_meshes_{};	///< Imported mesh cache.
		std::unordered_map<MaterialHandle, RenderMaterialHandle, HandleHash<MaterialHandle>> imported_render_materials_{};	///< Imported material cache.
		std::map<std::pair<std::array<Scalar, 3>, RenderTextureIndex>, RenderMaterialHandle> primitive_materials_{}; ///< Materials keyed by colour and texture slot.
		std::map<std::pair<PrimitiveShape, std::array<Scalar, 6>>, RenderMeshHandle> primitive_meshes_{}; ///< Meshes keyed by shape and local min/max extents.
		RenderMaterialHandle default_material_{}; ///< Shared fallback for imported meshes without a material.
		std::unordered_map<RenderObjectHandle, RenderInstanceHandle, HandleHash<RenderObjectHandle>>
			render_objects_{};														///< Public render-object to internal instance map.
		std::unordered_map<RenderObjectHandle, std::pair<RenderSceneInstanceHandle, NodeHandle>, HandleHash<RenderObjectHandle>>
			object_sources_{};														///< Public render-object source scene and node map.
		std::set<SceneHandle> scenes_{};											///< Backend light scenes loaded with loadScene(Scene).
		std::map<RenderSceneInstanceHandle, Vector<RenderObjectHandle>> scene_instances_{};	///< Render objects created per scene instance.
		std::map<RenderSceneInstanceHandle, SceneHandle> scene_instance_sources_{};	///< Asset scene used to create each scene instance.
		std::uint64_t rendered_frames_{0};											///< Number of presented frames.
		std::uint64_t render_fps_frames_{0};										///< Presented frames accumulated for the render-FPS sample.
		std::chrono::steady_clock::time_point render_fps_start_{};		///< Start of the current render-FPS sample window.
		double render_fps_{};															///< Last measured render-frame throughput.
		std::size_t last_window_count_{0};											///< Number of windows presented in the last frame call.
		bool initialized_{false};														///< True after the concrete renderer is initialized.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Stores read access to imported asset-scene descriptors owned by the engine.
	inline RenderSystem::RenderSystem() {
		renderer_.bindRenderScene(scene_);
	}

	/// @brief Stores read access to imported asset-scene descriptors owned by the engine.
	inline RenderSystem::RenderSystem(ImportedAssetReadAccess imported_assets, const WindowSystem *windows)
		: imported_assets_{std::move(imported_assets)}, window_system_{windows} {
		renderer_.bindRenderScene(scene_);
	}

	/// @brief Returns the forward renderer backend.
	inline auto RenderSystem::forward()																			-> ForwardRenderer &{ return renderer_; }

	/// @brief Returns the forward renderer backend.
	inline auto RenderSystem::forward() const																	-> const ForwardRenderer &{ return renderer_; }

	/// @brief Mints a process-wide unique public render-object handle for one internal scene instance.
	inline auto RenderSystem::registerRenderObject(RenderInstanceHandle instance)
		-> RenderObjectHandle{
		const auto handle = makeCounterHandle<RenderObjectHandle>();
		render_objects_.emplace(handle, instance);
		renderer_.markSceneResourcesDirty();
		return handle;
	}

	/// @brief Looks up the internal instance behind a public render-object handle.
	inline auto RenderSystem::findRenderObject(RenderObjectHandle handle) const
		-> std::optional<RenderInstanceHandle>{
		const auto found = render_objects_.find(handle);
		return found == render_objects_.end() ? std::nullopt :
														 std::optional<RenderInstanceHandle>{found->second};
	}

	/// @brief Removes one public render-object mapping.
	inline auto RenderSystem::eraseRenderObject(RenderObjectHandle handle)								-> void{
		render_objects_.erase(handle);
	}

	/// @brief Forwards GUI preparation so the renderer can skip empty GUI passes.
	inline auto RenderSystem::setGuiPrepareSink(std::function<bool()> sink) -> void {
		renderer_.setGuiPrepareSink(std::move(sink));
	}

	/// @brief Forwards the GUI recorder into the active forward renderer.
	inline auto RenderSystem::setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)			-> void{
		renderer_.setGuiRecordSink(std::move(sink));
	}

	/// @brief Forwards the post processing setup into the active forward renderer.
	/// @note The callback runs once per rendered window when its target is created. The PostProcessing and settings
	/// references stay valid until that window closes or the engine shuts down, including across resizes, so they may be kept
	/// and changed at any time.
	inline auto RenderSystem::setPostProcessSetup(std::function<void(vvppl::PostProcessing &)> setup) -> void {
		renderer_.setPostProcessSetup(std::move(setup));
	}

	/// @brief Accepts forward rendering by default, or the explicit opt-out for a window.
	inline bool RenderSystem::supportsRenderer(const RendererId &id) {
		return id.value.empty() || id.value == "forward" || id.value == "none";
	}

	/// @brief Creates one target per forward window in window order; an empty target list is a valid opt-out.
	inline auto RenderSystem::initialize(WindowSystem &windows) -> std::expected<void, Error> {
		if (initialized_) { return {}; }
		window_system_ = &windows;
		const auto entries = windows.windows();
		// Validate all renderer ids before creating any GPU resources.
		for (const auto &entry : entries) {
			if (!supportsRenderer(entry.get().rendererId())) { return std::unexpected(Error::invalid_argument); }
		}
		for (const auto &entry : entries) {
			const auto &window = entry.get();
			if (window.rendererId().value == "none" || window.info().should_close) { continue; }
			const VkResult result = renderer_.initialized()
				? renderer_.addTarget(window.native(), window.info().handle) : renderer_.init(window.native(), window.info().handle);
			if (result != VK_SUCCESS) {
				const auto error = renderer_.textureUploadError().value_or(Error::platform_error);
				renderer_.shutdown();
				return std::unexpected(error);
			}
			if (const auto camera = pending_cameras_.find(window.info().handle); camera != pending_cameras_.end()) {
				renderer_.targets.back().camera = camera->second;
			}
		}
		pending_cameras_.clear();
		initialized_ = true;
		return {};
	}

	inline auto RenderSystem::makeGuiInitInfo() const													-> std::optional<ImGui_ImplVulkan_InitInfo>{
		if (!initialized_) { return std::nullopt; }
		auto info = renderer_.makeImguiInitInfo();
		if (info.Device == VK_NULL_HANDLE || info.DescriptorPool == VK_NULL_HANDLE) {
			return std::nullopt;
		}
		return info;
	}

	/// @brief Waits for renderer-owned Vulkan work before dependent resources are destroyed.
	inline auto RenderSystem::waitIdle() -> void {
		if (!initialized_) { return; }
		(void)renderer_.waitIdle();
	}

	inline auto RenderSystem::shutdown()																				-> void{
		if (initialized_) {
			waitIdle();
			renderer_.shutdown();
			initialized_ = false;
		}
	}

	inline auto RenderSystem::initialized() const																	-> bool{
		return initialized_;
	}

	/// @brief Returns lifetime decode attempts, including failures; clearScene() only resets the failure cache.
	inline std::size_t RenderSystem::textureDecodeCount() const { return scene_.textureDecodeCount(); }
	/// @brief Counts decoded CPU pixel bytes still retained by live texture slots.
	inline std::size_t RenderSystem::sceneTexturePixelBytes() const {
		std::size_t bytes{};
		// Released and unused slots contribute no decoded storage.
		for (const auto &texture : scene_.textures()) { bytes += texture.rgba8.size(); }
		return bytes;
	}
	inline std::size_t RenderSystem::sceneTextureCount() const { return scene_.textureCount(); }
	inline auto RenderSystem::sceneTextureIsLinear(std::size_t index) const -> std::expected<bool, Error> {
		const auto *texture = scene_.findTexture(static_cast<RenderTextureIndex>(index));
		return texture == nullptr ? std::unexpected(Error::missing_object) : std::expected<bool, Error>{texture->linear};
	}
	inline std::size_t RenderSystem::gpuTextureCount() const { return renderer_.gpuTextureCount(); }
	inline std::size_t RenderSystem::gpuMeshCount() const { return renderer_.gpuMeshCount(); }
	inline std::size_t RenderSystem::gpuMaterialCount() const { return renderer_.gpuMaterialCount(); }
	inline std::size_t RenderSystem::gpuMeshUploadCount() const { return renderer_.gpuMeshUploadCount(); }
	inline std::size_t RenderSystem::gpuMaterialUploadCount() const { return renderer_.gpuMaterialUploadCount(); }
	inline auto RenderSystem::renderMaterials() const -> const Vector<RenderMaterial> & { return scene_.materials(); }
	inline std::size_t RenderSystem::sceneMeshCount() const { return scene_.meshCount(); }
	inline std::size_t RenderSystem::sceneMaterialCount() const { return scene_.materialCount(); }
	/// @brief Returns the number of active directional lights.
	inline std::size_t RenderSystem::sceneDirectionalLightCount() const { return renderer_.scene.directionalLights.size(); }
	/// @brief Returns the number of active point lights.
	inline std::size_t RenderSystem::scenePointLightCount() const { return renderer_.scene.pointLights.size(); }
	/// @brief Returns the number of active spot lights.
	inline std::size_t RenderSystem::sceneSpotLightCount() const { return renderer_.scene.spotLights.size(); }
	/// @brief Returns the number of imported cameras applied to the render scene.
	inline std::size_t RenderSystem::sceneCameraCount() const { return scene_.importedCameraCount(); }
	inline std::size_t RenderSystem::sceneInstanceCount() const { return scene_.instanceCount(); }
	inline std::size_t RenderSystem::sceneVertexCount() const { return scene_.vertexCount(); }
	inline std::size_t RenderSystem::sceneIndexCount() const { return scene_.indexCount(); }
	/// @brief Returns the prepared shadow metadata row count.
	inline std::size_t RenderSystem::sceneShadowLightMetaCount() const { return forward().sceneShadowLightMetaCount(); }
	/// @brief Returns one prepared shadow metadata row.
	inline std::optional<ShadowLightMeta> RenderSystem::sceneShadowLightMeta(std::size_t index) const { return forward().sceneShadowLightMeta(index); }
	/// @brief Returns the shadow-depth samples recorded by the last rendered frame.
	inline auto RenderSystem::shadowDepthSamples() const -> std::span<const RenderShadowDepthSample> { return renderer_.shadowDepthSamples; }
	/// @brief Enables the per-frame GPU shadow-depth readback for verification runs.
	inline auto RenderSystem::setGpuDebugReadback(bool enabled) -> void { renderer_.setGpuDebugReadback(enabled); }
	/// @brief Copies the last rendered swapchain image and writes it as a PNG.
	inline auto RenderSystem::captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		if (output_path.empty()) { return std::unexpected(Error::invalid_argument); }
		if (renderer_.targets.empty()) { return std::unexpected(Error::missing_object); }
		return renderer_.captureFrameToPng(renderer_.targets.front(), output_path);
	}
	/// @brief Captures a specific rendered window; unknown, opted-out and closed windows have no target.
	inline auto RenderSystem::captureFrameToPng(WindowHandle window, const std::filesystem::path &output_path) -> std::expected<void, Error> {
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		if (output_path.empty()) { return std::unexpected(Error::invalid_argument); }
		const auto target = std::ranges::find(renderer_.targets, window, &WindowTarget::handle);
		if (target == renderer_.targets.end()) { return std::unexpected(Error::invalid_handle); }
		return renderer_.captureFrameToPng(*target, output_path);
	}
	inline bool RenderSystem::hasSceneCamera() const { return scene_.hasCamera(); }
	/// @brief Reports whether the renderer stores any directional light.
	inline bool RenderSystem::hasSceneDirectionalLight() const { return !renderer_.scene.directionalLights.empty(); }
	/// @brief Reports whether the renderer stores any point light.
	inline bool RenderSystem::hasScenePointLight() const { return !renderer_.scene.pointLights.empty(); }
	/// @brief Reports whether the renderer stores any spot light.
	inline bool RenderSystem::hasSceneSpotLight() const { return !renderer_.scene.spotLights.empty(); }

	/// @brief Requires an initialized renderer and counts only presented frames; skipped frames remain successful.
	inline auto RenderSystem::renderFrame(WindowSystem &windows)											-> std::expected<void, Error>{
		last_window_count_ = 0U;
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		// Retire closed targets only after their submitted work and presentation have finished.
		for (auto target = renderer_.targets.begin(); target != renderer_.targets.end();) {
			const auto *window = windows.findWindow(target->handle);
			if (!window || window->info().should_close) {
				const VkResult result = renderer_.waitIdle();
				if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
				target->cleanup();
				target = renderer_.targets.erase(target);
				continue;
			}
			// Each target uploads its camera uniforms and reuses the ordered shared shadow arrays.
			// The renderer checks the live drawable extent and skips zero-size windows.
			if (!window->info().minimized) {
				const bool presented = renderer_.renderFrame(*target);
				scene_.releaseUploadedPixels([this](std::size_t index) { return renderer_.uploadedTextureGeneration(index); });
				if (const auto error = renderer_.textureUploadError()) { return std::unexpected(*error); }
				if (presented) { ++last_window_count_; }
			}
			++target;
		}
		if (last_window_count_ == 0U) { return {}; }
		++rendered_frames_;
		const auto now = std::chrono::steady_clock::now();
		if (render_fps_start_ == std::chrono::steady_clock::time_point{}) { render_fps_start_ = now; }
		++render_fps_frames_;
		const std::chrono::duration<double> elapsed = now - render_fps_start_;
		if (elapsed.count() >= 0.25) {
			render_fps_ = static_cast<double>(render_fps_frames_) / elapsed.count();
			render_fps_frames_ = 0;
			render_fps_start_ = now;
		}
		return {};
	}

	inline std::uint64_t RenderSystem::renderedFrameCount() const { return rendered_frames_; }
	inline double RenderSystem::renderingFramesPerSecond() const { return render_fps_; }
	inline std::size_t RenderSystem::lastRenderedWindowCount() const { return last_window_count_; }

} // namespace vve::simple
