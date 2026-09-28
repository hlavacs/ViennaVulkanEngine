module VEEngine.Simple;
import std;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;

/// @file
/// @brief RenderSystem definitions that update object state, cameras, and renderer-owned lights.

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

	/// @brief Sets the default camera for windows without an override; each target supplies its own aspect ratio.
	auto RenderSystem::setCamera(Camera camera) -> void {
		renderer_.setCamera(std::move(camera));
		scene_.setCamera();
	}

	/// @brief Sets one rendered window's camera, including before the first frame creates its GPU target.
	auto RenderSystem::setCamera(WindowHandle window, Camera camera) -> std::expected<void, Error> {
		return updateCamera(window, std::move(camera));
	}

	/// @brief Restores a rendered window's use of the current default camera.
	auto RenderSystem::clearCamera(WindowHandle window) -> std::expected<void, Error> {
		return updateCamera(window, std::nullopt);
	}

	/// @brief Changes a target's override or retains it until lazy initialization; invalid windows leave cameras untouched.
	auto RenderSystem::updateCamera(WindowHandle window, std::optional<Camera> camera) -> std::expected<void, Error> {
		const auto *selected = window_system_ == nullptr ? nullptr : window_system_->findWindow(window);
		if (selected == nullptr || selected->info().should_close || selected->rendererId().value == "none") {
			return std::unexpected(Error::invalid_handle);
		}
		if (initialized_) {
			const auto target = std::ranges::find(renderer_.targets, window, &WindowTarget::handle);
			if (target == renderer_.targets.end()) { return std::unexpected(Error::invalid_handle); }
			target->camera = std::move(camera);
		} else if (camera) {
			pending_cameras_.insert_or_assign(window, std::move(*camera));
		} else {
			pending_cameras_.erase(window);
		}
		return {};
	}

	/// @brief Clears lights and their embedded owner tags without changing objects, resources, cameras or scene instances.
	auto RenderSystem::clearLights() -> void {
		renderer_.scene.directionalLights.clear();
		renderer_.scene.pointLights.clear();
		renderer_.scene.spotLights.clear();
	}

	/// @brief Replaces the active directional-light list with one renderer light.
	auto RenderSystem::setDirectionalLight(Direction direction, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const ForwardDirectionalLight light{
			.direction = direction.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		renderer_.scene.directionalLights.clear();
		renderer_.scene.directionalLights.push_back(light);
	}

	/// @brief Appends one directional light to the capped renderer light list.
	auto RenderSystem::addDirectionalLight(Direction direction, LinearColor color,
													 LightIntensity intensity, LinearColor ambient) -> void {
		const ForwardDirectionalLight light{
			.direction = direction.value,
			.color = color.value,
			.intensity = intensity,
			.ambient = ambient.value.x};
		if (renderer_.scene.directionalLights.size() < kMaxDirectionalLights) {
			renderer_.scene.directionalLights.push_back(light);
		}
	}

	/// @brief Replaces the active point-light list using the facade descriptor's default ambient.
	auto RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) -> void {
		setPointLight(position, color, intensity, range, vve::PointLight{}.ambient);
	}

	/// @brief Replaces the active point-light list using an explicit ambient term.
	auto RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity,
											 LightRange range, LinearColor ambient) -> void {
		const ForwardPointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = ambient.value.x};
		renderer_.scene.pointLights.clear();
		renderer_.scene.pointLights.push_back(light);
		renderer_.scene.ambient = light.ambient;
	}

	/// @brief Appends one point light to the capped renderer light list using default ambient.
	auto RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) -> void {
		addPointLight(position, color, intensity, range, vve::PointLight{}.ambient);
	}

	/// @brief Appends one point light to the capped renderer light list using explicit ambient.
	auto RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity,
											 LightRange range, LinearColor ambient) -> void {
		const ForwardPointLight light{.position = position.value,
									  .color = color.value,
									  .intensity = intensity.value,
									  .range = range.value,
									  .ambient = ambient.value.x};
		if (renderer_.scene.pointLights.size() < kMaxShadowedPointLights) {
			renderer_.scene.pointLights.push_back(light);
			if (renderer_.scene.pointLights.size() == 1U) { renderer_.scene.ambient = light.ambient; }
		}
	}

	/// @brief Replaces the active spot-light list using the facade descriptor's default ambient.
	auto RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void {
		setSpotLight(position, direction, color, intensity, range, cone, vve::SpotLight{}.ambient);
	}

	/// @brief Replaces the active spot-light list using an explicit ambient term.
	auto RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void {
		const ForwardSpotLight light{.position = position.value,
									 .direction = direction.value,
									 .color = color.value,
									 .intensity = intensity,
									 .range = range,
									 .innerConeAngle = detail::defaultInnerCone(cone),
									 .outerConeAngle = cone,
									 .ambient = ambient.value.x};
		renderer_.scene.spotLights.clear();
		renderer_.scene.spotLights.push_back(light);
	}

	/// @brief Appends one spot light to the capped renderer light list using default ambient.
	auto RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone) -> void {
		addSpotLight(position, direction, color, intensity, range, cone, vve::SpotLight{}.ambient);
	}

	/// @brief Appends one spot light to the capped renderer light list using explicit ambient.
	auto RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) -> void {
		const ForwardSpotLight light{.position = position.value,
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
	}

	/// @brief Adds one imported light to the renderer, tagged with its scene instance.
	auto RenderSystem::addImportedLight(const LightDescriptor &light, std::uint64_t owner) -> void {
		switch (light.kind) {
		case LightKind::directional: {
			const ForwardDirectionalLight backend{.direction = light.direction.value, .color = light.color.value,
				.intensity = light.intensity, .ambient = ForwardDirectionalLight{}.ambient, .owner = owner};
			if (renderer_.scene.directionalLights.size() < kMaxDirectionalLights) { renderer_.scene.directionalLights.push_back(backend); }
			break;
		}
		case LightKind::point: {
			const ForwardPointLight backend{.position = light.position.value, .color = light.color.value,
				.intensity = light.intensity.value, .range = light.range.value, .ambient = ForwardPointLight{}.ambient, .owner = owner};
			if (renderer_.scene.pointLights.size() < kMaxShadowedPointLights) {
				renderer_.scene.pointLights.push_back(backend);
				if (renderer_.scene.pointLights.size() == 1U) { renderer_.scene.ambient = backend.ambient; }
			}
			break;
		}
		case LightKind::spot: {
			const ForwardSpotLight backend{.position = light.position.value, .direction = light.direction.value,
				.color = light.color.value, .intensity = light.intensity, .range = light.range,
				// Assimp reports 2*pi when a file has no inner cone; then the engine default applies.
				.innerConeAngle = light.inner_cone.radians < light.cone.radians ? light.inner_cone : detail::defaultInnerCone(light.cone),
				.outerConeAngle = light.cone, .ambient = ForwardSpotLight{}.ambient, .owner = owner};
			if (renderer_.scene.spotLights.size() < kMaxShadowedSpotLights) { renderer_.scene.spotLights.push_back(backend); }
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
