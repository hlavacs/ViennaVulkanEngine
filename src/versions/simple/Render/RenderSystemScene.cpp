module VEEngine.Simple;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;

/// @file
/// @brief RenderSystem definitions that mirror object state, cameras, and lights into the forward renderer CPU Scene.

namespace vve::simple {

	/// @brief Sets whether one live render object is drawn in its flat base color without lighting.
	auto RenderSystem::setObjectUnlit(RenderObjectHandle handle, bool unlit) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		if (instance->second >= renderer_.scene.objects.size()) { return std::unexpected(Error::missing_object); }
		renderer_.scene.objects[instance->second].unlit = unlit;
		return {};
	}

	/// @brief Sets whether one live render object is drawn into shadow depth passes.
	auto RenderSystem::setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		if (instance->second >= renderer_.scene.objects.size()) { return std::unexpected(Error::missing_object); }
		renderer_.scene.objects[instance->second].castsShadow = casts_shadow;
		return {};
	}

	/// @brief Sets whether one live render object participates in future backend uploads.
	auto RenderSystem::setObjectVisible(RenderObjectHandle handle, bool visible) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(instance->first);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		if (instance->second >= renderer_.scene.objects.size()) { return std::unexpected(Error::missing_object); }
		// Keep the CPU scene and renderer-visible backend scene in the same visibility state.
		scene_instance->visible = visible;
		renderer_.scene.objects[instance->second].visible = visible;
		return {};
	}

	/// @brief Returns whether one live render object is marked visible.
	auto RenderSystem::objectVisible(RenderObjectHandle handle) const -> std::expected<bool, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		const auto *scene_instance = scene_.findInstance(instance->first);
		return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
													 std::expected<bool, Error>{scene_instance->visible};
	}

	/// @brief Sets the source transform for one live render object.
	auto RenderSystem::setObjectTransform(RenderObjectHandle handle, Transform transform) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(instance->first);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		if (instance->second >= renderer_.scene.objects.size()) { return std::unexpected(Error::missing_object); }
		// Keep the CPU scene and renderer-visible backend scene in the same world transform.
		scene_instance->local_transform = transform;
		scene_instance->world_transform = detail::modelMatrix(transform);
		renderer_.scene.objects[instance->second].model = scene_instance->world_transform;
		return {};
	}

	/// @brief Returns the source transform for one live render object.
	auto RenderSystem::objectTransform(RenderObjectHandle handle) const -> std::expected<Transform, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		const auto *scene_instance = scene_.findInstance(instance->first);
		return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
													 std::expected<Transform, Error>{scene_instance->local_transform};
	}

	/// @brief Mirrors one camera into the renderer CPU scene and retained render-scene data.
	auto RenderSystem::setCamera(Camera camera, PixelExtent extent) -> void {
		const auto eye = camera.position.value;
		const auto target = math::add(eye, camera.forward.value);
		renderer_.setCamera(eye, target);
		scene_.setCamera({.camera = std::move(camera), .target_extent = extent});
	}

	/// @brief Replaces the active directional-light list with one renderer light.
	auto RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const DirectionalLight light{
			.direction = direction_to_light.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		renderer_.scene.directionalLights.clear();
		renderer_.scene.directionalLights.push_back(light);
		scene_.setDirectionalLight({.direction_to_light = direction_to_light,
											 .color = color, .intensity = intensity, .ambient = ambient});
	}

	/// @brief Appends one directional light to the capped renderer light list.
	auto RenderSystem::addDirectionalLight(Direction direction_to_light, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const DirectionalLight light{
			.direction = direction_to_light.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		if (renderer_.scene.directionalLights.size() < kMaxDirectionalLights) {
			renderer_.scene.directionalLights.push_back(light);
		}
		scene_.addDirectionalLight({.direction_to_light = direction_to_light,
											 .color = color, .intensity = intensity, .ambient = ambient});
	}

	/// @brief Replaces the active point-light list using the current ambient fallback.
	auto RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) -> void {
		const PointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = renderer_.scene.ambient};
		renderer_.scene.pointLights.clear();
		renderer_.scene.pointLights.push_back(light);
		renderer_.scene.ambient = light.ambient;
		scene_.setPointLight({.position = position, .color = color,
									.intensity = intensity, .range = range});
	}

	/// @brief Replaces the active point-light list using an explicit ambient term.
	auto RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity,
											 LightRange range, LinearColor ambient) -> void {
		const PointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = ambient.value.x};
		renderer_.scene.pointLights.clear();
		renderer_.scene.pointLights.push_back(light);
		renderer_.scene.ambient = light.ambient;
		scene_.setPointLight({.position = position, .color = color,
									.intensity = intensity, .range = range, .ambient = ambient});
	}

	/// @brief Appends one point light to the capped renderer light list using default ambient.
	auto RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) -> void {
		const PointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = PointLight{}.ambient};
		if (renderer_.scene.pointLights.size() < kMaxShadowedPointLights) {
			renderer_.scene.pointLights.push_back(light);
			if (renderer_.scene.pointLights.size() == 1U) { renderer_.scene.ambient = light.ambient; }
		}
		scene_.addPointLight({.position = position, .color = color,
									.intensity = intensity, .range = range});
	}

	/// @brief Appends one point light to the capped renderer light list using explicit ambient.
	auto RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity,
											 LightRange range, LinearColor ambient) -> void {
		const PointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = ambient.value.x};
		if (renderer_.scene.pointLights.size() < kMaxShadowedPointLights) {
			renderer_.scene.pointLights.push_back(light);
			if (renderer_.scene.pointLights.size() == 1U) { renderer_.scene.ambient = light.ambient; }
		}
		scene_.addPointLight({.position = position, .color = color,
									.intensity = intensity, .range = range, .ambient = ambient});
	}

	/// @brief Replaces the active spot-light list using the current inner cone and ambient fallback.
	auto RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void {
		const SpotLight previous = renderer_.scene.spotLights.empty() ? SpotLight{} : renderer_.scene.spotLights.front(); ///< Keeps the inner cone and ambient of the replaced light.
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = previous.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = previous.ambient};
		renderer_.scene.spotLights.clear();
		renderer_.scene.spotLights.push_back(light);
		scene_.setSpotLight({.position = position, .direction = direction, .color = color,
								   .intensity = intensity, .range = range, .cone = cone});
	}

	/// @brief Replaces the active spot-light list using an explicit ambient term.
	auto RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = renderer_.scene.spotLights.empty() ? SpotLight{}.innerConeAngle : renderer_.scene.spotLights.front().innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		renderer_.scene.spotLights.clear();
		renderer_.scene.spotLights.push_back(light);
		scene_.setSpotLight({.position = position, .direction = direction, .color = color,
								   .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
	}

	/// @brief Appends one spot light to the capped renderer light list using default ambient.
	auto RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = SpotLight{}.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = SpotLight{}.ambient};
		if (renderer_.scene.spotLights.size() < kMaxShadowedSpotLights) {
			renderer_.scene.spotLights.push_back(light);
		}
		scene_.addSpotLight({.position = position, .direction = direction, .color = color,
								   .intensity = intensity, .range = range, .cone = cone});
	}

	/// @brief Appends one spot light to the capped renderer light list using explicit ambient.
	auto RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void {
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = SpotLight{}.innerConeAngle,
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		if (renderer_.scene.spotLights.size() < kMaxShadowedSpotLights) {
			renderer_.scene.spotLights.push_back(light);
		}
		scene_.addSpotLight({.position = position, .direction = direction, .color = color,
								   .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
	}

	/// @brief Mirrors one facade scene instance into the backend scene that the renderer uploads.
	auto RenderSystem::appendBackendObject(RenderInstanceHandle instance_handle) -> std::expected<std::size_t, Error> {
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
		for (const auto &vertex : mesh->vertices) {
			backend_mesh.vertices.push_back(Vertex{.position = {vertex.position.x, vertex.position.y, vertex.position.z},
																.color = color,
																.texCoord = {vertex.uv.x, vertex.uv.y}});
		}
		for (const auto index : mesh->indices) { backend_mesh.indices.push_back(index); }

		auto model = detail::modelMatrix(instance->local_transform);
		const auto use_texture = !material->base_color_texture_source.empty();
		renderer_.appendObject(std::move(backend_mesh), model,
									  use_texture ? std::optional<std::string>{material->base_color_texture_source.string()} :
														 std::nullopt);
		if (renderer_.scene.objects.empty()) { return std::unexpected(Error::missing_object); }
		return renderer_.scene.objects.size() - 1U;
	}

} // namespace vve::simple
