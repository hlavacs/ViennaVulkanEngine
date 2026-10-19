module VVEngine.Simple;
import std;
import VVEngine.Simple.Scene;
import VVEngine.Simple.Renderer;

/// @file
/// @brief RenderSystem definitions that update object state, cameras, and renderer-owned lights.

namespace vve::simple {

	/// @brief Validates haze before committing it; zero density disables the effect without rebuilding resources.
	auto RenderSystem::setAtmosphere(AtmosphereSettings settings) -> std::expected<void, Error> {
		const auto c = settings.color.value;
		if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || c.x < 0 || c.y < 0 || c.z < 0 ||
			!std::isfinite(settings.density) || settings.density < 0 || !std::isfinite(settings.height_falloff) ||
			settings.height_falloff < 0 || !std::isfinite(settings.base_height)) { return std::unexpected(Error::invalid_argument); }
		renderer_.setAtmosphere(settings);
		return {};
	}

	/// @brief Reads the configured haze; defaults preserve the original renderer's pixels.
	auto RenderSystem::atmosphere() const -> AtmosphereSettings { return renderer_.atmosphere(); }

	/// @brief Validates the physical sky before committing it; it is renderer state, so clearScene keeps it.
	auto RenderSystem::setSky(SkySettings settings) -> std::expected<void, Error> {
		const auto &s = settings;
		const std::array values{s.intensity, s.haze, s.sun_size, s.cloud_coverage, s.cloud_base, s.cloud_thickness, s.cloud_density, s.wind.x, s.wind.y};
		if (!std::ranges::all_of(values, [](Scalar value) { return std::isfinite(value); }) || s.intensity < 0 || s.haze < 0 || s.sun_size < 0 ||
			s.cloud_coverage < 0 || s.cloud_coverage > 1 || s.cloud_thickness <= 0 || s.cloud_density < 0) { return std::unexpected(Error::invalid_argument); }
		renderer_.setSky(settings);
		return {};
	}

	/// @brief Reads the physical sky settings.
	auto RenderSystem::sky() const -> SkySettings { return renderer_.sky(); }

	/// @brief Sets elapsed animation seconds; no wall-clock dependency is hidden in frame recording.
	auto RenderSystem::setAnimationTime(AnimationTime time) -> std::expected<void, Error> {
		if (!std::isfinite(time.seconds) || time.seconds < 0 || time.seconds > std::numeric_limits<float>::max()) {
			return std::unexpected(Error::invalid_argument);
		}
		renderer_.setAnimationTime(time);
		return {};
	}
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

	/// @brief Sets one live render object's alpha in [0, 1]; below one it is blended over what lies behind it.
	auto RenderSystem::setObjectOpacity(RenderObjectHandle handle, Scalar opacity) -> std::expected<void, Error> {
		if (!(opacity >= zero() && opacity <= one())) { return std::unexpected(Error::invalid_argument); } // Also rejects NaN.
		const auto instance = findRenderObject(handle);
		auto *scene_instance = instance ? scene_.findInstance(*instance) : nullptr;
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		scene_instance->opacity = opacity;
		return {};
	}

	/// @brief Returns one live render object's alpha.
	auto RenderSystem::objectOpacity(RenderObjectHandle handle) const -> std::expected<Scalar, Error> {
		const auto instance = findRenderObject(handle);
		const auto *scene_instance = instance ? scene_.findInstance(*instance) : nullptr;
		if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
		return scene_instance->opacity;
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

	/// @brief Returns one live object's world box from its mesh vertices and world matrix.
	///
	/// A sprite turns towards every camera, so its box is a cube around its origin that covers each orientation.
	auto RenderSystem::objectBounds(RenderObjectHandle handle) const -> std::expected<Bounds, Error> {
		const auto object = findRenderObject(handle);
		const auto *instance = object ? scene_.findInstance(*object) : nullptr;
		const auto *mesh = instance ? scene_.findMesh(instance->mesh) : nullptr;
		if (mesh == nullptr) { return std::unexpected(Error::missing_object); }
		const Mat4 &world = instance->world_transform;
		auto box = Bounds{};
		const auto include = [&box](Vec3 point) {
			box.minimum.value = box.valid ? min(box.minimum.value, point) : point;
			box.maximum.value = box.valid ? max(box.maximum.value, point) : point;
			box.valid = true;
		};
		if (instance->sprite) {
			Scalar radius{};
			for (const auto &vertex : *mesh->vertices) { radius = std::max(radius, length(vertex.position)); }
			radius *= std::max(length(Vec3{world[0]}), length(Vec3{world[1]}));
			const Vec3 centre{world[3]};
			include(subtract(centre, Vec3{radius, radius, radius}));
			include(add(centre, Vec3{radius, radius, radius}));
			return box;
		}
		for (const auto &vertex : *mesh->vertices) { include(Vec3{multiply(world, Vec4{vertex.position, one()})}); }
		// Shader waves displace vertically in world space, independently of the patch's placement.
		if (instance->waves.w != zero()) { box.minimum.value.y -= instance->waves.x; box.maximum.value.y += instance->waves.x; }
		return box;
	}

	/// @brief Finds the visible object whose world box a ray enters first, within max_distance along the normalized direction.
	auto RenderSystem::raycast(Position origin, Direction direction, Scalar max_distance) const -> std::optional<RaycastHit> {
		if (!(lengthSquared(direction.value) > zero())) { return std::nullopt; }
		const Vec3 step = normalize(direction.value);
		auto nearest = std::optional<RaycastHit>{};
		// Slab test per box: the ray is inside between its latest entry and its earliest exit over the three axes.
		for (const auto &[object, instance] : render_objects_) {
			const auto *item = scene_.findInstance(instance);
			const auto box = item != nullptr && item->visible ? objectBounds(object) : std::unexpected(Error::missing_object);
			if (!box || !box->valid) { continue; }
			Scalar enter{zero()};
			Scalar leave{max_distance};
			for (const int axis : {0, 1, 2}) {
				const Scalar start = origin.value[axis];
				const Scalar low = box->minimum.value[axis];
				const Scalar high = box->maximum.value[axis];
				if (step[axis] == zero()) {
					if (start < low || start > high) { leave = -one(); }
					continue;
				}
				const Scalar first = (low - start) / step[axis];
				const Scalar second = (high - start) / step[axis];
				enter = std::max(enter, std::min(first, second));
				leave = std::min(leave, std::max(first, second));
			}
			if (enter <= leave && (!nearest || enter < nearest->distance)) { nearest = RaycastHit{.object = object, .distance = enter}; }
		}
		return nearest;
	}

	/// @brief Returns the world ray through a point of a rendered window, (0,0) top left to (1,1) bottom right, from its camera.
	auto RenderSystem::screenRay(WindowHandle window, Vec2 normalized_position) const -> std::expected<std::pair<Position, Direction>, Error> {
		const auto *selected = renderedWindow(window);
		if (selected == nullptr) { return std::unexpected(Error::invalid_handle); }
		// The same camera the renderer uses: the window override, a pending one before initialization, or the default.
		const Camera *camera = &renderer_.camera();
		if (const auto target = std::ranges::find(renderer_.targets, window, &WindowTarget::handle);
			target != renderer_.targets.end() && target->camera) { camera = &*target->camera; }
		if (const auto pending = pending_cameras_.find(window); pending != pending_cameras_.end()) { camera = &pending->second; }
		const auto extent = selected->info().extent;
		const Scalar aspect = extent.height == 0U ? one() : static_cast<Scalar>(extent.width) / static_cast<Scalar>(extent.height);
		const Vec3 forward = normalize(camera->forward.value);
		const Vec3 right = normalize(cross(forward, detail::stableUp(forward)));
		const Vec3 up = cross(right, forward);
		const Scalar tan_half = std::tan(camera->fov_y.radians / 2);
		const Scalar x = (2 * normalized_position.x - one()) * tan_half * aspect;
		const Scalar y = (one() - 2 * normalized_position.y) * tan_half;
		return std::pair{camera->position, Direction{.value = normalize(add(forward, add(scale(right, x), scale(up, y))))}};
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

	/// @brief Returns an open window that renders, or null for unknown, closed and opted-out windows.
	auto RenderSystem::renderedWindow(WindowHandle window) const -> const Window * {
		const auto *selected = window_system_ == nullptr ? nullptr : window_system_->findWindow(window);
		return selected == nullptr || selected->info().should_close || selected->rendererId().value == "none" ? nullptr : selected;
	}

	/// @brief Changes a target's override or retains it until lazy initialization; invalid windows leave cameras untouched.
	auto RenderSystem::updateCamera(WindowHandle window, std::optional<Camera> camera) -> std::expected<void, Error> {
		if (renderedWindow(window) == nullptr) { return std::unexpected(Error::invalid_handle); }
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
