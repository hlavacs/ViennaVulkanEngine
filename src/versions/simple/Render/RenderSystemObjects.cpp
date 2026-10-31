module VVEngine.Simple;
import std;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Renderer;

/// @file
/// @brief RenderSystem definitions that create primitive render objects, remove live objects, and manage loaded backend scenes.

namespace vve::simple::detail {

	/// @brief Owner tag for the lights of a scene loaded with loadScene(Scene).
	///
	/// A generation bit, which counter handles never set, keeps it apart from scene-instance owners.
	[[nodiscard]] auto lightSceneOwner(SceneHandle scene) -> std::uint64_t { return scene.value | (1ULL << 62U); }

} // namespace vve::simple::detail

namespace vve::simple {

	/// @brief Removes one live render object, including its entry in the scene instance that created it.
	auto RenderSystem::removeObject(RenderObjectHandle handle) -> std::expected<void, Error> {
		const auto object = findRenderObject(handle);
		const auto *instance = object ? scene_.findInstance(*object) : nullptr;
		if (instance == nullptr) { return std::unexpected(Error::missing_object); }
		const auto mesh = instance->mesh;
		const auto material = instance->material;
		if (!scene_.eraseInstance(*object)) { return std::unexpected(Error::missing_object); }
		releaseUnused(mesh, material);
		eraseRenderObject(handle);
		if (const auto source = object_sources_.find(handle); source != object_sources_.end()) {
			if (const auto instance = scene_instances_.find(source->second.first); instance != scene_instances_.end()) {
				auto &objects = instance->second;
				if (const auto entry = std::ranges::find(objects, handle); entry != objects.end()) { objects.erase(entry); }
			}
			object_sources_.erase(source);
		}
		detachObject(handle);	// The scene tree stops placing the removed object.
		renderer_.markSceneResourcesDirty();
		return {};
	}

	/// @brief Adds an unlit, blended, textured quad of the given size that always faces the camera.
	///
	/// Only the translation and the x/y scale of the transform apply; a sprite casts no shadow.
	auto RenderSystem::addSprite(Vec2 size, std::filesystem::path texture, LinearColor color, Transform transform)
		-> std::expected<RenderObjectHandle, Error> {
		if (!(size.x > zero() && size.y > zero() && std::isfinite(size.x) && std::isfinite(size.y))) {
			return std::unexpected(Error::invalid_argument);
		}
		const auto texture_index = scene_.acquireTexture(texture);
		if (!texture_index) { return std::unexpected(texture_index.error()); }
		const auto material = acquirePrimitiveMaterial(color, *texture_index);
		const auto half = Vec3{size.x / 2, size.y / 2, 0};
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::sprite, Vec3{-half.x, -half.y, 0}, half);
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		auto *sprite = scene_.findInstance(*instance);
		sprite->sprite = true;
		sprite->unlit = true;
		sprite->casts_shadow = false;
		return registerRenderObject(*instance);
	}

	/// @brief Releases a mesh, a material and textures that no instance uses anymore.
	///
	/// Imported caches retain their resources; primitive and edited resources live until their last instance.
	auto RenderSystem::releaseUnused(RenderMeshHandle mesh, RenderMaterialHandle material) -> void {
		if (std::ranges::none_of(scene_.instances(), [mesh](const auto &item) { return item.mesh == mesh; }) &&
			std::ranges::none_of(imported_render_meshes_, [mesh](const auto &entry) { return entry.second == mesh; })) {
			scene_.eraseMesh(mesh);
			std::erase_if(primitive_meshes_, [mesh](const auto &entry) { return entry.second == mesh; });
		}
		if (material != default_material_ &&
			std::ranges::none_of(scene_.instances(), [material](const auto &item) { return item.material == material; }) &&
			std::ranges::none_of(imported_render_materials_, [material](const auto &entry) { return entry.second == material; })) {
			scene_.eraseMaterial(material);
			std::erase_if(primitive_materials_, [material](const auto &entry) { return entry.second == material; });
			renderer_.markMaterialsDirty();
		}
		(void)scene_.releaseUnusedTextures();
	}

	/// @brief Shows a 2:1 equirectangular panorama around every camera; it replaces an earlier skybox.
	///
	/// The sky is one internal instance (an inward-facing cube with an unlit textured material), not a render object.
	auto RenderSystem::setSkybox(const std::filesystem::path &panorama) -> std::expected<void, Error> {
		const auto texture = scene_.acquireTexture(panorama);
		if (!texture) { return std::unexpected(texture.error()); }
		const auto mesh = scene_.addCuboidMesh(Vec3{-1, -1, -1}, Vec3{1, 1, 1});
		// Reverse the winding so the faces point inwards, towards the camera at the centre.
		auto &indices = scene_.findMesh(mesh)->indices;
		for (std::size_t first{}; first + 2U < indices.size(); first += 3U) { std::swap(indices[first + 1U], indices[first + 2U]); }
		const auto material = acquirePrimitiveMaterial(LinearColor{.value = oneVec3()}, *texture);
		const auto sky = scene_.addInstance(mesh, material);
		if (!sky) { return std::unexpected(sky.error()); }
		auto *instance = scene_.findInstance(*sky);
		instance->sky = true;
		instance->casts_shadow = false;
		clearSkybox();	// The new texture is already held, so a repeated panorama is not decoded again.
		sky_ = *sky;
		renderer_.setEnvironmentTexture(*texture);
		renderer_.markSceneResourcesDirty(false); // Sky radiance changes, but no shadow-caster silhouette changes.
		return {};
	}

	/// @brief Removes the skybox, if any, and releases its resources; the background colour shows again.
	auto RenderSystem::clearSkybox() -> void {
		const auto *instance = scene_.findInstance(sky_);
		if (instance == nullptr) { return; }
		const auto mesh = instance->mesh;
		const auto material = instance->material;
		(void)scene_.eraseInstance(sky_);
		sky_ = {};
		renderer_.setEnvironmentTexture(kNoTexture);
		releaseUnused(mesh, material);
		renderer_.markSceneResourcesDirty(false);
	}

	/// @brief Returns public render objects registered for one scene instance.
	auto RenderSystem::sceneInstanceObjects(RenderSceneInstanceHandle instance) const
		-> std::expected<Vector<RenderObjectHandle>, Error> {
		const auto found = scene_instances_.find(instance);
		return found == scene_instances_.end() ? std::unexpected(Error::missing_object) :
															std::expected<Vector<RenderObjectHandle>, Error>{found->second};
	}

	/// @brief Returns the public scene instance that created one render object.
	auto RenderSystem::objectSourceScene(RenderObjectHandle handle) const
		-> std::expected<RenderSceneInstanceHandle, Error> {
		const auto found = object_sources_.find(handle);
		return found == object_sources_.end() ? std::unexpected(Error::missing_object) :
														  std::expected<RenderSceneInstanceHandle, Error>{found->second.first};
	}

	/// @brief Returns the source asset-scene node that created one render object.
	auto RenderSystem::objectSourceNode(RenderObjectHandle handle) const -> std::expected<NodeHandle, Error> {
		const auto found = object_sources_.find(handle);
		return found == object_sources_.end() ? std::unexpected(Error::missing_object) :
														  std::expected<NodeHandle, Error>{found->second.second};
	}

	/// @brief Removes a public scene instance with all render objects, lights, and cameras it created.
	auto RenderSystem::removeSceneInstance(RenderSceneInstanceHandle instance) -> std::expected<void, Error> {
		if (!scene_instances_.contains(instance)) { return std::unexpected(Error::missing_object); }
		rollbackSceneInstance(instance);
		renderer_.invalidateProbes(true);
		return {};
	}

	/// @brief Removes everything one scene instance created, irrespective of reparenting; also undoes a partly built instance.
	auto RenderSystem::rollbackSceneInstance(RenderSceneInstanceHandle instance) -> void {
		if (const auto found = scene_instances_.find(instance); found != scene_instances_.end()) {
			const auto objects = found->second; // removeObject edits the stored list.
			for (const auto object : objects) { (void)removeObject(object); }
		}
		removeImportedLights(instance.value);
		if (const auto nodes = instance_nodes_.find(instance); nodes != instance_nodes_.end()) {
			auto owned = std::unordered_set<SceneNodeHandle, HandleHash<SceneNodeHandle>>{};
			for (const auto &[asset_node, node] : nodes->second) { owned.insert(node); }
			// Parenthood does not transfer ownership: preserve foreign subtrees and objects before removing our nodes.
			for (const auto node : owned) {
				const auto found = scene_nodes_.find(node);
				if (found == scene_nodes_.end()) { continue; } // Explicit removeNode may already have removed this part.
				const auto objects = found->second.objects;
				for (const auto &[object, offset] : objects) { detachObject(object); } // Keep unrelated objects in world space.
			}
			for (const auto node : owned) {
				const auto found = scene_nodes_.find(node);
				if (found == scene_nodes_.end()) { continue; }
				const auto children = found->second.children;
				for (const auto child : children) {
					if (!owned.contains(child)) { (void)setNodeParent(child, scene_root_); } // Keep its local transform.
				}
			}
			// The ownership map also contains interior subtrees moved outside the synthetic instance root.
			for (const auto node : owned) { (void)removeNode(node); }
			instance_nodes_.erase(nodes);
		}
		scene_instance_sources_.erase(instance);
		scene_instances_.erase(instance);
	}

	/// @brief Releases the render data of an asset scene or a loaded light scene once no scene instance uses it.
	auto RenderSystem::removeScene(SceneHandle handle) -> std::expected<void, Error> {
		if (!handle.valid()) { return std::unexpected(Error::missing_object); }
		if (std::ranges::any_of(scene_instance_sources_, [handle](const auto &source) { return source.second == handle; })) {
			return std::unexpected(Error::invalid_argument);	// Live scene instances keep their asset scene.
		}
		if (scenes_.erase(handle) > 0U) {
			removeImportedLights(detail::lightSceneOwner(handle)); // Lights added after the load stay.
			renderer_.invalidateProbes(true);
			return {};
		}
		if (!imported_assets_.scene_nodes || !imported_assets_.scene_nodes(handle)) { return std::unexpected(Error::missing_object); }

		// Forget the cached render meshes and materials of this scene, then free whatever no object uses anymore.
		for (const auto &[node, mesh, material, world_transform, world] : importedSceneMeshInstances(handle)) {
			imported_render_meshes_.erase(mesh);
			imported_render_materials_.erase(material);
		}
		(void)purgeUnusedAssets();
		renderer_.invalidateProbes(true);
		return {};
	}

	/// @brief Removes CPU render meshes, materials, and textures that are not referenced by live instances.
	auto RenderSystem::purgeUnusedAssets() -> std::size_t {
		const auto removed = scene_.purgeUnusedAssets();
		if (removed > 0U) {
			renderer_.markSceneResourcesDirty();
			renderer_.markMaterialsDirty();
		}
		// Imported caches must not hand out handles of purged resources.
		std::erase_if(imported_render_meshes_, [this](const auto &entry) { return scene_.findMesh(entry.second) == nullptr; });
		std::erase_if(imported_render_materials_, [this](const auto &entry) { return scene_.findMaterial(entry.second) == nullptr; });
		return removed;
	}

	/// @brief Copies one object's material before changing surface factors; shared and imported materials remain unchanged.
	auto RenderSystem::setObjectSurface(RenderObjectHandle handle, SurfaceMaterial surface) -> std::expected<void, Error> {
		if (!std::isfinite(surface.roughness) || !std::isfinite(surface.metalness) || surface.roughness < 0 ||
			surface.roughness > 1 || surface.metalness < 0 || surface.metalness > 1) { return std::unexpected(Error::invalid_argument); }
		const auto object = findRenderObject(handle);
		if (!object) { return std::unexpected(Error::missing_object); }
		auto *instance = scene_.findInstance(*object);
		if (instance == nullptr) { return std::unexpected(Error::missing_object); }
		const auto previous = instance->material;
		auto material = *scene_.findMaterial(previous);
		if (material.roughness == surface.roughness && material.metalness == surface.metalness) { return {}; }
		material.roughness = surface.roughness;
		material.metalness = surface.metalness;
		material.handle = {}; // A private material needs a distinct handle even when copied from an imported material.
		instance->material = scene_.addMaterial(std::move(material));
		releaseUnused(instance->mesh, previous);
		renderer_.markMaterialsDirty();
		return {};
	}

	/// @brief Reuses a primitive material while its colour/texture entry still belongs to the scene.
	auto RenderSystem::acquirePrimitiveMaterial(LinearColor color, RenderTextureIndex texture) -> RenderMaterialHandle {
		const auto key = std::pair{std::array{color.value.x, color.value.y, color.value.z}, texture};
		auto &material = primitive_materials_[key];
		if (scene_.findMaterial(material) == nullptr) {
			material = scene_.addMaterial({.base_color = color, .base_color_texture_index = texture});
			renderer_.markMaterialsDirty();
		}
		return material;
	}

	/// @brief Reuses local primitive geometry; placement and object flags belong to each instance.
	auto RenderSystem::acquirePrimitiveMesh(PrimitiveShape shape, Vec3 minimum, Vec3 maximum,
		Vec2 uv_scale) -> RenderMeshHandle {
		const auto key = std::pair{shape, std::array{minimum.x, minimum.y, minimum.z,
			maximum.x, maximum.y, maximum.z, uv_scale.x, uv_scale.y}};
		auto &mesh = primitive_meshes_[key];
		if (scene_.findMesh(mesh) == nullptr) {
			mesh = shape == PrimitiveShape::plane ? scene_.addPlaneMesh(Vec2{maximum.x, maximum.z}, uv_scale)
				: shape == PrimitiveShape::sprite ? scene_.addSpriteMesh(Vec2{maximum.x, maximum.y})
				: shape == PrimitiveShape::sphere ? scene_.addSphereMesh(maximum.x)
				: scene_.addCuboidMesh(minimum, maximum);
		}
		return mesh;
	}

	/// @brief Adds a plane instance using shared local geometry and colour material.
	auto RenderSystem::addPlane(Vec2 half_extent, LinearColor color, Transform transform)
		-> std::expected<RenderObjectHandle, Error> {
		const auto material = acquirePrimitiveMaterial(color);
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::plane,
			Vec3{-half_extent.x, 0, -half_extent.y}, Vec3{half_extent.x, 0, half_extent.y});
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Adds a cuboid mesh, material, and public object handle to the CPU scene.
	auto RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color, Transform transform)
		-> std::expected<RenderObjectHandle, Error> {
		const auto material = acquirePrimitiveMaterial(color);
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::cuboid, minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Adds one colored indexed triangle mesh to the CPU and backend scenes.
	auto RenderSystem::addTriangleMesh(Vector<Vec3> positions, Vector<std::uint32_t> indices,
		LinearColor color, Transform transform) -> std::expected<RenderObjectHandle, Error> {
		if (positions.size() < 3U || indices.empty() || indices.size() % 3U != 0U ||
			std::ranges::any_of(positions, [](const Vec3 &position) {
				return !std::isfinite(position.x) || !std::isfinite(position.y) ||
					!std::isfinite(position.z);
			}) || std::ranges::any_of(indices, [&positions](std::uint32_t index) {
				return index >= positions.size();
			})) {
			return std::unexpected(Error::invalid_argument);
		}

		const auto material = acquirePrimitiveMaterial(color);
		const auto mesh = scene_.addTriangleMesh(std::move(positions), std::move(indices));
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Adds a coloured sphere; spheres of equal radius share one mesh.
	auto RenderSystem::addSphere(Scalar radius, LinearColor color, Transform transform) -> std::expected<RenderObjectHandle, Error> {
		if (!(radius > zero() && std::isfinite(radius))) { return std::unexpected(Error::invalid_argument); }
		const auto material = acquirePrimitiveMaterial(color);
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::sphere, Vec3{-radius, -radius, -radius}, Vec3{radius, radius, radius});
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Adds a mesh built by game code; it gets its own geometry and a shared plain material.
	auto RenderSystem::addMesh(MeshDescriptor mesh) -> std::expected<RenderObjectHandle, Error> {
		const auto count = mesh.positions.size();
		const auto finite = [](const Vec3 &value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); };
		if (count < 3U || mesh.indices.empty() || mesh.indices.size() % 3U != 0U || !std::ranges::all_of(mesh.positions, finite) ||
			std::ranges::any_of(mesh.indices, [count](std::uint32_t index) { return index >= count; }) ||
			(!mesh.normals.empty() && mesh.normals.size() != count) || (!mesh.uvs.empty() && mesh.uvs.size() != count)) {
			return std::unexpected(Error::invalid_argument);
		}
		auto texture = RenderTextureIndex{kNoTexture};
		if (!mesh.base_color_texture.empty()) {
			const auto acquired = scene_.acquireTexture(mesh.base_color_texture);
			if (!acquired) { return std::unexpected(acquired.error()); }
			texture = *acquired;
		}
		// Zero normals make the shader use face normals; missing UVs stay zero.
		auto vertices = std::make_shared<std::vector<RenderVertex>>(count);
		for (std::size_t index{}; index < count; ++index) {
			(*vertices)[index] = RenderVertex{.position = mesh.positions[index],
				.normal = mesh.normals.empty() ? zeroVec3() : mesh.normals[index], .uv = mesh.uvs.empty() ? Vec2{} : mesh.uvs[index]};
		}
		const auto material = acquirePrimitiveMaterial(mesh.color, texture);
		const auto render_mesh = scene_.addMesh(std::move(vertices), std::move(mesh.indices));
		auto instance = scene_.addInstance(render_mesh, material, mesh.transform, detail::modelMatrix(mesh.transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Builds a fixed grid once; two shader waves animate it without CPU uploads or extra resource owners.
	auto RenderSystem::addOcean(OceanDescriptor ocean) -> std::expected<RenderObjectHandle, Error> {
		const auto c = ocean.color.value;
		const auto p = ocean.position.value;
		if (!std::isfinite(ocean.half_extent.x) || !std::isfinite(ocean.half_extent.y) || ocean.half_extent.x <= 0 || ocean.half_extent.y <= 0 ||
			!std::isfinite(ocean.wave_height) || ocean.wave_height < 0 || !std::isfinite(ocean.wavelength) || ocean.wavelength <= 0 ||
			!std::isfinite(ocean.speed) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
			!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || c.x < 0 || c.y < 0 || c.z < 0 ||
			ocean.columns == 0 || ocean.columns > 1024 || ocean.rows == 0 || ocean.rows > 1024) {
			return std::unexpected(Error::invalid_argument);
		}
		const std::uint32_t columns = ocean.columns, rows = ocean.rows, stride = columns + 1;
		MeshDescriptor mesh{.color = ocean.color, .transform = Transform{.translation = ocean.position}};
		mesh.positions.reserve(std::size_t{stride} * (rows + 1));
		mesh.normals.reserve(std::size_t{stride} * (rows + 1));
		mesh.indices.reserve(std::size_t{columns} * rows * 6);
		// Geometry is static; the shader evaluates world-space wave heights and exact normal derivatives.
		for (const auto z : std::views::iota(0U, rows + 1)) {
			for (const auto x : std::views::iota(0U, stride)) {
				mesh.positions.push_back({(2.0F * x / columns - 1) * ocean.half_extent.x, 0, (2.0F * z / rows - 1) * ocean.half_extent.y});
				mesh.normals.push_back({0, 1, 0});
				if (x < columns && z < rows) {
					const auto a = z * stride + x;
					mesh.indices.insert(mesh.indices.end(), {a, a + stride, a + 1, a + 1, a + stride, a + stride + 1});
				}
			}
		}
		return addWater(std::move(mesh), ocean);
	}

	/// @brief Turns validated game geometry into a water object: shader waves, smooth water material, no shadow casting.
	auto RenderSystem::addWater(MeshDescriptor surface, OceanDescriptor waves) -> std::expected<RenderObjectHandle, Error> {
		const auto c = waves.color.value;
		if (!std::isfinite(waves.wave_height) || waves.wave_height < 0 || !std::isfinite(waves.wavelength) || waves.wavelength <= 0 ||
			!std::isfinite(waves.speed) || !std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || c.x < 0 || c.y < 0 || c.z < 0) {
			return std::unexpected(Error::invalid_argument);
		}
		surface.color = waves.color;
		const auto object = addMesh(std::move(surface));
		if (!object) { return object; }
		auto *instance = scene_.findInstance(*findRenderObject(*object));
		const auto previous = instance->material;
		instance->material = scene_.addMaterial({.base_color = waves.color, .roughness = 0.08F});
		instance->waves = {waves.wave_height, waves.wavelength, waves.speed, 1};
		instance->casts_shadow = false;
		releaseUnused(instance->mesh, previous);
		renderer_.markMaterialsDirty();
		return object;
	}

	/**
	 * @brief Builds a coarse grid of 4-corner patches; the tessellation stages add detail from the shared heights.
	 *
	 * Each patch spans up to 32 cells (VVE_TERRAIN_PATCH_CELLS), so its finest tessellation still has one height sample per
	 * vertex. Corner vertices carry their real heights and two unreferenced vertices widen the culling box to the full
	 * height range between corners.
	 */
	auto RenderSystem::addTerrain(TerrainDescriptor terrain) -> std::expected<RenderObjectHandle, Error> {
		const auto columns = terrain.columns, rows = terrain.rows;
		const auto c = terrain.color.value;
		if (columns < 2U || rows < 2U || columns > 65535U || rows > 65535U ||
			terrain.heights.size() != static_cast<std::size_t>(columns) * rows ||
			!std::isfinite(terrain.cell_size) || terrain.cell_size <= 0 ||
			!std::ranges::all_of(terrain.heights, [](float height) { return std::isfinite(height); }) ||
			!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || c.x < 0 || c.y < 0 || c.z < 0) {
			return std::unexpected(Error::invalid_argument);
		}
		auto texture = RenderTextureIndex{kNoTexture};
		if (!terrain.base_color_texture.empty()) {
			const auto acquired = scene_.acquireTexture(terrain.base_color_texture);
			if (!acquired) { return std::unexpected(acquired.error()); }
			texture = *acquired;
		}
		constexpr std::uint32_t patch_cells = 32U; // Mirrors VVE_TERRAIN_PATCH_CELLS in shaders/simple_shared.h.
		const auto corners = [](std::uint32_t samples) {
			Vector<std::uint32_t> result{};
			for (std::uint32_t sample{}; sample + 1U < samples; sample += patch_cells) { result.push_back(sample); }
			result.push_back(samples - 1U);
			return result;
		};
		const auto xs = corners(columns), zs = corners(rows);
		const auto height = [&](std::uint32_t x, std::uint32_t z) { return terrain.heights[static_cast<std::size_t>(z) * columns + x]; };
		auto vertices = std::make_shared<std::vector<RenderVertex>>();
		vertices->reserve(xs.size() * zs.size() + 2U);
		for (const auto z : zs) {
			for (const auto x : xs) {
				vertices->push_back(RenderVertex{.position = Vec3{static_cast<Scalar>(x) * terrain.cell_size, height(x, z), static_cast<Scalar>(z) * terrain.cell_size},
					.uv = Vec2{static_cast<Scalar>(x) / static_cast<Scalar>(columns - 1U), static_cast<Scalar>(z) / static_cast<Scalar>(rows - 1U)}});
			}
		}
		const auto [low, high] = std::ranges::minmax(terrain.heights);
		vertices->push_back(RenderVertex{.position = Vec3{zero(), low, zero()}});
		vertices->push_back(RenderVertex{.position = Vec3{zero(), high, zero()}});
		const auto stride = static_cast<std::uint32_t>(xs.size());
		auto indices = Vector<std::uint32_t>{};
		indices.reserve((xs.size() - 1U) * (zs.size() - 1U) * 4U);
		for (std::uint32_t row{}; row + 1U < zs.size(); ++row) {
			for (std::uint32_t column{}; column + 1U < xs.size(); ++column) {
				const auto a = row * stride + column; // Patch corners (u0,v0), (u1,v0), (u1,v1), (u0,v1).
				indices.insert(indices.end(), {a, a + 1U, a + stride + 1U, a + stride});
			}
		}
		const auto material = acquirePrimitiveMaterial(terrain.color, texture);
		const auto render_mesh = scene_.addMesh(std::move(vertices), std::move(indices));
		auto instance = scene_.addInstance(render_mesh, material, terrain.transform, detail::modelMatrix(terrain.transform));
		if (!instance) { return std::unexpected(instance.error()); }
		auto *created = scene_.findInstance(*instance);
		if (created == nullptr) { return std::unexpected(Error::internal_error); }
		created->terrain = std::make_shared<const RenderTerrain>(RenderTerrain{.id = ++terrain_ids_, .heights = std::move(terrain.heights),
			.columns = columns, .rows = rows, .cell_size = terrain.cell_size});
		return registerRenderObject(*instance);
	}

	/// @brief Updates positions while preserving one triangle mesh's topology and allocation size.
	auto RenderSystem::setObjectMeshPositions(RenderObjectHandle handle, Vector<Vec3> positions)
		-> std::expected<void, Error> {
		const auto object = findRenderObject(handle);
		if (!object) { return std::unexpected(Error::missing_object); }
		auto *instance = scene_.findInstance(*object);
		if (instance == nullptr) { return std::unexpected(Error::missing_object); }
		const auto *source_mesh = scene_.findMesh(instance->mesh);
		if (source_mesh == nullptr || source_mesh->vertices->size() != positions.size() ||
			std::ranges::any_of(positions, [](const Vec3 &position) {
				return !std::isfinite(position.x) || !std::isfinite(position.y) ||
					!std::isfinite(position.z);
			})) {
			return std::unexpected(Error::invalid_argument);
		}

		// Shared primitive and imported meshes need a private copy before editing this object's vertices.
		const bool shared = std::ranges::count(scene_.instances(), instance->mesh, &RenderInstance::mesh) > 1 ||
			std::ranges::any_of(imported_render_meshes_, [instance](const auto &entry) { return entry.second == instance->mesh; });
		if (shared) {
			instance->mesh = scene_.addMesh(source_mesh->vertices, source_mesh->indices);
			renderer_.markSceneResourcesDirty();
		}
		// addMesh may reallocate meshes_; the instance is separate and the mesh is looked up again.
		auto *mesh = scene_.findMesh(instance->mesh);
		if (mesh == nullptr) { return std::unexpected(Error::internal_error); }
		// A sole primitive owner may edit in place, but future primitives must retain their authored shape.
		std::erase_if(primitive_meshes_, [instance](const auto &entry) { return entry.second == instance->mesh; });

		// Imported meshes were cloned above; unique buffers originate from mutable primitive or edit allocations.
		if (mesh->vertices.use_count() > 1) { mesh->vertices = std::make_shared<std::vector<RenderVertex>>(*mesh->vertices); }
		auto vertices = std::const_pointer_cast<std::vector<RenderVertex>>(mesh->vertices);

		// Update positions only; the GPU mesh refreshes its culling box when these vertices are uploaded.
		for (std::size_t index{}; index < positions.size(); ++index) {
			const Vec3 &position = positions[index];
			(*vertices)[index].position = position;
		}

		renderer_.markMeshDirty(instance->mesh);
		return {};
	}

	/// @brief Gives one object a new base colour; its base-colour texture stays.
	auto RenderSystem::setObjectColor(RenderObjectHandle handle, LinearColor color) -> std::expected<void, Error> {
		return setObjectLook(handle, color, std::nullopt);
	}

	/// @brief Gives one object a new base-colour texture; its base colour stays.
	auto RenderSystem::setObjectTexture(RenderObjectHandle handle, std::filesystem::path texture) -> std::expected<void, Error> {
		if (!findRenderObject(handle)) { return std::unexpected(Error::missing_object); }
		const auto texture_index = scene_.acquireTexture(texture);
		if (!texture_index) { return std::unexpected(texture_index.error()); }
		return setObjectLook(handle, std::nullopt, *texture_index);
	}

	/// @brief Switches an object to the shared plain material for a colour and texture slot; missing values keep the current ones.
	///
	/// An imported object loses its other PBR values; the old material is released when nothing uses it anymore.
	auto RenderSystem::setObjectLook(RenderObjectHandle handle, std::optional<LinearColor> color,
		std::optional<RenderTextureIndex> texture) -> std::expected<void, Error> {
		const auto object = findRenderObject(handle);
		auto *instance = object ? scene_.findInstance(*object) : nullptr;
		const auto *current = instance ? scene_.findMaterial(instance->material) : nullptr;
		if (current == nullptr) { return std::unexpected(Error::missing_object); }
		const auto old_material = instance->material;
		const bool shadowChange = instance->casts_shadow && current->alpha_mode != AlphaMode::opaque; // Replacing mask/blend with a plain material changes shadow geometry.
		instance->material = acquirePrimitiveMaterial(color.value_or(current->base_color), texture.value_or(current->base_color_texture_index));
		if (instance->material != old_material) { releaseUnused(instance->mesh, old_material); renderer_.invalidateProbes(false, shadowChange); }
		return {};
	}

	/// @brief Adds one textured XZ quad with shared geometry keyed by extents and UV scale.
	auto RenderSystem::addTexturedPlane(Vec2 half_extent, std::filesystem::path base_color_texture,
		Vec2 uv_scale, Transform transform) -> std::expected<RenderObjectHandle, Error> {
		if (!std::isfinite(uv_scale.x) || !std::isfinite(uv_scale.y)) {
			return std::unexpected(Error::invalid_argument);
		}
		const auto texture_index = scene_.acquireTexture(base_color_texture);
		if (!texture_index) { return std::unexpected(texture_index.error()); }

		const auto material = acquirePrimitiveMaterial(LinearColor{.value = oneVec3()}, *texture_index);
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::plane,
			Vec3{-half_extent.x, 0, -half_extent.y}, Vec3{half_extent.x, 0, half_extent.y}, uv_scale);
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Adds a textured cuboid and returns its public render-object handle.
	auto RenderSystem::addTexturedCuboid(Vec3 minimum, Vec3 maximum, std::filesystem::path base_color_texture,
												 Transform transform) -> std::expected<RenderObjectHandle, Error> {
		const auto texture_index = scene_.acquireTexture(base_color_texture);
		if (!texture_index) { return std::unexpected(texture_index.error()); } // io_error, or capacity_exceeded when all texture slots are used.

		const auto material = acquirePrimitiveMaterial(LinearColor{.value = oneVec3()}, *texture_index);
		const auto mesh = acquirePrimitiveMesh(PrimitiveShape::cuboid, minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform, detail::modelMatrix(transform));
		if (!instance) { return std::unexpected(instance.error()); }
		return registerRenderObject(*instance);
	}

	/// @brief Removes all objects, scene instances, lights, cameras, and CPU render resources.
	auto RenderSystem::clearScene() -> void {
		renderer_.clearScene();
		scene_.clear();
		render_objects_.clear();
		object_sources_.clear();
		scene_instances_.clear();
		scene_instance_sources_.clear();
		scene_nodes_.clear();
		scene_nodes_.emplace(scene_root_, SceneNode{});	// The world root survives with the same handle.
		instance_nodes_.clear();
		object_nodes_.clear();
		scenes_.clear();
		imported_render_meshes_.clear();
		imported_render_materials_.clear();
		primitive_materials_.clear();
		primitive_meshes_.clear();
		default_material_ = {};
		sky_ = {};
		renderer_.setEnvironmentTexture(kNoTexture);
		renderer_.markSceneResourcesDirty();
		renderer_.markMaterialsDirty();
	}

	/// @brief Replaces all lights with the lights of a backend scene; objects and textures stay.
	auto RenderSystem::loadScene(Scene scene) -> SceneHandle {
		if (scene.directionalLights.size() > kMaxDirectionalLights) { scene.directionalLights.resize(kMaxDirectionalLights); }
		if (scene.pointLights.size() > kMaxShadowedPointLights) { scene.pointLights.resize(kMaxShadowedPointLights); }
		if (scene.spotLights.size() > kMaxShadowedSpotLights) { scene.spotLights.resize(kMaxShadowedSpotLights); }

		const auto handle = makeCounterHandle<SceneHandle>();
		const auto owner = detail::lightSceneOwner(handle); // removeScene(handle) removes exactly these lights.
		for (auto &light : scene.directionalLights) { light.owner = owner; }
		for (auto &light : scene.pointLights) { light.owner = owner; }
		for (auto &light : scene.spotLights) { light.owner = owner; }

		scenes_.insert(handle);
		renderer_.loadScene(std::move(scene));
		return handle;
	}

} // namespace vve::simple
