export module VEEngine.Simple:RenderSystemScene;
import std;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Renderer;
import :Types;

/**
	* @file
	* @brief ECS/world scene application for the simple render system.
	*
	* Functional objects:
	* - detail::modelMatrix builds the renderer model matrix from the public transform contract.
	* - RenderSystemScene mirrors object visibility, object transforms, cameras, and lights into the forward renderer CPU Scene.
	*
	* Chosen cluster: ECS/world-to-renderer scene application logic for lights, cameras, and object transforms/visibility.
	*/

namespace vve::simple::detail {

	/// @brief Builds the backend model matrix from the public transform contract.
	[[nodiscard]] inline auto modelMatrix(Transform transform) -> Mat4 {
		const auto q = transform.rotation.value;
		auto rotation = identityMat4();
		rotation[0][0] = one() - static_cast<Scalar>(2) * (q.y * q.y + q.z * q.z);
		rotation[0][1] = static_cast<Scalar>(2) * (q.x * q.y + q.w * q.z);
		rotation[0][2] = static_cast<Scalar>(2) * (q.x * q.z - q.w * q.y);
		rotation[1][0] = static_cast<Scalar>(2) * (q.x * q.y - q.w * q.z);
		rotation[1][1] = one() - static_cast<Scalar>(2) * (q.x * q.x + q.z * q.z);
		rotation[1][2] = static_cast<Scalar>(2) * (q.y * q.z + q.w * q.x);
		rotation[2][0] = static_cast<Scalar>(2) * (q.x * q.z + q.w * q.y);
		rotation[2][1] = static_cast<Scalar>(2) * (q.y * q.z - q.w * q.x);
		rotation[2][2] = one() - static_cast<Scalar>(2) * (q.x * q.x + q.y * q.y);
		auto model = translate(identityMat4(), transform.translation.value);
		model = multiply(model, rotation);
		return scale(model, transform.scale.value);
	}

} // namespace vve::simple::detail

export namespace vve::simple {

	/// @brief CRTP mixin that applies facade scene data to the selected renderer CPU scene.
	template<typename System>
	struct RenderSystemScene {
		/// @brief Sets whether one live render object participates in future backend uploads.
		[[nodiscard]] auto setObjectVisible(RenderObjectHandle handle, bool visible) -> std::expected<void, Error> {
			const auto instance = system().findRenderObject(handle);
			if (!instance) { return std::unexpected(Error::missing_object); }
			auto *scene_instance = system().scene_.findInstance(instance->first);
			if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
			if (instance->second >= system().forward().scene.objects.size()) { return std::unexpected(Error::missing_object); }
			// Keep the CPU scene and renderer-visible backend scene in the same visibility state.
			scene_instance->visible = visible;
			system().forward().scene.objects[instance->second].visible = visible;
			return {};
		}

		/// @brief Returns whether one live render object is marked visible.
		[[nodiscard]] auto objectVisible(RenderObjectHandle handle) const -> std::expected<bool, Error> {
			const auto instance = system().findRenderObject(handle);
			if (!instance) { return std::unexpected(Error::missing_object); }
			const auto *scene_instance = system().scene_.findInstance(instance->first);
			return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
														 std::expected<bool, Error>{scene_instance->visible};
		}

		/// @brief Sets the source transform for one live render object.
		[[nodiscard]] auto setObjectTransform(RenderObjectHandle handle, Transform transform) -> std::expected<void, Error> {
			const auto instance = system().findRenderObject(handle);
			if (!instance) { return std::unexpected(Error::missing_object); }
			auto *scene_instance = system().scene_.findInstance(instance->first);
			if (scene_instance == nullptr) { return std::unexpected(Error::missing_object); }
			if (instance->second >= system().forward().scene.objects.size()) { return std::unexpected(Error::missing_object); }
			// Keep the CPU scene and renderer-visible backend scene in the same world transform.
			scene_instance->local_transform = transform;
			scene_instance->world_transform = detail::modelMatrix(transform);
			system().forward().scene.objects[instance->second].model = scene_instance->world_transform;
			return {};
		}

		/// @brief Returns the source transform for one live render object.
		[[nodiscard]] auto objectTransform(RenderObjectHandle handle) const -> std::expected<Transform, Error> {
			const auto instance = system().findRenderObject(handle);
			if (!instance) { return std::unexpected(Error::missing_object); }
			const auto *scene_instance = system().scene_.findInstance(instance->first);
			return scene_instance == nullptr ? std::unexpected(Error::missing_object) :
														 std::expected<Transform, Error>{scene_instance->local_transform};
		}

		/// @brief Mirrors one camera into the renderer CPU scene and retained render-scene data.
		auto setCamera(Camera camera, PixelExtent extent) -> void {
			const auto eye = camera.position.value;
			const auto target = math::add(eye, camera.forward.value);
			system().forward().setCamera(eye, target);
			system().scene_.setCamera({.camera = std::move(camera), .target_extent = extent});
		}

		/// @brief Replaces the active directional-light list with one renderer light.
		void setDirectionalLight(Direction direction_to_light, LinearColor color,
										 LightIntensity intensity, LinearColor ambient) {
			const DirectionalLight light{
				.direction = direction_to_light.value,
				.color = color.value,
				.intensity = intensity,
				.ambient = ambient.value.x};
			system().forward().scene.directionalLights.clear();													// Setter preserves the legacy single-light mode.
			system().forward().scene.directionalLights.push_back(light);
			system().forward().scene.directionalLight = light;
			system().scene_.setDirectionalLight({.direction_to_light = direction_to_light,
														  .color = color, .intensity = intensity, .ambient = ambient});
		}

		/// @brief Appends one directional light to the capped renderer light list.
		void addDirectionalLight(Direction direction_to_light, LinearColor color,
										 LightIntensity intensity, LinearColor ambient) {
			const DirectionalLight light{
				.direction = direction_to_light.value,
				.color = color.value,
				.intensity = intensity,
				.ambient = ambient.value.x};
			if (system().forward().scene.directionalLights.size() < kMaxDirectionalLights) {				// First entry remains the shader-visible directional light.
				system().forward().scene.directionalLights.push_back(light);
			}
			if (!system().forward().scene.directionalLights.empty()) {
				system().forward().scene.directionalLight = system().forward().scene.directionalLights.front();
			}
			system().scene_.addDirectionalLight({.direction_to_light = direction_to_light,
														  .color = color, .intensity = intensity, .ambient = ambient});
		}

		/// @brief Replaces the active point-light list using the current ambient fallback.
		void setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
			const PointLight light{.position = position.value,
										  .color = color.value,
										  .intensity = intensity.value,
										  .range = range.value,
										  .ambient = system().forward().scene.pointLight.ambient};
			system().forward().scene.pointLights.clear();
			system().forward().scene.pointLights.push_back(light);
			system().forward().scene.pointLight = light;
			system().scene_.setPointLight({.position = position, .color = color,
													 .intensity = intensity, .range = range});
		}

		/// @brief Replaces the active point-light list using an explicit ambient term.
		void setPointLight(Position position, LinearColor color, LightIntensity intensity,
								 LightRange range, LinearColor ambient) {
			const PointLight light{.position = position.value,
										  .color = color.value,
										  .intensity = intensity.value,
										  .range = range.value,
										  .ambient = ambient.value.x};
			system().forward().scene.pointLights.clear();
			system().forward().scene.pointLights.push_back(light);
			system().forward().scene.pointLight = light;
			system().scene_.setPointLight({.position = position, .color = color,
													 .intensity = intensity, .range = range, .ambient = ambient});
		}

		/// @brief Appends one point light to the capped renderer light list using default ambient.
		void addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
			const PointLight light{.position = position.value,
										  .color = color.value,
										  .intensity = intensity.value,
										  .range = range.value,
										  .ambient = PointLight{}.ambient};
			if (system().forward().scene.pointLights.size() < kMaxShadowedPointLights) {					// Fixed cap mirrors the point-shadow layer budget.
				system().forward().scene.pointLights.push_back(light);
				if (system().forward().scene.pointLights.size() == 1U) { system().forward().scene.pointLight = light; }
			}
			system().scene_.addPointLight({.position = position, .color = color,
													 .intensity = intensity, .range = range});
		}

		/// @brief Appends one point light to the capped renderer light list using explicit ambient.
		void addPointLight(Position position, LinearColor color, LightIntensity intensity,
								 LightRange range, LinearColor ambient) {
			const PointLight light{.position = position.value,
										  .color = color.value,
										  .intensity = intensity.value,
										  .range = range.value,
										  .ambient = ambient.value.x};
			if (system().forward().scene.pointLights.size() < kMaxShadowedPointLights) {					// First entry remains the shader-visible point light.
				system().forward().scene.pointLights.push_back(light);
				if (system().forward().scene.pointLights.size() == 1U) { system().forward().scene.pointLight = light; }
			}
			system().scene_.addPointLight({.position = position, .color = color,
													 .intensity = intensity, .range = range, .ambient = ambient});
		}

		/// @brief Replaces the active spot-light list using the current inner cone and ambient fallback.
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone) {
			const SpotLight light{.position = position.value,
										 .direction = direction.value,
										 .color = color.value,
										 .intensity = intensity,
										 .range = range,
										 .innerConeAngle = system().forward().scene.spotLight.innerConeAngle,
										 .outerConeAngle = cone,
										 .ambient = system().forward().scene.spotLight.ambient};
			system().forward().scene.spotLights.clear();													// Setter preserves the legacy single-light mode.
			system().forward().scene.spotLights.push_back(light);
			system().forward().scene.spotLight = light;
			system().scene_.setSpotLight({.position = position, .direction = direction, .color = color,
													 .intensity = intensity, .range = range, .cone = cone});
		}

		/// @brief Replaces the active spot-light list using an explicit ambient term.
		void setSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
			const SpotLight light{.position = position.value,
										 .direction = direction.value,
										 .color = color.value,
										 .intensity = intensity,
										 .range = range,
										 .innerConeAngle = system().forward().scene.spotLight.innerConeAngle,
										 .outerConeAngle = cone,
										 .ambient = ambient.value.x};
			system().forward().scene.spotLights.clear();													// Setter replaces all CPU-side spot lights.
			system().forward().scene.spotLights.push_back(light);
			system().forward().scene.spotLight = light;
			system().scene_.setSpotLight({.position = position, .direction = direction, .color = color,
													 .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
		}

		/// @brief Appends one spot light to the capped renderer light list using default ambient.
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone) {
			const SpotLight light{.position = position.value,
										 .direction = direction.value,
										 .color = color.value,
										 .intensity = intensity,
										 .range = range,
										 .innerConeAngle = SpotLight{}.innerConeAngle,
										 .outerConeAngle = cone,
										 .ambient = SpotLight{}.ambient};
			if (system().forward().scene.spotLights.size() < kMaxShadowedSpotLights) {					// Extra lights are stored only for later CPU shadow work.
				system().forward().scene.spotLights.push_back(light);
				if (system().forward().scene.spotLights.size() == 1U) { system().forward().scene.spotLight = light; }
			}
			system().scene_.addSpotLight({.position = position, .direction = direction, .color = color,
												 .intensity = intensity, .range = range, .cone = cone});
		}

		/// @brief Appends one spot light to the capped renderer light list using explicit ambient.
		void addSpotLight(Position position, Direction direction, LinearColor color,
								LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
			const SpotLight light{.position = position.value,
										 .direction = direction.value,
										 .color = color.value,
										 .intensity = intensity,
										 .range = range,
										 .innerConeAngle = SpotLight{}.innerConeAngle,
										 .outerConeAngle = cone,
										 .ambient = ambient.value.x};
			if (system().forward().scene.spotLights.size() < kMaxShadowedSpotLights) {					// First entry remains the shader-visible spot light.
				system().forward().scene.spotLights.push_back(light);
				if (system().forward().scene.spotLights.size() == 1U) { system().forward().scene.spotLight = light; }
			}
			system().scene_.addSpotLight({.position = position, .direction = direction, .color = color,
												 .intensity = intensity, .range = range, .ambient = ambient, .cone = cone});
		}

	protected:
		/// @brief Mirrors one facade scene instance into the backend scene that the renderer uploads.
		[[nodiscard]] auto appendBackendObject(auto instance_handle) -> std::expected<std::size_t, Error> {
			const auto *instance = system().scene_.findInstance(instance_handle);
			if (instance == nullptr) { return std::unexpected(Error::missing_object); }
			const auto *mesh = system().scene_.findMesh(instance->mesh);
			const auto *material = system().scene_.findMaterial(instance->material);
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
			std::visit([&](auto &renderer) {
				renderer.appendObject(std::move(backend_mesh), model,
											 use_texture ? std::optional<std::string>{material->base_color_texture_source.string()} :
															 std::nullopt);
			}, system().renderer_);
			if (system().forward().scene.objects.empty()) { return std::unexpected(Error::missing_object); }
			return system().forward().scene.objects.size() - 1U;
		}

	private:
		/// @brief Returns the owning render system for dependent member access.
		[[nodiscard]] auto system() -> System & { return static_cast<System &>(*this); }
		/// @brief Returns the owning render system for const dependent member access.
		[[nodiscard]] auto system() const -> const System & { return static_cast<const System &>(*this); }
	};

} // namespace vve::simple
