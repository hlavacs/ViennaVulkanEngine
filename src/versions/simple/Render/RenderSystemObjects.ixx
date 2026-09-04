export module VEEngine.Simple:RenderSystemObjects;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;
import :RenderSystem;

/// @file
/// @brief RenderSystem definitions that create primitive render objects, remove live objects, and manage loaded backend scenes.

namespace vve::simple {

	/// @brief Removes one live render object from the backend scene.
	auto RenderSystem::removeObject(RenderObjectHandle handle) -> std::expected<void, Error> {
		const auto object = findRenderObject(handle);
		if (!object || scene_.findInstance(object->first) == nullptr ||
			 object->second >= renderer_.scene.objects.size()) {
			return std::unexpected(Error::missing_object);
		}

		if (!renderer_.removeObject(object->second)) { return std::unexpected(Error::missing_object); }
		if (!scene_.eraseInstance(object->first)) { return std::unexpected(Error::missing_object); }
		eraseRenderObject(handle);
		for (auto &entry : render_objects_) {
			if (entry.second.second > object->second) { --entry.second.second; }
		}
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

	/// @brief Removes a public scene instance and all render objects it created.
	auto RenderSystem::removeSceneInstance(RenderSceneInstanceHandle instance) -> std::expected<void, Error> {
		const auto found = scene_instances_.find(instance);
		if (found == scene_instances_.end()) { return std::unexpected(Error::missing_object); }
		const auto objects = found->second;

		// Reuse the single-object teardown so backend indices and CPU instances stay consistent.
		for (const auto object : objects) {
			if (auto removed = removeObject(object); !removed) { return std::unexpected(removed.error()); }
			object_sources_.erase(object);
		}
		scene_instance_sources_.erase(instance);
		scene_instances_.erase(instance);
		return {};
	}

	/// @brief Removes a loaded backend scene only when no public render objects are still live.
	auto RenderSystem::removeScene(SceneHandle handle) -> std::expected<void, Error> {
		if (std::ranges::any_of(scene_instance_sources_, [handle](const auto &source) {
				return source.second == handle;
			})) {
			return std::unexpected(Error::invalid_argument);
		}
		const auto found = scenes_.find(handle);
		if (!handle.valid() || found == scenes_.end()) { return std::unexpected(Error::missing_object); }
		if (!render_objects_.empty()) { return std::unexpected(Error::invalid_argument); }	// Live objects keep scene ownership explicit.
		scenes_.erase(found);
		if (active_scene_ == handle) {
			active_scene_.reset();
			renderer_.clearScene();
		}
		return {};
	}

	/// @brief Removes CPU render assets that are not referenced by live instances.
	auto RenderSystem::purgeUnusedAssets() -> std::size_t {
		return scene_.purgeUnusedAssets();
	}

	/// @brief Adds a plane mesh, material, and public object handle to the CPU scene.
	auto RenderSystem::addPlane(Vec2 half_extent, LinearColor color, Transform transform)
		-> std::expected<RenderObjectHandle, Error> {
		const auto material = scene_.addMaterial({.base_color = color});
		const auto mesh = scene_.addPlaneMesh(half_extent);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		const auto backend_index = appendBackendObject(*instance);
		if (!backend_index) { return std::unexpected(backend_index.error()); }
		return registerRenderObject(*instance, *backend_index);
	}

	/// @brief Adds a cuboid mesh, material, and public object handle to the CPU scene.
	auto RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color, Transform transform)
		-> std::expected<RenderObjectHandle, Error> {
		const auto material = scene_.addMaterial({.base_color = color});
		const auto mesh = scene_.addCuboidMesh(minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		const auto backend_index = appendBackendObject(*instance);
		if (!backend_index) { return std::unexpected(backend_index.error()); }
		return registerRenderObject(*instance, *backend_index);
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

		Vec3 minimum = positions.front();
		Vec3 maximum = minimum;
		for (const Vec3 &position : positions) {
			minimum.x = std::min(minimum.x, position.x);
			minimum.y = std::min(minimum.y, position.y);
			minimum.z = std::min(minimum.z, position.z);
			maximum.x = std::max(maximum.x, position.x);
			maximum.y = std::max(maximum.y, position.y);
			maximum.z = std::max(maximum.z, position.z);
		}

		const auto material = scene_.addMaterial({.base_color = color});
		const auto mesh = scene_.addTriangleMesh(std::move(positions), std::move(indices),
			Bounds{.minimum = Position{.value = minimum}, .maximum = Position{.value = maximum},
				.valid = true});
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		const auto backend_index = appendBackendObject(*instance);
		if (!backend_index) { return std::unexpected(backend_index.error()); }
		return registerRenderObject(*instance, *backend_index);
	}

	/// @brief Updates positions while preserving one triangle mesh's topology and allocation size.
	auto RenderSystem::setObjectMeshPositions(RenderObjectHandle handle, Vector<Vec3> positions)
		-> std::expected<void, Error> {
		const auto object = findRenderObject(handle);
		if (!object) { return std::unexpected(Error::missing_object); }
		auto *instance = scene_.findInstance(object->first);
		if (instance == nullptr) { return std::unexpected(Error::missing_object); }
		auto *mesh = scene_.findMesh(instance->mesh);
		if (mesh == nullptr || mesh->vertices.size() != positions.size() ||
			std::ranges::any_of(positions, [](const Vec3 &position) {
				return !std::isfinite(position.x) || !std::isfinite(position.y) ||
					!std::isfinite(position.z);
			})) {
			return std::unexpected(Error::invalid_argument);
		}

		Vec3 minimum = positions.front();
		Vec3 maximum = minimum;
		for (std::size_t index{}; index < positions.size(); ++index) {
			const Vec3 &position = positions[index];
			mesh->vertices[index].position = position;
			minimum.x = std::min(minimum.x, position.x);
			minimum.y = std::min(minimum.y, position.y);
			minimum.z = std::min(minimum.z, position.z);
			maximum.x = std::max(maximum.x, position.x);
			maximum.y = std::max(maximum.y, position.y);
			maximum.z = std::max(maximum.z, position.z);
		}
		mesh->bounds = Bounds{.minimum = Position{.value = minimum},
			.maximum = Position{.value = maximum}, .valid = true};

		const bool updated = renderer_.updateObjectMeshPositions(object->second, positions);
		return updated ? std::expected<void, Error>{} :
			std::unexpected(Error::invalid_argument);
	}

	/// @brief Adds a textured cuboid and returns its public render-object handle.
	auto RenderSystem::addTexturedCuboid(Vec3 minimum, Vec3 maximum, std::filesystem::path base_color_texture,
													 Transform transform) -> std::expected<RenderObjectHandle, Error> {
		if (base_color_texture.empty()) { return std::unexpected(Error::io_error); }
		auto texture_file = std::ifstream{base_color_texture, std::ios::binary};
		if (!texture_file) { return std::unexpected(Error::io_error); }

		const auto material = scene_.addMaterial({.base_color = LinearColor{.value = oneVec3()},
																.base_color_texture = makeCounterHandle<TextureHandle>(),
																.base_color_texture_source = base_color_texture});
		const auto mesh = scene_.addCuboidMesh(minimum, maximum);
		auto instance = scene_.addInstance(mesh, material, transform);
		if (!instance) { return std::unexpected(instance.error()); }
		const auto backend_index = appendBackendObject(*instance);
		if (!backend_index) { return std::unexpected(backend_index.error()); }
		return registerRenderObject(*instance, *backend_index);
	}

	/// @brief Removes all CPU-scene instances and backend object payloads.
	auto RenderSystem::clearScene() -> void {
		scene_.clear();
		render_objects_.clear();
		renderer_.clearScene();
	}

	/// @brief Stores a backend scene and mirrors it into the selected renderer.
	auto RenderSystem::loadScene(Scene scene) -> SceneHandle {
		if (scene.directionalLights.size() > kMaxDirectionalLights) { scene.directionalLights.resize(kMaxDirectionalLights); }
		if (scene.spotLights.size() > kMaxShadowedSpotLights) { scene.spotLights.resize(kMaxShadowedSpotLights); }
		const auto handle = makeCounterHandle<SceneHandle>();
		scenes_[handle] = scene;
		active_scene_ = handle;
		renderer_.loadScene(std::move(scene));
		return handle;
	}

} // namespace vve::simple
