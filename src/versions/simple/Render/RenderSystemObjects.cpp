module VEEngine.Simple;
import std;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;

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
		// Imported caches retain their resources; primitive and edited resources live until their last instance.
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
		eraseRenderObject(handle);
		if (const auto source = object_sources_.find(handle); source != object_sources_.end()) {
			if (const auto instance = scene_instances_.find(source->second.first); instance != scene_instances_.end()) {
				auto &objects = instance->second;
				if (const auto entry = std::ranges::find(objects, handle); entry != objects.end()) { objects.erase(entry); }
			}
			object_sources_.erase(source);
		}
		renderer_.markSceneResourcesDirty();
		return {};
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
		return {};
	}

	/// @brief Removes everything one scene instance created; also undoes a partly built instance.
	auto RenderSystem::rollbackSceneInstance(RenderSceneInstanceHandle instance) -> void {
		if (const auto found = scene_instances_.find(instance); found != scene_instances_.end()) {
			const auto objects = found->second; // removeObject edits the stored list.
			for (const auto object : objects) { (void)removeObject(object); }
		}
		removeImportedLights(instance.value);
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
			return {};
		}
		if (importedSceneNodes(handle).empty()) { return std::unexpected(Error::missing_object); }

		// Forget the cached render meshes and materials of this scene, then free whatever no object uses anymore.
		for (const auto &[node, mesh, material, world_transform, world] : importedSceneMeshInstances(handle)) {
			imported_render_meshes_.erase(mesh);
			imported_render_materials_.erase(material);
		}
		(void)purgeUnusedAssets();
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
		scenes_.clear();
		imported_render_meshes_.clear();
		imported_render_materials_.clear();
		primitive_materials_.clear();
		primitive_meshes_.clear();
		default_material_ = {};
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
