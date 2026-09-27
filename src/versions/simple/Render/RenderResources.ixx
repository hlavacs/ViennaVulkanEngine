module;

#include <stb_image.h>

export module VEEngine.Simple.RenderResources;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.Scene;

/**
	* @file
	* @brief CPU render-resource model and storage for the simple render system.
	*
	* Functional objects:
	* - Render*Handle aliases identify CPU render resources and registry entries.
	* - RenderMesh, RenderMaterial, and RenderInstance store CPU scene geometry, material, and draw-item data.
	* - RenderDirectionalLight, RenderPointLight, RenderSpotLight, and RenderCamera store CPU light and camera resources.
	* - RenderScene owns the CPU render-resource storage and scene mutation helpers.
	*/

export namespace vve::simple {

	struct RenderMeshHandleTag {};																				///< simple render mesh handle tag.
	struct RenderMaterialHandleTag {};																			///< simple render material handle tag.
	struct RenderInstanceHandleTag {};																			///< simple render instance handle tag.

	using RenderMeshHandle	= TypedHandle<RenderMeshHandleTag>;										///< simple render mesh handle.
	using RenderMaterialHandle = TypedHandle<RenderMaterialHandleTag>;								///< simple render material handle.
	using RenderInstanceHandle = TypedHandle<RenderInstanceHandleTag>;								///< simple render instance handle.
	using RenderTextureIndex = std::uint32_t;															///< Dense index into the render-scene texture table.


	inline constexpr RenderTextureIndex kNoRenderTexture{std::numeric_limits<RenderTextureIndex>::max()}; ///< Sentinel for an absent material texture.

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
		RenderTextureIndex base_color_texture_index{kNoRenderTexture};			///< Base-color texture-table index.
		RenderTextureIndex normal_texture_index{kNoRenderTexture};				///< Normal texture-table index.
		RenderTextureIndex metalness_texture_index{kNoRenderTexture};			///< Metalness texture-table index.
		RenderTextureIndex roughness_texture_index{kNoRenderTexture};			///< Roughness texture-table index.
		RenderTextureIndex emissive_texture_index{kNoRenderTexture};			///< Emissive texture-table index.
		RenderTextureIndex ambient_occlusion_texture_index{kNoRenderTexture}; ///< Ambient-occlusion texture-table index.
	};

	/// @brief Scene draw item connecting mesh, material, and transforms.
	struct RenderInstance {
		RenderInstanceHandle handle{};												///< Stable render instance handle.
		RenderMeshHandle mesh{};														///< Mesh drawn by this instance.
		RenderMaterialHandle material{};												///< Material used by this instance.
		Transform local_transform{};													///< Source scene local transform.
		Mat4 world_transform{identityMat4()};										///< World transform used by later renderers.
		bool visible{true};																///< True when this instance should be rendered.
		bool casts_shadow{true};													///< True when this instance participates in shadow passes.
		bool unlit{false};														///< True when this instance bypasses lighting.
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

	/// @brief Minimal CPU scene that stores render resources but does not draw them.
	class RenderScene {
	public:
		[[nodiscard]] auto acquireTexture(const std::filesystem::path &source,
			MaterialTextureSemantic semantic = MaterialTextureSemantic::base_color)
			-> std::expected<RenderTextureIndex, Error>;
		[[nodiscard]] RenderMaterialHandle addMaterial(RenderMaterial material = {});
		[[nodiscard]] RenderMeshHandle addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
														Bounds bounds = {});
		[[nodiscard]] RenderMeshHandle addTriangleMesh(Vector<Vec3> positions,
			Vector<std::uint32_t> indices, Bounds bounds = {});
		[[nodiscard]] auto addPlaneMesh(Vec2 half_extent)																		-> RenderMeshHandle;
		[[nodiscard]] auto addCuboidMesh(Vec3 minimum, Vec3 maximum)														-> RenderMeshHandle;
		[[nodiscard]] std::expected<RenderInstanceHandle, Error>
		addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local = {},
						Mat4 world = identityMat4());
		auto setCamera(RenderCamera camera)																							-> void;
		auto addImportedCamera(CameraDescriptor camera)																		-> void;
		auto setDirectionalLight(RenderDirectionalLight light)																-> void;
		auto addDirectionalLight(RenderDirectionalLight light)																-> void;
		auto setPointLight(RenderPointLight light)																				-> void;
		auto addPointLight(RenderPointLight light)																				-> void;
		auto setSpotLight(RenderSpotLight light)																					-> void;
		auto addSpotLight(RenderSpotLight light)																					-> void;
		auto clear()																														-> void;
		[[nodiscard]] auto eraseInstance(RenderInstanceHandle handle)													-> bool;
		[[nodiscard]] auto purgeUnusedAssets()																				-> std::size_t;
		[[nodiscard]] RenderMesh *findMesh(RenderMeshHandle handle);
		[[nodiscard]] const RenderMesh *findMesh(RenderMeshHandle handle) const;
		[[nodiscard]] const RenderMaterial *findMaterial(RenderMaterialHandle handle) const;
		[[nodiscard]] const RenderTexture *findTexture(RenderTextureIndex index) const;
		[[nodiscard]] auto textureCount() const -> std::size_t;
		[[nodiscard]] RenderInstance *findInstance(RenderInstanceHandle handle);
		[[nodiscard]] const RenderInstance *findInstance(RenderInstanceHandle handle) const;
		[[nodiscard]] auto meshCount() const																						-> std::size_t;
		[[nodiscard]] auto materialCount() const																					-> std::size_t;
		[[nodiscard]] auto instanceCount() const																					-> std::size_t;
		[[nodiscard]] auto vertexCount() const																						-> std::size_t;
		[[nodiscard]] auto indexCount() const																						-> std::size_t;
		[[nodiscard]] const std::optional<RenderCamera> &camera() const;
		[[nodiscard]] const std::optional<RenderDirectionalLight> &directionalLight() const;
		[[nodiscard]] const std::vector<RenderDirectionalLight> &directionalLights() const;
		[[nodiscard]] const std::optional<RenderPointLight> &pointLight() const;
		[[nodiscard]] const std::vector<RenderPointLight> &pointLights() const;
		[[nodiscard]] std::optional<RenderSpotLight> spotLight() const;
		[[nodiscard]] const std::vector<RenderSpotLight> &spotLights() const;
		[[nodiscard]] const std::vector<RenderCamera> &importedCameras() const;
		[[nodiscard]] const Vector<RenderMesh> &meshes() const;
		[[nodiscard]] const Vector<RenderInstance> &instances() const;
		[[nodiscard]] const Vector<RenderMaterial> &materials() const;

	private:
		static void appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
										Vec3 normal, std::array<Vec3, 4> corners);

		Vector<RenderMesh> meshes_{};													///< CPU mesh resources.
		Vector<RenderMaterial> materials_{};										///< CPU material resources.
		Vector<RenderInstance> instances_{};										///< CPU draw items.
		std::deque<RenderTexture> textures_{};									///< Stable decoded texture entries in shader-index order.
		std::map<std::filesystem::path, RenderTextureIndex> texture_indices_{}; ///< Canonical source path to texture-table index.
		std::optional<RenderCamera> camera_{};										///< Optional active camera.
		std::optional<RenderDirectionalLight> light_{};							///< Optional active directional light.
		std::vector<RenderDirectionalLight> directional_lights_{};			///< Capped active directional lights.
		std::optional<RenderPointLight> point_light_{};							///< Optional active point light.
		std::vector<RenderPointLight> point_lights_{};						///< Capped active point lights.
		std::vector<RenderSpotLight> spot_lights_{};							///< Capped active spot lights.
		std::vector<RenderCamera> imported_cameras_{};						///< Cameras imported for render-scene inspection.
	};

} // namespace vve::simple

namespace vve::simple {
	/// @brief Returns a canonical absolute path when the filesystem can resolve the source.
	[[nodiscard]] inline auto canonicalTexturePath(const std::filesystem::path &source)
		-> std::expected<std::filesystem::path, Error> {
		if (source.empty()) { return std::unexpected(Error::io_error); }
		std::error_code error{};
		auto canonical = std::filesystem::weakly_canonical(source, error);
		if (error) {
			error.clear();
			canonical = std::filesystem::absolute(source, error).lexically_normal();
		}
		if (error || !canonical.is_absolute()) { return std::unexpected(Error::io_error); }
		return canonical;
	}

	/// @brief Finds or decodes one canonical texture-table entry.
	inline auto RenderScene::acquireTexture(const std::filesystem::path &source, MaterialTextureSemantic semantic)
		-> std::expected<RenderTextureIndex, Error> {
		const auto canonical = canonicalTexturePath(source);
		if (!canonical) { return std::unexpected(canonical.error()); }
		if (const auto found = texture_indices_.find(*canonical); found != texture_indices_.end()) {
			return found->second;
		}

		int width{};
		int height{};
		int channels{};
		const auto path = canonical->string();
		auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
			stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
		if (!pixels || width <= 0 || height <= 0) { return std::unexpected(Error::io_error); }

		const auto extent = PixelExtent{.width = static_cast<std::uint32_t>(width),
			.height = static_cast<std::uint32_t>(height)};
		const auto byte_count = static_cast<std::size_t>(extent.width) * extent.height * 4U;
		const auto *begin = reinterpret_cast<const std::byte *>(pixels.get());
		const auto index = static_cast<RenderTextureIndex>(textures_.size());
		const bool linear = semantic == MaterialTextureSemantic::normal ||
			semantic == MaterialTextureSemantic::metalness || semantic == MaterialTextureSemantic::roughness ||
			semantic == MaterialTextureSemantic::ambient_occlusion;
		textures_.push_back(RenderTexture{.canonical_path = *canonical,
			.rgba8 = std::vector<std::byte>{begin, begin + byte_count}, .extent = extent, .linear = linear});
		texture_indices_.emplace(textures_.back().canonical_path, index);
		return index;
	}

	/// @brief Adds one quad face to a CPU mesh.
	inline void RenderScene::appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
													Vec3 normal, std::array<Vec3, 4> corners) {
		const auto base = static_cast<std::uint32_t>(vertices.size());
		const auto uvs = std::array{Vec2{0.0F, 0.0F}, Vec2{1.0F, 0.0F}, Vec2{1.0F, 1.0F}, Vec2{0.0F, 1.0F}};
		const auto tangent_xyz = normalize(subtract(corners[1], corners[0]));
		const auto bitangent = normalize(subtract(corners[3], corners[0]));
		const auto tangent = Vec4{tangent_xyz.x, tangent_xyz.y, tangent_xyz.z,
			dot(cross(normal, tangent_xyz), bitangent) < zero() ? -one() : one()};
		for (std::size_t corner_index{}; corner_index < corners.size(); ++corner_index) {
			vertices.push_back(RenderVertex{.position = corners[corner_index], .normal = normal,
				.uv = uvs[corner_index], .tangent = tangent});
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

	/// @brief Converts public positions into one CPU-side indexed triangle mesh.
	inline RenderMeshHandle RenderScene::addTriangleMesh(
		Vector<Vec3> positions, Vector<std::uint32_t> indices, Bounds bounds) {
		auto vertices = Vector<RenderVertex>{};
		vertices.reserve(positions.size());
		for (const Vec3 &position : positions) {
			vertices.push_back(RenderVertex{.position = position});
		}
		return addMesh(std::move(vertices), std::move(indices), bounds);
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

	/// @brief Appends one imported camera for render-scene inspection.
	inline void RenderScene::addImportedCamera(CameraDescriptor camera) {
		const auto target = math::add(camera.position.value, camera.direction.value);
		imported_cameras_.push_back(RenderCamera{.camera = Camera{.position = camera.position,
																				 .forward = camera.direction,
																				 .view_transform = math::lookAt(camera.position.value, target, camera.up.value),
																				 .fov_y = camera.fov,
																				 .clip = ClipPlanes{.near_plane = camera.near_clip, .far_plane = camera.far_clip}},
															 .target_extent = PixelExtent{.width = 1, .height = 1}});
	}

	/// @brief Replaces the active directional-light list with one first-light entry.
	inline void RenderScene::setDirectionalLight(RenderDirectionalLight light) {
		directional_lights_.clear();
		directional_lights_.push_back(std::move(light));
		light_ = directional_lights_.front();
	}

	/// @brief Adds one directional light until the fixed simple-engine cap is reached.
	inline void RenderScene::addDirectionalLight(RenderDirectionalLight light) {
		if (directional_lights_.size() < kMaxDirectionalLights) { directional_lights_.push_back(std::move(light)); }
		if (!directional_lights_.empty()) { light_ = directional_lights_.front(); }
	}

	/// @brief Replaces the active point-light list with one first-light entry.
	inline void RenderScene::setPointLight(RenderPointLight light) {
		point_lights_.clear();
		point_lights_.push_back(std::move(light));
		point_light_ = point_lights_.front();
	}

	/// @brief Adds one point light until the fixed simple-engine cap is reached.
	inline void RenderScene::addPointLight(RenderPointLight light) {
		if (point_lights_.size() < kMaxShadowedPointLights) { point_lights_.push_back(std::move(light)); }
		if (!point_lights_.empty()) { point_light_ = point_lights_.front(); }
	}

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
		textures_.clear();
		texture_indices_.clear();
		camera_.reset();
		light_.reset();
		directional_lights_.clear();
		point_light_.reset();
		point_lights_.clear();
		spot_lights_.clear();
		imported_cameras_.clear();
	}

	/// @brief Removes one CPU draw item by stable handle.
	inline auto RenderScene::eraseInstance(RenderInstanceHandle handle)										-> bool{
		const auto found = std::ranges::find(instances_, handle, &RenderInstance::handle);
		if (found == instances_.end()) { return false; }
		instances_.erase(found);
		return true;
	}

	/// @brief Removes mesh and material resources no live instance references.
	inline auto RenderScene::purgeUnusedAssets()																-> std::size_t{
		const auto mesh_referenced = [this](RenderMeshHandle handle) {
			return std::ranges::any_of(instances_, [handle](const RenderInstance &instance) { return instance.mesh == handle; });
		};
		const auto material_referenced = [this](RenderMaterialHandle handle) {
			return std::ranges::any_of(instances_, [handle](const RenderInstance &instance) { return instance.material == handle; });
		};

		const auto mesh_count = meshes_.size();
		const auto material_count = materials_.size();
		auto unused_meshes = std::ranges::remove_if(meshes_, [&](const RenderMesh &mesh) { return !mesh_referenced(mesh.handle); });
		for (auto current = unused_meshes.begin(); current != meshes_.end();) { current = meshes_.erase(current); }
		auto unused_materials = std::ranges::remove_if(materials_, [&](const RenderMaterial &material) {
			return !material_referenced(material.handle);
		});
		for (auto current = unused_materials.begin(); current != materials_.end();) { current = materials_.erase(current); }
		return (mesh_count - meshes_.size()) + (material_count - materials_.size());
	}

	/// @brief Finds a mesh by handle.
	inline RenderMesh *RenderScene::findMesh(RenderMeshHandle handle) {
		const auto found = std::ranges::find(meshes_, handle, &RenderMesh::handle);
		return found == meshes_.end() ? nullptr : std::addressof(*found);
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

	/// @brief Finds a decoded texture by its dense table index.
	inline const RenderTexture *RenderScene::findTexture(RenderTextureIndex index) const {
		return index < textures_.size() ? std::addressof(textures_[index]) : nullptr;
	}

	/// @brief Finds an instance by handle.
	inline RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) {
		const auto found = std::ranges::find(instances_, handle, &RenderInstance::handle);
		return found == instances_.end() ? nullptr : std::addressof(*found);
	}

	/// @brief Finds an instance by handle.
	inline const RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) const {
		const auto found = std::ranges::find(instances_, handle, &RenderInstance::handle);
		return found == instances_.end() ? nullptr : std::addressof(*found);
	}

	inline std::size_t RenderScene::meshCount() const { return meshes_.size(); }
	inline std::size_t RenderScene::materialCount() const { return materials_.size(); }
	inline std::size_t RenderScene::textureCount() const { return textures_.size(); }
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
	inline const std::vector<RenderDirectionalLight> &RenderScene::directionalLights() const { return directional_lights_; }
	inline const std::optional<RenderPointLight> &RenderScene::pointLight() const { return point_light_; }
	inline const std::vector<RenderPointLight> &RenderScene::pointLights() const { return point_lights_; }
	inline std::optional<RenderSpotLight> RenderScene::spotLight() const {
		return spot_lights_.empty() ? std::nullopt : std::optional<RenderSpotLight>{spot_lights_.front()};
	}
	inline const std::vector<RenderSpotLight> &RenderScene::spotLights() const { return spot_lights_; }
	inline const std::vector<RenderCamera> &RenderScene::importedCameras() const { return imported_cameras_; }
	inline const Vector<RenderMesh> &RenderScene::meshes() const { return meshes_; }
	inline const Vector<RenderInstance> &RenderScene::instances() const { return instances_; }
	inline const Vector<RenderMaterial> &RenderScene::materials() const { return materials_; }

} // namespace vve::simple
