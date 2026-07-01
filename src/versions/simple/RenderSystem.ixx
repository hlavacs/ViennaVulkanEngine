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
export import :RenderPass;
import :Window;
import VEEngine.Simple.Vulkan;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;

/// @file
/// @brief Compact simple render-resource model with no concrete renderer backend.

namespace vve::simple::detail {

	/// @brief Orders string views for deterministic graph node lookup.
	struct StringViewLess {
		[[nodiscard]] inline bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
			const auto count = std::min(lhs.size(), rhs.size());
			for (std::size_t index{}; index < count; ++index) {
				const auto left = static_cast<unsigned char>(lhs[index]);
				const auto right = static_cast<unsigned char>(rhs[index]);
				if (left != right) { return left < right; }
			}
			return lhs.size() < rhs.size();
		}
	};

	using RenderPassHandleMap = std::map<std::string_view, RenderPassHandle, StringViewLess>;	///< Pass-name map.

} // namespace vve::simple::detail

export namespace vve::simple {

	struct RenderMeshHandleTag {};																				///< simple render mesh handle tag.
	struct RenderMaterialHandleTag {};																			///< simple render material handle tag.
	struct RenderInstanceHandleTag {};																			///< simple render instance handle tag.
	struct RenderResourceHandleTag {};																			///< simple render resource handle tag.
	struct RenderFunctionHandleTag {};																			///< simple render function handle tag.

	using RenderMeshHandle	= TypedHandle<RenderMeshHandleTag>;										///< simple render mesh handle.
	using RenderMaterialHandle = TypedHandle<RenderMaterialHandleTag>;								///< simple render material handle.
	using RenderInstanceHandle = TypedHandle<RenderInstanceHandleTag>;								///< simple render instance handle.
	using RenderResourceHandle = TypedHandle<RenderResourceHandleTag>;								///< simple render resource handle.
	using RenderFunctionHandle = TypedHandle<RenderFunctionHandleTag>;								///< simple render function handle.

	/// @brief Coarse render resource categories used by the stub renderer registry.
	enum class RenderResourceKind : std::uint8_t {
		mesh,																					///< Geometry source.
		material,																			///< Surface state.
		texture,																				///< Sampled image data.
		camera,																				///< View/projection source.
		light,																				///< Light source data.
		target																				///< Render output target.
	};

	/// @brief Vertex payload stored by the CPU-side scene registry.
	struct RenderVertex {
		Vec3 position{zeroVec3()};														///< Object-space position.
		Vec3 normal{Vec3(zero(), one(), zero())};									///< Object-space normal.
		Vec2 uv{zero(), zero()};														///< First texture coordinate.
	};

	/// @brief CPU-side mesh resource.
	struct RenderMesh {
		RenderMeshHandle handle{};														///< Stable render mesh handle.
		Vector<RenderVertex> vertices{};												///< Source vertices.
		Vector<std::uint32_t> indices{};												///< Triangle indices.
		Bounds bounds{};																	///< Object-space bounds.
	};

	/// @brief CPU-side material resource.
	struct RenderMaterial {
		RenderMaterialHandle handle{};												///< Stable render material handle.
		LinearColor base_color{.value = oneVec3()};								///< Base color factor.
		TextureHandle base_color_texture{};											///< Optional texture handle.
		std::filesystem::path base_color_texture_source{};						///< Optional source path for diagnostics.
	};

	/// @brief Scene draw item connecting mesh, material, and transforms.
	struct RenderInstance {
		RenderInstanceHandle handle{};												///< Stable render instance handle.
		RenderMeshHandle mesh{};														///< Mesh drawn by this instance.
		RenderMaterialHandle material{};												///< Material used by this instance.
		Transform local_transform{};													///< Source scene local transform.
		Mat4 world_transform{identityMat4()};										///< World transform used by later renderers.
	};

	/// @brief Directional light resource data.
	struct RenderDirectionalLight {
		Direction direction_to_light{.value = Vec3(-0.5F, 1.0F, 0.25F)};	///< Direction from surface to light.
		LinearColor color{.value = oneVec3()};										///< Direct light color.
		LightIntensity intensity{.value = one()};									///< Direct light intensity.
		LinearColor ambient{.value = Vec3(0.04F, 0.04F, 0.04F)};				///< Small ambient term.
		Mat4 light_view_projection{identityMat4()};								///< Optional light-space transform.
	};

	/// @brief Point light resource data.
	struct RenderPointLight {
		Position position{};																///< Light position in world space.
		LinearColor color{.value = oneVec3()};										///< Direct light color.
		LightIntensity intensity{.value = zero()};								///< Direct light intensity.
		LightRange range{.value = static_cast<Scalar>(1)};						///< Influence radius.
		LinearColor ambient{.value = Vec3(0.04F, 0.04F, 0.04F)};				///< Small ambient term.
	};

	/// @brief Spot light resource data.
	struct RenderSpotLight {
		Position position{};																///< Light position in world space.
		Direction direction{.value = Vec3(zero(), -one(), zero())};			///< Direction from light to scene.
		LinearColor color{.value = oneVec3()};										///< Direct light color.
		LightIntensity intensity{.value = zero()};								///< Direct light intensity.
		LightRange range{.value = static_cast<Scalar>(1)};						///< Influence radius.
		LinearColor ambient{.value = Vec3(0.04F, 0.04F, 0.04F)};				///< Small ambient term.
		SpotConeAngle cone{};															///< Outer cone angle.
	};

	/// @brief Camera resource data consumed by future render functions.
	struct RenderCamera {
		Camera camera{};																	///< Facade camera description.
		PixelExtent target_extent{.width = 1, .height = 1};					///< Target size for projection choices.
	};

	/// @brief Generic render resource descriptor for simple design experiments.
	struct RenderResource {
		RenderResourceHandle handle{};												///< Stable resource handle.
		RenderResourceKind kind{};														///< Coarse resource category.
		ObjectName name{};																///< Human-readable resource name.
	};

	/// @brief Generic render function descriptor with explicit resource dependencies.
	struct RenderFunction {
		RenderFunctionHandle handle{};												///< Stable function handle.
		ObjectName name{};																///< Human-readable function name.
		Vector<RenderResourceHandle> reads{};										///< Resources read by this function.
		Vector<RenderResourceHandle> writes{};										///< Resources written by this function.
	};

	/// @brief Minimal CPU scene that stores render resources but does not draw them.
	class RenderScene {
	public:
		[[nodiscard]] RenderMaterialHandle addMaterial(RenderMaterial material = {});
		[[nodiscard]] RenderMeshHandle addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
															Bounds bounds = {});
		[[nodiscard]] auto addPlaneMesh(Vec2 half_extent)																		-> RenderMeshHandle;
		[[nodiscard]] auto addCuboidMesh(Vec3 minimum, Vec3 maximum)														-> RenderMeshHandle;
		[[nodiscard]] std::expected<RenderInstanceHandle, Error>
		addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local = {},
						Mat4 world = identityMat4());
		auto setCamera(RenderCamera camera)																							-> void;
		auto setDirectionalLight(RenderDirectionalLight light)																-> void;
		auto setPointLight(RenderPointLight light)																				-> void;
		auto setSpotLight(RenderSpotLight light)																					-> void;
		auto addSpotLight(RenderSpotLight light)																					-> void;
		auto clear()																														-> void;
		[[nodiscard]] const RenderMesh *findMesh(RenderMeshHandle handle) const;
		[[nodiscard]] const RenderMaterial *findMaterial(RenderMaterialHandle handle) const;
		[[nodiscard]] const RenderInstance *findInstance(RenderInstanceHandle handle) const;
		[[nodiscard]] auto meshCount() const																						-> std::size_t;
		[[nodiscard]] auto materialCount() const																					-> std::size_t;
		[[nodiscard]] auto instanceCount() const																					-> std::size_t;
		[[nodiscard]] auto vertexCount() const																						-> std::size_t;
		[[nodiscard]] auto indexCount() const																						-> std::size_t;
		[[nodiscard]] const std::optional<RenderCamera> &camera() const;
		[[nodiscard]] const std::optional<RenderDirectionalLight> &directionalLight() const;
		[[nodiscard]] const std::optional<RenderPointLight> &pointLight() const;
		[[nodiscard]] std::optional<RenderSpotLight> spotLight() const;
		[[nodiscard]] const std::vector<RenderSpotLight> &spotLights() const;
		[[nodiscard]] const Vector<RenderInstance> &instances() const;

	private:
		static void appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
										Vec3 normal, std::array<Vec3, 4> corners);

		Vector<RenderMesh> meshes_{};													///< CPU mesh resources.
		Vector<RenderMaterial> materials_{};										///< CPU material resources.
		Vector<RenderInstance> instances_{};										///< CPU draw items.
		std::optional<RenderCamera> camera_{};										///< Optional active camera.
		std::optional<RenderDirectionalLight> light_{};							///< Optional active directional light.
		std::optional<RenderPointLight> point_light_{};							///< Optional active point light.
		std::vector<RenderSpotLight> spot_lights_{};							///< Capped active spot lights.
	};

	/// @brief Renderer descriptor used only for graph construction in the simple stub.
	struct RendererDescriptor {
		using HandleType = RendererHandle;											///< Descriptor handle type.
		RendererHandle handle{};														///< Stable renderer descriptor handle.
		RendererId id{.value = "stub"};												///< Renderer id chosen by the application.
		bool shadow_maps{};																///< Stubs do not create shadow maps.
		std::span<const RenderPassContract> passes{};							///< Stub graph nodes.
	};

	/// @brief simple render facade storing resources and frame counters without GPU work.
	class RenderSystem {
	public:
		[[nodiscard]] auto createRenderer(RendererId id) const																-> std::expected<RendererDescriptor, Error>;
		[[nodiscard]] auto buildRenderGraph(const RendererDescriptor &renderer) const									-> std::expected<RenderGraph, Error>;
		[[nodiscard]] auto buildRenderGraph(std::span<const RenderPassContract> passes) const						-> std::expected<RenderGraph, Error>;
		[[nodiscard]] std::expected<RenderGraph, Error>
		buildRenderGraph(std::span<const std::span<const RenderPassContract>> pass_lists) const;
		[[nodiscard]] std::expected<RenderResourceHandle, Error>
		createResource(RenderResourceKind kind, ObjectName name = {});
		[[nodiscard]] std::expected<RenderFunctionHandle, Error>
		createFunction(ObjectName name, Vector<RenderResourceHandle> reads = {}, Vector<RenderResourceHandle> writes = {});
		[[nodiscard]] auto resourceCount() const																					-> std::size_t;
		[[nodiscard]] auto functionCount() const																					-> std::size_t;
		[[nodiscard]] auto resourceName(RenderResourceHandle handle) const												-> std::expected<ObjectName, Error>;
		[[nodiscard]] auto resourceKind(RenderResourceHandle handle) const												-> std::expected<RenderResourceKind, Error>;
		[[nodiscard]] auto functionName(RenderFunctionHandle handle) const												-> std::expected<ObjectName, Error>;
		[[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
																			Transform transform = {});
		[[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																			Transform transform = {});
		[[nodiscard]] std::expected<void, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																					 std::filesystem::path base_color_texture,
																					 Transform transform = {});
		auto clearScene()																													-> void;
		auto setCamera(Camera camera, PixelExtent extent)																		-> void;
		void setDirectionalLight(Direction direction_to_light, LinearColor color,
											LightIntensity intensity, LinearColor ambient);
		auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
		auto setPointLight(Position position, LinearColor color,
									LightIntensity intensity, LightRange range, LinearColor ambient)			-> void;
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone);
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone);
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
		auto loadScene(Scene scene)																									-> void;
		/// @brief Stores the borrowed GUI system for later forwarding to renderer backends.
		auto setGuiSystem(void *gui)																								-> void;
		auto setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)												-> void;
		[[nodiscard]] auto initialize(SDL_Window *window, RendererId id = {})												-> std::expected<void, Error>;
		[[nodiscard]] auto makeGuiInitInfo() const																			-> std::optional<ImGui_ImplVulkan_InitInfo>;
		[[nodiscard]] auto backend()																								-> SelectedRenderer &;
		[[nodiscard]] auto backend() const																						-> const SelectedRenderer &;
		auto shutdown()																													-> void;
		[[nodiscard]] auto initialized() const																					-> bool;
		[[nodiscard]] auto sceneMeshCount() const																					-> std::size_t;
		[[nodiscard]] auto sceneMaterialCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneInstanceCount() const																			-> std::size_t;
		[[nodiscard]] auto sceneVertexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneIndexCount() const																				-> std::size_t;
		[[nodiscard]] auto sceneShadowDepthSampleCount() const														-> std::size_t;
		[[nodiscard]] auto sceneShadowDepthSample(std::size_t index) const									-> std::optional<RenderShadowDepthSample>;
		[[nodiscard]] auto sceneSpotShadowDepthSampleCount() const													-> std::size_t;
		[[nodiscard]] auto sceneSpotShadowDepthSample(std::size_t index) const								-> std::optional<RenderShadowDepthSample>;
		[[nodiscard]] auto sceneSpotShadowDepthGpuDepth(std::size_t index) const						-> std::optional<float>;
		[[nodiscard]] auto sceneSpotShadowDepthHasGpu(std::size_t index) const							-> std::optional<bool>;
		[[nodiscard]] auto sceneSpotShadowDepthError(std::size_t index) const							-> std::optional<float>;
		[[nodiscard]] auto hasSceneCamera() const																					-> bool;
		[[nodiscard]] auto hasSceneDirectionalLight() const																	-> bool;
		[[nodiscard]] auto hasScenePointLight() const																			-> bool;
		[[nodiscard]] auto hasSceneSpotLight() const																				-> bool;
		[[nodiscard]] auto captureFrameToPng(const std::filesystem::path &output_path)						-> std::expected<void, Error>;
		[[nodiscard]] auto renderFrame(const WindowFrameData &windows)														-> std::expected<void, Error>;
		[[nodiscard]] auto renderFrame(WindowSystem &windows)																	-> std::expected<void, Error>;
		[[nodiscard]] auto renderedFrameCount() const																			-> std::uint64_t;
		[[nodiscard]] auto lastRenderedWindowCount() const																		-> std::size_t;

	private:
		[[nodiscard]] static std::expected<void, Error>
		addPass(RenderGraph &graph, detail::RenderPassHandleMap &handles, const RenderPassContract &pass);
		[[nodiscard]] static std::expected<void, Error>
		addDependencies(RenderGraph &graph, const detail::RenderPassHandleMap &handles,
								std::span<const RenderPassContract> passes);
		[[nodiscard]] const RenderResource *find(RenderResourceHandle handle) const;
		[[nodiscard]] const RenderFunction *find(RenderFunctionHandle handle) const;
		[[nodiscard]] auto appendBackendObject(RenderInstanceHandle instance)										-> std::expected<void, Error>;
		[[nodiscard]] auto forward()																					-> ForwardRenderer &;
		[[nodiscard]] auto forward() const																				-> const ForwardRenderer &;

		RenderScene scene_{};															///< Active CPU render scene.
		SelectedRenderer renderer_{};													///< Selected renderer backend.
		void *guiSystem_{nullptr};													///< Non-owning, type-erased GUI system pointer for later renderer wiring.
		Vector<RenderResource> resources_{};										///< Generic render resource registry.
		Vector<RenderFunction> functions_{};										///< Generic render function registry.
		std::uint64_t rendered_frames_{0};											///< Number of accepted frame calls.
		std::size_t last_window_count_{0};											///< Last non-closed window count.
		bool initialized_{false};														///< True after the concrete renderer is initialized.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Adds one quad face to a CPU mesh.
	inline void RenderScene::appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
													Vec3 normal, std::array<Vec3, 4> corners) {
		const auto base = static_cast<std::uint32_t>(vertices.size());
		const auto uvs = std::array{Vec2{0.0F, 0.0F}, Vec2{1.0F, 0.0F}, Vec2{1.0F, 1.0F}, Vec2{0.0F, 1.0F}};
		for (std::size_t corner_index{}; corner_index < corners.size(); ++corner_index) {
			vertices.push_back(RenderVertex{.position = corners[corner_index], .normal = normal, .uv = uvs[corner_index]});
		}
		for (const auto index : std::array{0U, 1U, 2U, 0U, 2U, 3U}) { indices.push_back(base + index); }
	}

	/// @brief Registers a material and returns its handle.
	inline auto RenderScene::addMaterial(RenderMaterial material)											-> RenderMaterialHandle{
		material.handle = material.handle.valid() ? material.handle : makeCounterHandle<RenderMaterialHandle>();
		materials_.push_back(std::move(material));
		return materials_.back().handle;
	}

	/// @brief Registers a mesh and returns its handle.
	inline RenderMeshHandle RenderScene::addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
																Bounds bounds) {
		auto mesh = RenderMesh{.handle = makeCounterHandle<RenderMeshHandle>(),
										.vertices = std::move(vertices),
										.indices = std::move(indices),
										.bounds = bounds};
		meshes_.push_back(std::move(mesh));
		return meshes_.back().handle;
	}

	/// @brief Creates a two-triangle plane mesh.
	inline auto RenderScene::addPlaneMesh(Vec2 half_extent)													-> RenderMeshHandle{
		auto vertices = Vector<RenderVertex>{};
		auto indices = Vector<std::uint32_t>{};
		appendFace(vertices, indices, Vec3{0.0F, 1.0F, 0.0F},
						{Vec3{-half_extent.x, 0.0F, -half_extent.y}, Vec3{-half_extent.x, 0.0F, half_extent.y},
						Vec3{half_extent.x, 0.0F, half_extent.y}, Vec3{half_extent.x, 0.0F, -half_extent.y}});
		const auto bounds = Bounds{.minimum = Position{.value = Vec3{-half_extent.x, 0.0F, -half_extent.y}},
											.maximum = Position{.value = Vec3{half_extent.x, 0.0F, half_extent.y}},
											.valid = true};
		return addMesh(std::move(vertices), std::move(indices), bounds);
	}

	/// @brief Creates a six-face cuboid mesh.
	inline auto RenderScene::addCuboidMesh(Vec3 minimum, Vec3 maximum)									-> RenderMeshHandle{
		auto vertices = Vector<RenderVertex>{};
		auto indices = Vector<std::uint32_t>{};
		appendFace(vertices, indices, Vec3{0.0F, 0.0F, 1.0F},
						{Vec3{minimum.x, minimum.y, maximum.z}, Vec3{maximum.x, minimum.y, maximum.z},
						Vec3{maximum.x, maximum.y, maximum.z}, Vec3{minimum.x, maximum.y, maximum.z}});
		appendFace(vertices, indices, Vec3{0.0F, 0.0F, -1.0F},
						{Vec3{maximum.x, minimum.y, minimum.z}, Vec3{minimum.x, minimum.y, minimum.z},
						Vec3{minimum.x, maximum.y, minimum.z}, Vec3{maximum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{1.0F, 0.0F, 0.0F},
						{Vec3{maximum.x, minimum.y, maximum.z}, Vec3{maximum.x, minimum.y, minimum.z},
						Vec3{maximum.x, maximum.y, minimum.z}, Vec3{maximum.x, maximum.y, maximum.z}});
		appendFace(vertices, indices, Vec3{-1.0F, 0.0F, 0.0F},
						{Vec3{minimum.x, minimum.y, minimum.z}, Vec3{minimum.x, minimum.y, maximum.z},
						Vec3{minimum.x, maximum.y, maximum.z}, Vec3{minimum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{0.0F, 1.0F, 0.0F},
						{Vec3{minimum.x, maximum.y, maximum.z}, Vec3{maximum.x, maximum.y, maximum.z},
						Vec3{maximum.x, maximum.y, minimum.z}, Vec3{minimum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{0.0F, -1.0F, 0.0F},
						{Vec3{minimum.x, minimum.y, minimum.z}, Vec3{maximum.x, minimum.y, minimum.z},
						Vec3{maximum.x, minimum.y, maximum.z}, Vec3{minimum.x, minimum.y, maximum.z}});
		return addMesh(std::move(vertices), std::move(indices),
							Bounds{.minimum = Position{.value = minimum}, .maximum = Position{.value = maximum}, .valid = true});
	}

	/// @brief Registers an instance when its mesh and material exist.
	inline std::expected<RenderInstanceHandle, Error>
	RenderScene::addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local, Mat4 world) {
		if (findMesh(mesh) == nullptr || findMaterial(material) == nullptr) { return std::unexpected(Error::missing_object); }
		auto instance = RenderInstance{.handle = makeCounterHandle<RenderInstanceHandle>(),
													.mesh = mesh, .material = material,
													.local_transform = local, .world_transform = world};
		instances_.push_back(std::move(instance));
		return instances_.back().handle;
	}

	/// @brief Stores the active camera resource.
	inline void RenderScene::setCamera(RenderCamera camera) { camera_ = std::move(camera); }

	/// @brief Stores the active directional light resource.
	inline void RenderScene::setDirectionalLight(RenderDirectionalLight light) { light_ = std::move(light); }

	/// @brief Stores the active point light resource.
	inline void RenderScene::setPointLight(RenderPointLight light) { point_light_ = std::move(light); }

	/// @brief Replaces the active spot-light list with one first-light entry.
	inline void RenderScene::setSpotLight(RenderSpotLight light) {
		spot_lights_.clear();
		spot_lights_.push_back(std::move(light));
	}

	/// @brief Adds one spot light until the fixed simple-engine cap is reached.
	inline void RenderScene::addSpotLight(RenderSpotLight light) {
		if (spot_lights_.size() < kMaxShadowedSpotLights) { spot_lights_.push_back(std::move(light)); }
	}

	/// @brief Removes all scene resources.
	inline auto RenderScene::clear()																					-> void{
		meshes_.clear();
		materials_.clear();
		instances_.clear();
		camera_.reset();
		light_.reset();
		point_light_.reset();
		spot_lights_.clear();
	}

	/// @brief Finds a mesh by handle.
	inline const RenderMesh *RenderScene::findMesh(RenderMeshHandle handle) const {
		const auto found = std::ranges::find(meshes_, handle, &RenderMesh::handle);
		return found == meshes_.end() ? nullptr : std::addressof(*found);
	}

	/// @brief Finds a material by handle.
	inline const RenderMaterial *RenderScene::findMaterial(RenderMaterialHandle handle) const {
		const auto found = std::ranges::find(materials_, handle, &RenderMaterial::handle);
		return found == materials_.end() ? nullptr : std::addressof(*found);
	}

	/// @brief Finds an instance by handle.
	inline const RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) const {
		const auto found = std::ranges::find(instances_, handle, &RenderInstance::handle);
		return found == instances_.end() ? nullptr : std::addressof(*found);
	}

	inline std::size_t RenderScene::meshCount() const { return meshes_.size(); }
	inline std::size_t RenderScene::materialCount() const { return materials_.size(); }
	inline std::size_t RenderScene::instanceCount() const { return instances_.size(); }

	/// @brief Returns the total source vertex count.
	inline auto RenderScene::vertexCount() const																	-> std::size_t{
		return std::accumulate(meshes_.begin(), meshes_.end(), std::size_t{}, [](std::size_t total, const auto &mesh) {
			return total + mesh.vertices.size();
		});
	}

	/// @brief Returns the total source index count.
	inline auto RenderScene::indexCount() const																	-> std::size_t{
		return std::accumulate(meshes_.begin(), meshes_.end(), std::size_t{}, [](std::size_t total, const auto &mesh) {
			return total + mesh.indices.size();
		});
	}

	inline const std::optional<RenderCamera> &RenderScene::camera() const { return camera_; }
	inline const std::optional<RenderDirectionalLight> &RenderScene::directionalLight() const { return light_; }
	inline const std::optional<RenderPointLight> &RenderScene::pointLight() const { return point_light_; }
	inline std::optional<RenderSpotLight> RenderScene::spotLight() const {
		return spot_lights_.empty() ? std::nullopt : std::optional<RenderSpotLight>{spot_lights_.front()};
	}
	inline const std::vector<RenderSpotLight> &RenderScene::spotLights() const { return spot_lights_; }
	inline const Vector<RenderInstance> &RenderScene::instances() const { return instances_; }

	/// @brief Accepts the historical forward id and the new stub id.
	inline auto RenderSystem::createRenderer(RendererId id) const											-> std::expected<RendererDescriptor, Error>{
		if (id.value == "forward" || id.value == "stub" || id.value.empty()) {
			return RendererDescriptor{.handle = makeCounterHandle<RendererHandle>(),
												.id = std::visit([](const auto &renderer) { return renderer.id(); }, renderer_),
												.shadow_maps = false,
												.passes = std::visit([](const auto &renderer) { return renderer.passes(); }, renderer_)};
		}
		return std::unexpected(Error::invalid_argument);
	}

	/// @brief Builds a render graph from a renderer descriptor.
	inline auto RenderSystem::buildRenderGraph(const RendererDescriptor &renderer) const			-> std::expected<RenderGraph, Error>{
		return buildRenderGraph(renderer.passes);
	}

	/// @brief Builds a render graph from one pass list.
	inline auto RenderSystem::buildRenderGraph(std::span<const RenderPassContract> passes) const	-> std::expected<RenderGraph, Error>{
		const std::array lists{passes};
		return buildRenderGraph(lists);
	}

	/// @brief Builds a render graph from several pass lists.
	inline std::expected<RenderGraph, Error>
	RenderSystem::buildRenderGraph(std::span<const std::span<const RenderPassContract>> pass_lists) const {
		auto graph = RenderGraph{};
		auto handles = detail::RenderPassHandleMap{};
		for (const auto passes : pass_lists) {
			for (const auto &pass : passes) {
				if (const auto result = addPass(graph, handles, pass); !result) { return std::unexpected(result.error()); }
			}
		}
		for (const auto passes : pass_lists) {
			if (const auto result = addDependencies(graph, handles, passes); !result) { return std::unexpected(result.error()); }
		}
		return graph;
	}

	/// @brief Adds one graph node, reusing milestone names shared by systems.
	inline std::expected<void, Error>
	RenderSystem::addPass(RenderGraph &graph, detail::RenderPassHandleMap &handles, const RenderPassContract &pass) {
		if (pass.name.empty()) { return std::unexpected(Error::invalid_argument); }
		if (handles.contains(pass.name)) { return {}; }
		auto handle = graph.addNode(ObjectName{.value = std::string(pass.name)});
		if (!handle) { return std::unexpected(handle.error()); }
		handles.emplace(pass.name, *handle);
		return {};
	}

	/// @brief Adds dependency edges for one pass list.
	inline std::expected<void, Error>
	RenderSystem::addDependencies(RenderGraph &graph, const detail::RenderPassHandleMap &handles,
											std::span<const RenderPassContract> passes) {
		for (const auto &pass : passes) {
			const auto pass_handle = handles.at(pass.name);
			for (const auto dependency : pass.depends_on) {
				if (dependency.empty()) { return std::unexpected(Error::invalid_argument); }
				const auto found = handles.find(dependency);
				if (found == handles.end()) { return std::unexpected(Error::missing_object); }
				graph.addEdge(found->second, pass_handle);
			}
		}
		return {};
	}

	/// @brief Creates a generic render resource descriptor.
	inline std::expected<RenderResourceHandle, Error>
	RenderSystem::createResource(RenderResourceKind kind, ObjectName name) {
		if (name.value.empty()) { name.value = "resource_" + std::to_string(resources_.size()); }
		auto resource = RenderResource{.handle = makeCounterHandle<RenderResourceHandle>(),
													.kind = kind, .name = std::move(name)};
		resources_.push_back(std::move(resource));
		return resources_.back().handle;
	}

	/// @brief Creates a generic render function descriptor.
	inline std::expected<RenderFunctionHandle, Error>
	RenderSystem::createFunction(ObjectName name, Vector<RenderResourceHandle> reads,
											Vector<RenderResourceHandle> writes) {
		if (name.value.empty()) { return std::unexpected(Error::invalid_argument); }
		auto valid = [&](RenderResourceHandle handle) { return find(handle) != nullptr; };
		if (!std::ranges::all_of(reads, valid) || !std::ranges::all_of(writes, valid)) {
			return std::unexpected(Error::missing_object);
		}
		auto function = RenderFunction{.handle = makeCounterHandle<RenderFunctionHandle>(),
													.name = std::move(name),
													.reads = std::move(reads),
													.writes = std::move(writes)};
		functions_.push_back(std::move(function));
		return functions_.back().handle;
	}

	inline std::size_t RenderSystem::resourceCount() const { return resources_.size(); }
	inline std::size_t RenderSystem::functionCount() const { return functions_.size(); }

	/// @brief Finds a generic render resource by handle.
	inline const RenderResource *RenderSystem::find(RenderResourceHandle handle) const {
		const auto found = std::ranges::find(resources_, handle, &RenderResource::handle);
		return found == resources_.end() ? nullptr : std::addressof(*found);
	}

	/// @brief Finds a generic render function by handle.
	inline const RenderFunction *RenderSystem::find(RenderFunctionHandle handle) const {
		const auto found = std::ranges::find(functions_, handle, &RenderFunction::handle);
		return found == functions_.end() ? nullptr : std::addressof(*found);
	}

	/// @brief Returns the current forward renderer backend.
	inline auto RenderSystem::forward()																			-> ForwardRenderer &{
		return std::get<ForwardRenderer>(renderer_);
	}

	/// @brief Returns the current forward renderer backend.
	inline auto RenderSystem::forward() const																	-> const ForwardRenderer &{
		return std::get<ForwardRenderer>(renderer_);
	}

	/// @brief Renderer-selection seam used by renderer-specific tests.
	inline auto RenderSystem::backend()																			-> SelectedRenderer &{
		return renderer_;
	}

	/// @brief Renderer-selection seam used by renderer-specific tests.
	inline auto RenderSystem::backend() const																	-> const SelectedRenderer &{
		return renderer_;
	}

	inline auto RenderSystem::resourceName(RenderResourceHandle handle) const							-> std::expected<ObjectName, Error>{
		const auto *resource = find(handle);
		return resource == nullptr ? std::unexpected(Error::missing_object) : std::expected<ObjectName, Error>{resource->name};
	}

	inline auto RenderSystem::resourceKind(RenderResourceHandle handle) const							-> std::expected<RenderResourceKind, Error>{
		const auto *resource = find(handle);
		return resource == nullptr ? std::unexpected(Error::missing_object) :
												std::expected<RenderResourceKind, Error>{resource->kind};
	}

	inline auto RenderSystem::functionName(RenderFunctionHandle handle) const							-> std::expected<ObjectName, Error>{
		const auto *function = find(handle);
		return function == nullptr ? std::unexpected(Error::missing_object) : std::expected<ObjectName, Error>{function->name};
	}

	/// @brief Mirrors one facade scene instance into the backend scene that the renderer uploads.
	inline auto RenderSystem::appendBackendObject(RenderInstanceHandle instance_handle)					-> std::expected<void, Error>{
		const auto *instance = scene_.findInstance(instance_handle);
		if (instance == nullptr) { return std::unexpected(Error::missing_object); }
		const auto *mesh = scene_.findMesh(instance->mesh);
		const auto *material = scene_.findMaterial(instance->material);
		if (mesh == nullptr || material == nullptr) { return std::unexpected(Error::missing_object); }

		auto backend_mesh = Mesh{};
		backend_mesh.vertices.reserve(mesh->vertices.size());
		backend_mesh.indices.reserve(mesh->indices.size());
		const auto color = std::array{material->base_color.value.x, material->base_color.value.y,
											 material->base_color.value.z};
		for (const RenderVertex &vertex : mesh->vertices) {
			backend_mesh.vertices.push_back(Vertex{.position = {vertex.position.x, vertex.position.y, vertex.position.z},
																.color = color,
																.texCoord = {vertex.uv.x, vertex.uv.y}});
		}
		for (const auto index : mesh->indices) { backend_mesh.indices.push_back(index); }

		auto model = translate(identityMat4(), instance->local_transform.translation.value);
		model = scale(model, instance->local_transform.scale.value);
		const auto use_texture = !material->base_color_texture_source.empty();
		std::visit([&](auto &renderer) {
			renderer.appendObject(std::move(backend_mesh), model,
										 use_texture ? std::optional<std::string>{material->base_color_texture_source.string()} :
														 std::nullopt);
		}, renderer_);
		return {};
	}

	/// @brief Adds a plane mesh, material, and instance to the CPU scene.
	inline auto RenderSystem::addPlane(Vec2 half_extent, LinearColor color, Transform transform)	-> std::expected<void, Error>{
		const auto material = scene_.addMaterial(RenderMaterial{.base_color = color});
		const auto mesh = scene_.addPlaneMesh(half_extent);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		return appendBackendObject(*instance);
	}

	/// @brief Adds a cuboid mesh, material, and instance to the CPU scene.
	inline std::expected<void, Error>
	RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color, Transform transform) {
		const auto material = scene_.addMaterial(RenderMaterial{.base_color = color});
		const auto mesh = scene_.addCuboidMesh(minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		return appendBackendObject(*instance);
	}

	/// @brief Adds a cuboid that requests the backend scene base-color texture.
	inline std::expected<void, Error>
	RenderSystem::addTexturedCuboid(Vec3 minimum, Vec3 maximum, std::filesystem::path base_color_texture,
											  Transform transform) {
		if (base_color_texture.empty()) { return std::unexpected(Error::io_error); }
		auto texture_file = std::ifstream{base_color_texture, std::ios::binary};
		if (!texture_file) { return std::unexpected(Error::io_error); }

		const auto material = scene_.addMaterial(RenderMaterial{.base_color = LinearColor{.value = oneVec3()},
																			 .base_color_texture = makeCounterHandle<TextureHandle>(),
																			 .base_color_texture_source = base_color_texture});
		const auto mesh = scene_.addCuboidMesh(minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		return appendBackendObject(*instance);
	}

	inline void RenderSystem::clearScene() {
		scene_.clear();
		forward().scene.objects.clear();
		forward().scene.baseColorTexture.reset();
	}
	inline auto RenderSystem::setCamera(Camera camera, PixelExtent extent)								-> void{
		const auto eye = camera.position.value;
		const auto target = math::add(eye, camera.forward.value);
		forward().setCamera(eye, target);
		scene_.setCamera(RenderCamera{.camera = std::move(camera), .target_extent = extent});
	}
	inline void RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
																	LightIntensity intensity, LinearColor ambient) {
		forward().scene.directionalLight = DirectionalLight{
			.direction = direction_to_light.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		scene_.setDirectionalLight(RenderDirectionalLight{.direction_to_light = direction_to_light,
																			.color = color, .intensity = intensity, .ambient = ambient});
	}
	inline void RenderSystem::setPointLight(Position position, LinearColor color,
															LightIntensity intensity, LightRange range) {
		forward().scene.pointLight = PointLight{
			.position = position.value,
			.color = color.value,
			.intensity = intensity.value,
			.range = range.value,
			.ambient = forward().scene.pointLight.ambient};
		scene_.setPointLight(RenderPointLight{.position = position, .color = color,
															.intensity = intensity, .range = range});
	}
	inline void RenderSystem::setPointLight(Position position, LinearColor color,
															LightIntensity intensity, LightRange range, LinearColor ambient) {
		forward().scene.pointLight = PointLight{.position = position.value,
															 .color = color.value,
															 .intensity = intensity.value,
															 .range = range.value,
															 .ambient = ambient.value.x};
		scene_.setPointLight(RenderPointLight{.position = position, .color = color,
															.intensity = intensity, .range = range, .ambient = ambient});
	}
	inline void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
														LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = forward().scene.spotLight.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = forward().scene.spotLight.ambient};
		forward().scene.spotLights.clear();																// Setter preserves the legacy single-light mode.
		forward().scene.spotLights.push_back(light);
		forward().scene.spotLight = light;
		scene_.setSpotLight(RenderSpotLight{.position = position, .direction = direction, .color = color,
														.intensity = intensity, .range = range, .cone = cone});
	}
	inline void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
														LightIntensity intensity, LightRange range,
														SpotConeAngle cone, LinearColor ambient) {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = forward().scene.spotLight.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		forward().scene.spotLights.clear();																// Setter replaces all CPU-side spot lights.
		forward().scene.spotLights.push_back(light);
		forward().scene.spotLight = light;
		scene_.setSpotLight(RenderSpotLight{.position = position, .direction = direction, .color = color,
														.intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
	}

	inline void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
													 LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = SpotLight{}.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = SpotLight{}.ambient};
		if (forward().scene.spotLights.size() < kMaxShadowedSpotLights) {								// Extra lights are stored only for later CPU shadow work.
			forward().scene.spotLights.push_back(light);
			if (forward().scene.spotLights.size() == 1U) { forward().scene.spotLight = light; }
		}
		scene_.addSpotLight(RenderSpotLight{.position = position, .direction = direction, .color = color,
													 .intensity = intensity, .range = range, .cone = cone});
	}

	inline void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
													 LightIntensity intensity, LightRange range,
													 SpotConeAngle cone, LinearColor ambient) {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = SpotLight{}.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		if (forward().scene.spotLights.size() < kMaxShadowedSpotLights) {								// First entry remains the shader-visible spot light.
			forward().scene.spotLights.push_back(light);
			if (forward().scene.spotLights.size() == 1U) { forward().scene.spotLight = light; }
		}
		scene_.addSpotLight(RenderSpotLight{.position = position, .direction = direction, .color = color,
													 .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
	}

	inline auto RenderSystem::loadScene(Scene scene)																-> void{
		if (scene.spotLights.size() > kMaxShadowedSpotLights) { scene.spotLights.resize(kMaxShadowedSpotLights); }
		if (!scene.spotLights.empty()) { scene.spotLight = scene.spotLights.front(); }				// Forward renderer still consumes the first spot light only.
		forward().loadScene(std::move(scene));
	}

	inline auto RenderSystem::setGuiSystem(void *gui)																-> void{
		guiSystem_ = gui;
	}

	/// @brief Forwards the GUI recorder into the active forward renderer.
	inline auto RenderSystem::setGuiRecordSink(std::function<void(VkCommandBuffer)> sink)			-> void{
		if (std::holds_alternative<ForwardRenderer>(renderer_)) {
			std::get<ForwardRenderer>(renderer_).setGuiRecordSink(std::move(sink));
		}
	}

	inline auto RenderSystem::initialize(SDL_Window *window, RendererId id)									-> std::expected<void, Error>{
		if (initialized_) { return {}; }
		if (window == nullptr) { return std::unexpected(Error::invalid_argument); }
		if (id.value == "forward" || id.value.empty()) {
			if (!std::holds_alternative<ForwardRenderer>(renderer_)) { renderer_.emplace<ForwardRenderer>(); }
			std::get<ForwardRenderer>(renderer_).setGuiSystem(guiSystem_);
		} else if (id.value == "stub") {
			if (!std::holds_alternative<StubRenderer>(renderer_)) { renderer_.emplace<StubRenderer>(); }
		} else {
			return std::unexpected(Error::invalid_argument);
		}
		const VkResult result = std::visit([window](auto &renderer) { return renderer.init(window); }, renderer_);
		if (result != VK_SUCCESS) { return std::unexpected(Error::platform_error); }
		initialized_ = true;
		return {};
	}

	inline auto RenderSystem::makeGuiInitInfo() const													-> std::optional<ImGui_ImplVulkan_InitInfo>{
		if (!initialized_ || !std::holds_alternative<ForwardRenderer>(renderer_)) { return std::nullopt; }
		auto info = std::get<ForwardRenderer>(renderer_).makeImguiInitInfo();
		if (info.Device == VK_NULL_HANDLE || info.RenderPass == VK_NULL_HANDLE || info.DescriptorPool == VK_NULL_HANDLE) {
			return std::nullopt;
		}
		return info;
	}

	inline auto RenderSystem::shutdown()																				-> void{
		if (initialized_) {
			std::visit([](auto &renderer) {
				if constexpr (std::same_as<std::remove_cvref_t<decltype(renderer)>, ForwardRenderer>) {
					(void)vkDeviceWaitIdle(renderer.device.device);
				}
				renderer.shutdown();
			}, renderer_);
			initialized_ = false;
		}
	}

	inline auto RenderSystem::initialized() const																	-> bool{
		return initialized_;
	}

	inline std::size_t RenderSystem::sceneMeshCount() const { return scene_.meshCount(); }
	inline std::size_t RenderSystem::sceneMaterialCount() const { return scene_.materialCount(); }
	inline std::size_t RenderSystem::sceneInstanceCount() const { return scene_.instanceCount(); }
	inline std::size_t RenderSystem::sceneVertexCount() const { return scene_.vertexCount(); }
	inline std::size_t RenderSystem::sceneIndexCount() const { return scene_.indexCount(); }
	/// @brief Returns the retained directional shadow-depth debug sample count.
	inline std::size_t RenderSystem::sceneShadowDepthSampleCount() const { return forward().sceneShadowDepthSampleCount(); }
	/// @brief Returns one retained directional shadow-depth debug sample.
	inline std::optional<RenderShadowDepthSample> RenderSystem::sceneShadowDepthSample(std::size_t index) const { return forward().sceneShadowDepthSample(index); }
	/// @brief Returns the retained spot shadow-depth debug sample count.
	inline std::size_t RenderSystem::sceneSpotShadowDepthSampleCount() const { return forward().sceneSpotShadowDepthSampleCount(); }
	/// @brief Returns one retained spot shadow-depth debug sample.
	inline std::optional<RenderShadowDepthSample> RenderSystem::sceneSpotShadowDepthSample(std::size_t index) const { return forward().sceneSpotShadowDepthSample(index); }
	/// @brief Returns downloaded GPU depth for one retained spot shadow-depth sample.
	inline std::optional<float> RenderSystem::sceneSpotShadowDepthGpuDepth(std::size_t index) const {
		const auto sample = forward().sceneSpotShadowDepthSample(index);
		if (!sample || !sample->has_gpu) { return std::nullopt; }
		return sample->gpu_depth;
	}
	/// @brief Reports whether one retained spot shadow-depth sample has GPU data.
	inline std::optional<bool> RenderSystem::sceneSpotShadowDepthHasGpu(std::size_t index) const {
		const auto sample = forward().sceneSpotShadowDepthSample(index);
		if (!sample || !sample->has_gpu) { return std::nullopt; }
		return sample->has_gpu;
	}
	/// @brief Returns absolute CPU/GPU depth error for one retained spot shadow-depth sample.
	inline std::optional<float> RenderSystem::sceneSpotShadowDepthError(std::size_t index) const {
		const auto sample = forward().sceneSpotShadowDepthSample(index);
		if (!sample || !sample->has_gpu) { return std::nullopt; }
		return sample->error;
	}
	inline bool RenderSystem::hasSceneCamera() const { return scene_.camera().has_value(); }
	inline bool RenderSystem::hasSceneDirectionalLight() const { return scene_.directionalLight().has_value(); }
	inline bool RenderSystem::hasScenePointLight() const { return scene_.pointLight().has_value(); }
	inline bool RenderSystem::hasSceneSpotLight() const { return scene_.spotLight().has_value(); }

	/// @brief Copies the last rendered swapchain image and writes it as a PNG.
	auto RenderSystem::captureFrameToPng(const std::filesystem::path &output_path)	-> std::expected<void, Error>{
		if (!initialized_) { return std::unexpected(Error::not_initialized); }
		if (output_path.empty()) { return std::unexpected(Error::invalid_argument); }
		return std::visit([&](auto &renderer) { return renderer.captureFrameToPng(output_path); }, renderer_);
	}

	/// @brief Records a frame without creating GPU objects.
	inline auto RenderSystem::renderFrame(const WindowFrameData &windows)								-> std::expected<void, Error>{
		last_window_count_ = std::ranges::count_if(windows.windows, [](const WindowInfo &window) {
			return !window.should_close;
		});
		if (initialized_) {
			std::visit([](auto &renderer) { renderer.renderFrame(nullptr); }, renderer_);
			++rendered_frames_;
		} else {
			++rendered_frames_;
		}
		return {};
	}

	/// @brief Records a frame using the current window snapshot.
	inline auto RenderSystem::renderFrame(WindowSystem &windows)											-> std::expected<void, Error>{
		return renderFrame(WindowFrameData{.windows = windows.snapshot()});
	}

	inline std::uint64_t RenderSystem::renderedFrameCount() const { return rendered_frames_; }
	inline std::size_t RenderSystem::lastRenderedWindowCount() const { return last_window_count_; }

} // namespace vve::simple
