export module VEEngine.Simple:RenderSystemObjects;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;
import :Types;

/**
	* @file
	* @brief CPU-scene object and scene-lifecycle mutation for the simple render system.
	*
	* Functional objects:
	* - RenderSystemObjects creates primitive CPU render objects, removes live objects, manages loaded backend scenes, and answers scene-instance source queries.
	*
	* Chosen cluster: CPU-scene object/lifecycle mutation helpers. Imported-asset acquisition remains in RenderSceneImport because it is shared by scene instantiation.
	*/
export namespace vve::simple {

	/// @brief CRTP mixin that owns public render-object and loaded-scene lifecycle mutation.
	template<typename System>
	struct RenderSystemObjects {
		/// @brief Removes one live render object from the backend scene.
		[[nodiscard]] auto removeObject(RenderObjectHandle handle) -> std::expected<void, Error> {
			const auto object = system().findRenderObject(handle);
			if (!object || system().scene_.findInstance(object->first) == nullptr ||
				 object->second >= system().forward().scene.objects.size()) {
				return std::unexpected(Error::missing_object);
			}

			system().forward().scene.objects.erase(system().forward().scene.objects.begin() + static_cast<std::ptrdiff_t>(object->second));
			if (!system().scene_.eraseInstance(object->first)) { return std::unexpected(Error::missing_object); }
			system().eraseRenderObject(handle);
			for (auto &entry : system().render_objects_) {
				if (entry.second.second > object->second) { --entry.second.second; }
			}
			return {};
		}

		/// @brief Returns public render objects registered for one scene instance.
		[[nodiscard]] auto sceneInstanceObjects(RenderSceneInstanceHandle instance) const
			-> std::expected<Vector<RenderObjectHandle>, Error> {
			const auto found = system().scene_instances_.find(instance);
			return found == system().scene_instances_.end() ? std::unexpected(Error::missing_object) :
																		std::expected<Vector<RenderObjectHandle>, Error>{found->second};
		}

		/// @brief Returns the public scene instance that created one render object.
		[[nodiscard]] auto objectSourceScene(RenderObjectHandle handle) const
			-> std::expected<RenderSceneInstanceHandle, Error> {
			const auto found = system().object_sources_.find(handle);
			return found == system().object_sources_.end() ? std::unexpected(Error::missing_object) :
																	  std::expected<RenderSceneInstanceHandle, Error>{found->second.first};
		}

		/// @brief Returns the source asset-scene node that created one render object.
		[[nodiscard]] auto objectSourceNode(RenderObjectHandle handle) const -> std::expected<NodeHandle, Error> {
			const auto found = system().object_sources_.find(handle);
			return found == system().object_sources_.end() ? std::unexpected(Error::missing_object) :
																	  std::expected<NodeHandle, Error>{found->second.second};
		}

		/// @brief Removes a public scene instance and all render objects it created.
		[[nodiscard]] auto removeSceneInstance(RenderSceneInstanceHandle instance) -> std::expected<void, Error> {
			const auto found = system().scene_instances_.find(instance);
			if (found == system().scene_instances_.end()) { return std::unexpected(Error::missing_object); }
			const auto objects = found->second;

			// Reuse the single-object teardown so backend indices and CPU instances stay consistent.
			for (const auto object : objects) {
				if (auto removed = removeObject(object); !removed) { return std::unexpected(removed.error()); }
				system().object_sources_.erase(object);
			}
			system().scene_instance_sources_.erase(instance);
			system().scene_instances_.erase(instance);
			return {};
		}

		/// @brief Removes a loaded backend scene only when no public render objects are still live.
		[[nodiscard]] auto removeScene(SceneHandle handle) -> std::expected<void, Error> {
			if (std::ranges::any_of(system().scene_instance_sources_, [handle](const auto &source) {
					return source.second == handle;
				})) {
				return std::unexpected(Error::invalid_argument);
			}
			const auto found = system().scenes_.find(handle);
			if (!handle.valid() || found == system().scenes_.end()) { return std::unexpected(Error::missing_object); }
			if (!system().render_objects_.empty()) { return std::unexpected(Error::invalid_argument); }	// Live objects keep scene ownership explicit.
			system().scenes_.erase(found);
			if (system().active_scene_ == handle) {
				system().active_scene_.reset();
				system().forward().clearScene();
			}
			return {};
		}

		/// @brief Removes CPU render assets that are not referenced by live instances.
		[[nodiscard]] auto purgeUnusedAssets() -> std::size_t {
			return system().scene_.purgeUnusedAssets();
		}

		/// @brief Adds a plane mesh, material, and public object handle to the CPU scene.
		[[nodiscard]] auto addPlane(Vec2 half_extent, LinearColor color, Transform transform = {})
			-> std::expected<RenderObjectHandle, Error> {
			const auto material = system().scene_.addMaterial({.base_color = color});
			const auto mesh = system().scene_.addPlaneMesh(half_extent);
			auto instance = system().scene_.addInstance(mesh, material, transform);
			if (!instance) { return std::unexpected(instance.error()); }
			const auto backend_index = system().appendBackendObject(*instance);
			if (!backend_index) { return std::unexpected(backend_index.error()); }
			return system().registerRenderObject(*instance, *backend_index);
		}

		/// @brief Adds a cuboid mesh, material, and public object handle to the CPU scene.
		[[nodiscard]] auto addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color, Transform transform = {})
			-> std::expected<RenderObjectHandle, Error> {
			const auto material = system().scene_.addMaterial({.base_color = color});
			const auto mesh = system().scene_.addCuboidMesh(minimum, maximum);
			auto instance = system().scene_.addInstance(mesh, material, transform);
			if (!instance) { return std::unexpected(instance.error()); }
			const auto backend_index = system().appendBackendObject(*instance);
			if (!backend_index) { return std::unexpected(backend_index.error()); }
			return system().registerRenderObject(*instance, *backend_index);
		}

		/// @brief Adds a textured cuboid and returns its public render-object handle.
		[[nodiscard]] auto addTexturedCuboid(Vec3 minimum, Vec3 maximum, std::filesystem::path base_color_texture,
														 Transform transform = {}) -> std::expected<RenderObjectHandle, Error> {
			if (base_color_texture.empty()) { return std::unexpected(Error::io_error); }
			auto texture_file = std::ifstream{base_color_texture, std::ios::binary};
			if (!texture_file) { return std::unexpected(Error::io_error); }

			const auto material = system().scene_.addMaterial({.base_color = LinearColor{.value = oneVec3()},
																				.base_color_texture = makeCounterHandle<TextureHandle>(),
																				.base_color_texture_source = base_color_texture});
			const auto mesh = system().scene_.addCuboidMesh(minimum, maximum);
			auto instance = system().scene_.addInstance(mesh, material, transform);
			if (!instance) { return std::unexpected(instance.error()); }
			const auto backend_index = system().appendBackendObject(*instance);
			if (!backend_index) { return std::unexpected(backend_index.error()); }
			return system().registerRenderObject(*instance, *backend_index);
		}

		/// @brief Removes all CPU-scene instances and backend object payloads.
		auto clearScene() -> void {
			system().scene_.clear();
			system().render_objects_.clear();
			system().forward().scene.objects.clear();
			system().forward().scene.baseColorTexture.reset();
		}

		/// @brief Stores a backend scene and mirrors it into the selected renderer.
		auto loadScene(Scene scene) -> SceneHandle {
			if (scene.directionalLights.size() > kMaxDirectionalLights) { scene.directionalLights.resize(kMaxDirectionalLights); }
			if (!scene.directionalLights.empty()) { scene.directionalLight = scene.directionalLights.front(); }	// Forward renderer still consumes the first directional light only.
			if (scene.spotLights.size() > kMaxShadowedSpotLights) { scene.spotLights.resize(kMaxShadowedSpotLights); }
			if (!scene.spotLights.empty()) { scene.spotLight = scene.spotLights.front(); }				// Forward renderer still consumes the first spot light only.
			const auto handle = makeCounterHandle<SceneHandle>();
			system().scenes_[handle] = scene;
			system().active_scene_ = handle;
			system().forward().loadScene(std::move(scene));
			return handle;
		}

	private:
		/// @brief Returns the owning render system for dependent member access.
		[[nodiscard]] auto system() -> System & { return static_cast<System &>(*this); }
		/// @brief Returns the owning render system for const dependent member access.
		[[nodiscard]] auto system() const -> const System & { return static_cast<const System &>(*this); }
	};

} // namespace vve::simple
