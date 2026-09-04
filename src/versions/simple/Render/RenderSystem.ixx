module;
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

export module VEEngine.Simple:RenderSystem;
import std;
export import :Types;
import :Window;
import VEEngine.Simple.Vulkan;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;
import :RenderSystemScene;
import :RenderSystemObjects;
export import :RenderResources;

/// @file
/// @brief Simple render coordinator: renderer backend ownership and scene mirroring.

export namespace vve::simple {

	/// @brief Non-owning read callbacks for imported asset scenes owned by the engine.
	struct ImportedAssetReadAccess {
		std::function<std::expected<Vector<NodeHandle>, Error>(SceneHandle)> scene_nodes{};						///< Lists scene nodes.
		std::function<std::expected<NodeHandle, Error>(SceneHandle)> scene_root_node{};							///< Returns the root node.
		std::function<std::expected<Vector<NodeHandle>, Error>(SceneHandle, NodeHandle)> scene_node_children{};	///< Lists child nodes.
		std::function<std::expected<Transform, Error>(NodeHandle)> node_transform{};								///< Returns local transform.
		std::function<std::expected<Vector<MeshHandle>, Error>(NodeHandle)> node_meshes{};						///< Lists meshes attached to a node.
		std::function<std::expected<MaterialHandle, Error>(MeshHandle)> mesh_material{};							///< Returns the mesh material.
		std::function<std::expected<Vector<TextureHandle>, Error>(MaterialHandle)> material_textures{};		///< Lists material textures.
		std::function<std::expected<Vector<LightHandle>, Error>(SceneHandle)> scene_lights{};					///< Lists scene lights.
		std::function<std::expected<LightDescriptor, Error>(LightHandle)> light_data{};							///< Returns imported light data.
		std::function<std::expected<Vector<CameraHandle>, Error>(SceneHandle)> scene_cameras{};				///< Lists scene cameras.
		std::function<std::expected<CameraDescriptor, Error>(CameraHandle)> camera_data{};						///< Returns imported camera data.
		std::function<std::expected<Vector<Vec3>, Error>(MeshHandle)> mesh_positions{};							///< Returns mesh positions.
		std::function<std::expected<Vector<Vec3>, Error>(MeshHandle)> mesh_normals{};								///< Returns mesh normals.
		std::function<std::expected<Vector<Vec2>, Error>(MeshHandle)> mesh_texcoords{};							///< Returns mesh texture coordinates.
		std::function<std::expected<Vector<std::uint32_t>, Error>(MeshHandle)> mesh_indices{};					///< Returns mesh indices.
	};


	/// @brief simple render facade coordinating the renderer backend and CPU render scene.
	class RenderSystem : public RenderSystemScene<RenderSystem>, public RenderSystemObjects<RenderSystem> {
	public:
		RenderSystem() = default;
		explicit RenderSystem(ImportedAssetReadAccess imported_assets);
		[[nodiscard]] auto instantiateScene(SceneHandle scene, SceneInstantiationOptions options = {})	-> std::expected<RenderSceneInstanceHandle, Error>;
		auto waitIdle() -> void;
		/// @brief Stores the borrowed GUI system for later forwarding to renderer backends.
		auto setGuiSystem(void *gui)																								-> void;
		auto setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)												-> void;
		[[nodiscard]] auto initialize(SDL_Window *window, RendererId id = {})												-> std::expected<void, Error>;
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
		[[nodiscard]] auto hasSceneCamera() const																					-> bool;
		[[nodiscard]] auto hasSceneDirectionalLight() const																	-> bool;
		[[nodiscard]] auto hasScenePointLight() const																			-> bool;
		[[nodiscard]] auto hasSceneSpotLight() const																				-> bool;
		[[nodiscard]] auto renderFrame(const WindowFrameData &windows)														-> std::expected<void, Error>;
		[[nodiscard]] auto renderFrame(WindowSystem &windows)																	-> std::expected<void, Error>;
		[[nodiscard]] auto renderedFrameCount() const																			-> std::uint64_t;
		[[nodiscard]] auto renderingFramesPerSecond() const																-> double;
		[[nodiscard]] auto lastRenderedWindowCount() const																		-> std::size_t;

	private:
		template<typename>
		friend struct RenderSystemScene;
		template<typename>
		friend struct RenderSystemObjects;

		[[nodiscard]] auto registerRenderObject(RenderInstanceHandle instance, std::size_t backend_index)	-> RenderObjectHandle;
		[[nodiscard]] auto findRenderObject(RenderObjectHandle handle) const
			-> std::optional<std::pair<RenderInstanceHandle, std::size_t>>;
		auto eraseRenderObject(RenderObjectHandle handle)														-> void;
		[[nodiscard]] auto importedSceneNodes(SceneHandle scene) const									-> Vector<NodeHandle>;
		[[nodiscard]] auto importedSceneWorldTransforms(SceneHandle scene) const
			-> Vector<std::tuple<NodeHandle, Transform, Mat4>>;
		[[nodiscard]] auto importedSceneMeshInstances(SceneHandle scene) const
			-> Vector<std::tuple<NodeHandle, MeshHandle, MaterialHandle, Transform, Mat4>>;
		[[nodiscard]] auto importedMeshGeometry(MeshHandle mesh) const
			-> std::optional<std::tuple<Vector<Vec3>, Vector<Vec3>, Vector<Vec2>, Vector<std::uint32_t>>>;
		[[nodiscard]] auto acquireRenderMesh(MeshHandle imported_mesh)									-> std::optional<RenderMeshHandle>;
		[[nodiscard]] auto acquireRenderMaterial(MaterialHandle imported_material)						-> RenderMaterialHandle;
		[[nodiscard]] auto importedMaterialTextures(MaterialHandle material) const					-> std::optional<Vector<TextureHandle>>;

		RenderScene scene_{};															///< Active CPU render scene.
		ForwardRenderer renderer_{};													///< Forward renderer backend.
		ImportedAssetReadAccess imported_assets_{};								///< Borrowed asset-scene queries.
		void *guiSystem_{nullptr};													///< Non-owning, type-erased GUI system pointer for later renderer wiring.
		std::unordered_map<MeshHandle, RenderMeshHandle, HandleHash<MeshHandle>> imported_render_meshes_{};	///< Imported mesh cache.
		std::unordered_map<MaterialHandle, RenderMaterialHandle, HandleHash<MaterialHandle>> imported_render_materials_{};	///< Imported material cache.
		std::unordered_map<RenderObjectHandle, std::pair<RenderInstanceHandle, std::size_t>, HandleHash<RenderObjectHandle>>
			render_objects_{};														///< Public render-object to internal instance map.
		std::unordered_map<RenderObjectHandle, std::pair<RenderSceneInstanceHandle, NodeHandle>, HandleHash<RenderObjectHandle>>
			object_sources_{};														///< Public render-object source scene and node map.
		std::map<SceneHandle, Scene> scenes_{};									///< Loaded backend scenes by public scene handle.
		std::map<RenderSceneInstanceHandle, Vector<RenderObjectHandle>> scene_instances_{};	///< Render objects created per scene instance.
		std::map<RenderSceneInstanceHandle, SceneHandle> scene_instance_sources_{};	///< Asset scene used to create each scene instance.
		std::optional<SceneHandle> active_scene_{};								///< Scene currently mirrored into the backend.
		std::uint64_t next_render_object_id_{1};								///< Next public render-object id.
		std::uint64_t next_scene_instance_id_{1};								///< Next public scene-instance id.
		std::uint64_t rendered_frames_{0};											///< Number of accepted frame calls.
		std::uint64_t render_fps_frames_{0};										///< Frames accumulated for the render-FPS sample.
		std::chrono::steady_clock::time_point render_fps_start_{};		///< Start of the current render-FPS sample window.
		double render_fps_{};															///< Last measured render-frame throughput.
		std::size_t last_window_count_{0};											///< Last non-closed window count.
		bool initialized_{false};														///< True after the concrete renderer is initialized.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Stores read access to imported asset-scene descriptors owned by the engine.
	inline RenderSystem::RenderSystem(ImportedAssetReadAccess imported_assets)
		: imported_assets_{std::move(imported_assets)} {}

	/// @brief Returns the forward renderer backend.
	inline auto RenderSystem::forward()																			-> ForwardRenderer &{ return renderer_; }

	/// @brief Returns the forward renderer backend.
	inline auto RenderSystem::forward() const																	-> const ForwardRenderer &{ return renderer_; }

	/// @brief Mints a public render-object handle for one internal scene instance.
	inline auto RenderSystem::registerRenderObject(RenderInstanceHandle instance, std::size_t backend_index)
		-> RenderObjectHandle{
		const auto handle = RenderObjectHandle{RenderObjectHandle::counter_bit |
														  (next_render_object_id_++ & RenderObjectHandle::id_mask)};
		render_objects_.emplace(handle, std::pair{instance, backend_index});
		return handle;
	}

	/// @brief Looks up the internal instance behind a public render-object handle.
	inline auto RenderSystem::findRenderObject(RenderObjectHandle handle) const
		-> std::optional<std::pair<RenderInstanceHandle, std::size_t>>{
		const auto found = render_objects_.find(handle);
		return found == render_objects_.end() ? std::nullopt :
														 std::optional<std::pair<RenderInstanceHandle, std::size_t>>{found->second};
	}

	/// @brief Removes one public render-object mapping.
	inline auto RenderSystem::eraseRenderObject(RenderObjectHandle handle)								-> void{
		render_objects_.erase(handle);
	}

	inline auto RenderSystem::setGuiSystem(void *gui)																-> void{
		guiSystem_ = gui;
	}

	/// @brief Forwards the GUI recorder into the active forward renderer.
	inline auto RenderSystem::setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)			-> void{
		renderer_.setGuiRecordSink(std::move(sink));
	}

	inline auto RenderSystem::initialize(SDL_Window *window, RendererId id)									-> std::expected<void, Error>{
		if (initialized_) { return {}; }
		if (window == nullptr) { return std::unexpected(Error::invalid_argument); }
		if (id.value != "forward" && !id.value.empty()) { return std::unexpected(Error::invalid_argument); }
		renderer_.setGuiSystem(guiSystem_);
		const VkResult result = renderer_.init(window);
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
		initialized_ = true;
		return {};
	}

	inline auto RenderSystem::makeGuiInitInfo() const													-> std::optional<ImGui_ImplVulkan_InitInfo>{
		if (!initialized_) { return std::nullopt; }
		auto info = renderer_.makeImguiInitInfo();
		info.RenderPass = VK_NULL_HANDLE;
		info.UseDynamicRendering = true;
		info.PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1U,
			.pColorAttachmentFormats = &renderer_.swapchain.imageFormat,
			.depthAttachmentFormat = depthFormat,	///< GUI records inside the forward color pass, which binds the depth image.
		};
		if (info.Device == VK_NULL_HANDLE || info.DescriptorPool == VK_NULL_HANDLE) {
			return std::nullopt;
		}
		return info;
	}

	/// @brief Waits for renderer-owned Vulkan work before dependent resources are destroyed.
	inline auto RenderSystem::waitIdle() -> void {
		if (!initialized_) { return; }
		if (renderer_.device.device != VK_NULL_HANDLE) { (void)vkDeviceWaitIdle(renderer_.device.device); }
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

	inline std::size_t RenderSystem::sceneMeshCount() const { return scene_.meshCount(); }
	inline std::size_t RenderSystem::sceneMaterialCount() const { return scene_.materialCount(); }
	/// @brief Returns the number of active directional lights.
	inline std::size_t RenderSystem::sceneDirectionalLightCount() const { return scene_.directionalLights().size(); }
	/// @brief Returns the number of active point lights.
	inline std::size_t RenderSystem::scenePointLightCount() const { return scene_.pointLights().size(); }
	/// @brief Returns the number of active spot lights.
	inline std::size_t RenderSystem::sceneSpotLightCount() const { return scene_.spotLights().size(); }
	/// @brief Returns the number of imported cameras applied to the render scene.
	inline std::size_t RenderSystem::sceneCameraCount() const { return scene_.importedCameras().size(); }
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
		return renderer_.captureFrameToPng(output_path);
	}
	inline bool RenderSystem::hasSceneCamera() const { return scene_.camera().has_value(); }
	inline bool RenderSystem::hasSceneDirectionalLight() const { return scene_.directionalLight().has_value(); }
	inline bool RenderSystem::hasScenePointLight() const { return scene_.pointLight().has_value(); }
	inline bool RenderSystem::hasSceneSpotLight() const { return scene_.spotLight().has_value(); }

	/// @brief Records a frame without creating GPU objects.
	inline auto RenderSystem::renderFrame(const WindowFrameData &windows)								-> std::expected<void, Error>{
		last_window_count_ = std::ranges::count_if(windows.windows, [](const WindowInfo &window) {
			return !window.should_close;
		});
		if (initialized_) {
			renderer_.renderFrame(nullptr);
			++rendered_frames_;
		} else {
			++rendered_frames_;
		}
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

	/// @brief Records a frame using the current window snapshot.
	inline auto RenderSystem::renderFrame(WindowSystem &windows)											-> std::expected<void, Error>{
		return renderFrame(WindowFrameData{.windows = windows.snapshot()});
	}

	inline std::uint64_t RenderSystem::renderedFrameCount() const { return rendered_frames_; }
	inline double RenderSystem::renderingFramesPerSecond() const { return render_fps_; }
	inline std::size_t RenderSystem::lastRenderedWindowCount() const { return last_window_count_; }

} // namespace vve::simple
