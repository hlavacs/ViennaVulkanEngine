module VEEngine.Simple;
import std;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;

/// @file
/// @brief RenderSystem definitions that mirror object state, cameras, and lights into the forward renderer CPU Scene.

namespace vve::simple {
	/// @brief Sets whether one live render object is drawn in its flat base color without lighting.
	auto RenderSystem::setObjectUnlit(RenderObjectHandle handle, bool unlit) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(*instance);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		scene_instance->unlit = unlit;
		return {};
	}

	/// @brief Sets whether one live render object is drawn into shadow depth passes.
	auto RenderSystem::setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(*instance);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		scene_instance->casts_shadow = casts_shadow;
		return {};
	}

	/// @brief Sets whether one live render object is drawn.
	auto RenderSystem::setObjectVisible(RenderObjectHandle handle, bool visible) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(*instance);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		scene_instance->visible = visible;
		return {};
	}

	/// @brief Returns whether one live render object is marked visible.
	auto RenderSystem::objectVisible(RenderObjectHandle handle) const -> std::expected<bool, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		const auto *scene_instance = scene_.findInstance(*instance);
		return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
													 std::expected<bool, Error>{scene_instance->visible};
	}

	/// @brief Sets the source transform for one live render object.
	auto RenderSystem::setObjectTransform(RenderObjectHandle handle, Transform transform) -> std::expected<void, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		auto *scene_instance = scene_.findInstance(*instance);
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		scene_instance->local_transform = transform;
		scene_instance->world_transform = detail::modelMatrix(transform);
		return {};
	}

	/// @brief Returns the source transform for one live render object.
	auto RenderSystem::objectTransform(RenderObjectHandle handle) const -> std::expected<Transform, Error> {
		const auto instance = findRenderObject(handle);
		if (!instance) { return std::unexpected(Error::missing_object); }
		const auto *scene_instance = scene_.findInstance(*instance);
		return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
													 std::expected<Transform, Error>{scene_instance->local_transform};
	}

	/// @brief Mirrors one camera into the renderer CPU scene and retained render-scene data.
	auto RenderSystem::setCamera(Camera camera, PixelExtent extent) -> void {
		const auto eye = camera.position.value;
		const auto target = math::add(eye, camera.forward.value);
		renderer_.setCamera(eye, target, camera.fov_y.radians);
		scene_.setCamera({.camera = std::move(camera), .target_extent = extent});
	}

	/// @brief Replaces the active directional-light list with one renderer light.
	auto RenderSystem::setDirectionalLight(Direction direction, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const DirectionalLight light{
			.direction = direction.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		renderer_.scene.directionalLights.clear();
		renderer_.scene.directionalLights.push_back(light);
		scene_.setDirectionalLight({.direction_to_light = direction,
											 .color = color, .intensity = intensity, .ambient = ambient});
	}

	/// @brief Appends one directional light to the capped renderer light list.
	auto RenderSystem::addDirectionalLight(Direction direction, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const DirectionalLight light{
			.direction = direction.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		if (renderer_.scene.directionalLights.size() < kMaxDirectionalLights) {
			renderer_.scene.directionalLights.push_back(light);
		}
		scene_.addDirectionalLight({.direction_to_light = direction,
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

	/// @brief Replaces the active spot-light list, keeping the ambient term of the replaced light.
	auto RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void {
		const SpotLight previous = renderer_.scene.spotLights.empty() ? SpotLight{} : renderer_.scene.spotLights.front(); ///< Supplies the ambient of the replaced light.
		const SpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = detail::defaultInnerCone(cone),
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
									 .innerConeAngle = detail::defaultInnerCone(cone),
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
									 .innerConeAngle = detail::defaultInnerCone(cone),
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
									 .innerConeAngle = detail::defaultInnerCone(cone),
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		if (renderer_.scene.spotLights.size() < kMaxShadowedSpotLights) {
			renderer_.scene.spotLights.push_back(light);
		}
		scene_.addSpotLight({.position = position, .direction = direction, .color = color,
								   .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
	}

	/// @brief Adds one imported light to the renderer and the CPU render scene, tagged with its scene instance.
	auto RenderSystem::addImportedLight(const LightDescriptor &light, std::uint64_t owner) -> void {
		switch (light.kind) {
		case LightKind::directional: {
			const DirectionalLight backend{.direction = light.direction.value, .color = light.color.value,
				.intensity = light.intensity, .ambient = DirectionalLight{}.ambient, .owner = owner};
			if (renderer_.scene.directionalLights.size() < kMaxDirectionalLights) { renderer_.scene.directionalLights.push_back(backend); }
			scene_.addDirectionalLight({.direction_to_light = light.direction, .color = light.color, .intensity = light.intensity,
				.ambient = LinearColor{.value = Vec3{backend.ambient, backend.ambient, backend.ambient}}, .owner = owner});
			break;
		}
		case LightKind::point: {
			const PointLight backend{.position = light.position.value, .color = light.color.value,
				.intensity = light.intensity.value, .range = light.range.value, .ambient = PointLight{}.ambient, .owner = owner};
			if (renderer_.scene.pointLights.size() < kMaxShadowedPointLights) {
				renderer_.scene.pointLights.push_back(backend);
				if (renderer_.scene.pointLights.size() == 1U) { renderer_.scene.ambient = backend.ambient; }
			}
			scene_.addPointLight({.position = light.position, .color = light.color, .intensity = light.intensity,
				.range = light.range, .owner = owner});
			break;
		}
		case LightKind::spot: {
			const SpotLight backend{.position = light.position.value, .direction = light.direction.value,
				.color = light.color.value, .intensity = light.intensity, .range = light.range,
				// Assimp reports 2*pi when a file has no inner cone; then the engine default applies.
				.innerConeAngle = light.inner_cone.radians < light.cone.radians ? light.inner_cone : detail::defaultInnerCone(light.cone),
				.outerConeAngle = light.cone, .ambient = SpotLight{}.ambient, .owner = owner};
			if (renderer_.scene.spotLights.size() < kMaxShadowedSpotLights) { renderer_.scene.spotLights.push_back(backend); }
			scene_.addSpotLight({.position = light.position, .direction = light.direction, .color = light.color,
				.intensity = light.intensity, .range = light.range, .cone = light.cone, .owner = owner});
			break;
		}
		}
	}

	/// @brief Removes the lights and cameras one scene instance imported.
	auto RenderSystem::removeImportedLights(std::uint64_t owner) -> void {
		const auto owned = [owner](const auto &light) { return light.owner == owner; };
		std::erase_if(renderer_.scene.directionalLights, owned);
		std::erase_if(renderer_.scene.pointLights, owned);
		std::erase_if(renderer_.scene.spotLights, owned);
		scene_.eraseImportedBy(owner);
	}

} // namespace vve::simple
