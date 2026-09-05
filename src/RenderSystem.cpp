module VEEngine;
import :RenderSystem;

namespace vve {

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	RenderSystem::RenderSystem(Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Clears the active CPU scene.
	void RenderSystem::clearScene() { impl_.clearScene(); }

	/// @brief Loads the standard three-cube sample scene through facade authoring calls.
	std::expected<void, Error> RenderSystem::loadSampleScene() {
		clearScene();
		const auto minimum = Vec3{-0.5F, -0.5F, -0.5F};
		const auto maximum = Vec3{0.5F, 0.5F, 0.5F};
		const auto cube_color = LinearColor{.value = Vec3{0.55F, 0.55F, 0.55F}};
		if (auto result = addCuboid(minimum, maximum, cube_color); !result) { return std::unexpected(result.error()); }
		if (auto result = addCuboid(minimum, maximum, cube_color,
												Transform{.translation = Position{.value = Vec3{-1.5F, 0.0F, 0.0F}}});
			 !result) {
			return std::unexpected(result.error());
		}
		if (auto result = addCuboid(minimum, maximum, cube_color,
												Transform{.translation = Position{.value = Vec3{1.5F, 0.0F, 0.0F}}});
			 !result) {
			return std::unexpected(result.error());
		}
		setPointLight(Position{.value = Vec3{2.0F, 3.5F, -2.0F}},
							LinearColor{.value = Vec3{1.0F, 0.96F, 0.82F}},
							LightIntensity{.value = 3.0F}, LightRange{.value = 7.0F});
		setDirectionalLight(Direction{.value = Vec3{-0.55F, -0.78F, 0.30F}},
									LinearColor{.value = Vec3{0.65F, 0.82F, 1.0F}},
									LightIntensity{.value = 0.75F}, LinearColor{.value = Vec3{0.025F, 0.025F, 0.025F}});
		setSpotLight(Position{.value = Vec3{1.45F, 4.8F, -1.45F}},
						  Direction{.value = Vec3{0.10F, -0.98F, -0.16F}},
						  LinearColor{.value = Vec3{1.0F, 0.58F, 0.38F}},
						  LightIntensity{.value = 2.2F}, LightRange{.value = 5.8F}, SpotConeAngle{.radians = 0.58F});
		return {};
	}

	/// @brief Sets the active scene camera.
	void RenderSystem::setCamera(Camera camera, PixelExtent extent) { impl_.setCamera(std::move(camera), extent); }

	/// @brief Sets the active directional light.
	void RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
														LightIntensity intensity, LinearColor ambient) {
		impl_.setDirectionalLight(direction_to_light, color, intensity, ambient);
	}

	/// @brief Adds a directional light to the active CPU scene.
	void RenderSystem::addDirectionalLight(Direction direction_to_light, LinearColor color,
														LightIntensity intensity, LinearColor ambient) {
		impl_.addDirectionalLight(direction_to_light, color, intensity, ambient);
	}

	/// @brief Sets the active point light.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
		impl_.setPointLight(position, color, intensity, range);
	}

	/// @brief Sets the active point light with ambient lighting.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range,
											 LinearColor ambient) {
		impl_.setPointLight(position, color, intensity, range, ambient);
	}

	/// @brief Adds a point light to the active CPU scene.
	void RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
		impl_.addPointLight(position, color, intensity, range);
	}

	/// @brief Adds a point light with ambient lighting to the active CPU scene.
	void RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range,
											 LinearColor ambient) {
		impl_.addPointLight(position, color, intensity, range, ambient);
	}

	/// @brief Sets the active spotlight.
	void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		impl_.setSpotLight(position, direction, color, intensity, range, cone);
	}

	/// @brief Sets the active spotlight with ambient lighting.
	void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
		impl_.setSpotLight(position, direction, color, intensity, range, cone, ambient);
	}

	/// @brief Adds a spotlight to the active CPU scene.
	void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		impl_.addSpotLight(position, direction, color, intensity, range, cone);
	}

	/// @brief Adds a spotlight with ambient lighting to the active CPU scene.
	void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
		impl_.addSpotLight(position, direction, color, intensity, range, cone, ambient);
	}

	/// @brief Adds a colored plane and returns its public render-object handle.
	std::expected<RenderObjectHandle, Error> RenderSystem::addPlane(Vec2 half_extent, LinearColor color, Transform transform) {
		return impl_.addPlane(half_extent, color, transform);
	}

	/// @brief Adds a colored cuboid and returns its public render-object handle.
	std::expected<RenderObjectHandle, Error> RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																				  Transform transform) {
		return impl_.addCuboid(minimum, maximum, color, transform);
	}

	/// @brief Adds a colored indexed triangle mesh and returns its public render-object handle.
	std::expected<RenderObjectHandle, Error> RenderSystem::addTriangleMesh(
		Vector<Vec3> positions, Vector<std::uint32_t> indices, LinearColor color,
		Transform transform) {
		return impl_.addTriangleMesh(
			std::move(positions), std::move(indices), color, transform);
	}

	/// @brief Adds a textured cuboid and returns its public render-object handle.
	std::expected<RenderObjectHandle, Error> RenderSystem::addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																								  std::filesystem::path base_color_texture,
																								  Transform transform) {
		return impl_.addTexturedCuboid(minimum, maximum, std::move(base_color_texture), transform);
	}

	/// @brief Removes one previously added render object.
	std::expected<void, Error> RenderSystem::removeObject(RenderObjectHandle handle) {
		return impl_.removeObject(handle);
	}

	/// @brief Sets whether one render object is visible.
	std::expected<void, Error> RenderSystem::setObjectVisible(RenderObjectHandle handle, bool visible) {
		return impl_.setObjectVisible(handle, visible);
	}

	/// @brief Sets whether one render object casts shadows.
	std::expected<void, Error> RenderSystem::setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow) {
		return impl_.setObjectCastsShadow(handle, casts_shadow);
	}

	/// @brief Sets whether one render object is drawn in its flat base color without lighting.
	std::expected<void, Error> RenderSystem::setObjectUnlit(RenderObjectHandle handle, bool unlit) {
		return impl_.setObjectUnlit(handle, unlit);
	}

	/// @brief Returns whether one render object is visible.
	std::expected<bool, Error> RenderSystem::objectVisible(RenderObjectHandle handle) const {
		return impl_.objectVisible(handle);
	}

	/// @brief Sets one render object's source transform.
	std::expected<void, Error> RenderSystem::setObjectTransform(RenderObjectHandle handle, Transform transform) {
		return impl_.setObjectTransform(handle, transform);
	}

	/// @brief Returns one render object's source transform.
	std::expected<Transform, Error> RenderSystem::objectTransform(RenderObjectHandle handle) const {
		return impl_.objectTransform(handle);
	}

	/// @brief Updates vertex positions for a fixed-topology triangle mesh.
	std::expected<void, Error> RenderSystem::setObjectMeshPositions(
		RenderObjectHandle handle, Vector<Vec3> positions) {
		return impl_.setObjectMeshPositions(handle, std::move(positions));
	}

	/// @brief Returns render objects registered for one scene instance.
	std::expected<Vector<RenderObjectHandle>, Error>
	RenderSystem::sceneInstanceObjects(RenderSceneInstanceHandle instance) const {
		return impl_.sceneInstanceObjects(instance);
	}

	/// @brief Returns the scene instance that created one render object.
	std::expected<RenderSceneInstanceHandle, Error> RenderSystem::objectSourceScene(RenderObjectHandle handle) const {
		return impl_.objectSourceScene(handle);
	}

	/// @brief Returns the source asset-scene node that created one render object.
	std::expected<NodeHandle, Error> RenderSystem::objectSourceNode(RenderObjectHandle handle) const {
		return impl_.objectSourceNode(handle);
	}

	/// @brief Creates a render-scene instance for a loaded scene.
	std::expected<RenderSceneInstanceHandle, Error> RenderSystem::instantiateScene(SceneHandle scene,
																									 SceneInstantiationOptions options) {
		return impl_.instantiateScene(scene, options);
	}

	/// @brief Removes one render-scene instance and the objects it created.
	std::expected<void, Error> RenderSystem::removeSceneInstance(RenderSceneInstanceHandle instance) {
		return impl_.removeSceneInstance(instance);
	}

	/// @brief Removes one loaded scene when no live render object depends on it.
	std::expected<void, Error> RenderSystem::removeScene(SceneHandle handle) {
		return impl_.removeScene(handle);
	}

	/// @brief Removes CPU render assets no live render object references.
	std::size_t RenderSystem::purgeUnusedAssets() { return impl_.purgeUnusedAssets(); }

	/// @brief Returns mesh count in the active CPU scene.
	std::size_t RenderSystem::sceneMeshCount() const { return impl_.sceneMeshCount(); }

	/// @brief Returns material count in the active CPU scene.
	std::size_t RenderSystem::sceneMaterialCount() const { return impl_.sceneMaterialCount(); }

	/// @brief Returns directional-light count in the active CPU scene.
	std::size_t RenderSystem::sceneDirectionalLightCount() const {
		return impl_.sceneDirectionalLightCount();
	}

	/// @brief Returns point-light count in the active CPU scene.
	std::size_t RenderSystem::scenePointLightCount() const { return impl_.scenePointLightCount(); }

	/// @brief Returns spot-light count in the active CPU scene.
	std::size_t RenderSystem::sceneSpotLightCount() const { return impl_.sceneSpotLightCount(); }

	/// @brief Returns imported-camera count in the active CPU scene.
	std::size_t RenderSystem::sceneCameraCount() const { return impl_.sceneCameraCount(); }

	/// @brief Returns instance count in the active CPU scene.
	std::size_t RenderSystem::sceneInstanceCount() const { return impl_.sceneInstanceCount(); }

	/// @brief Returns source vertex count in the active CPU scene.
	std::size_t RenderSystem::sceneVertexCount() const { return impl_.sceneVertexCount(); }

	/// @brief Returns source index count in the active CPU scene.
	std::size_t RenderSystem::sceneIndexCount() const { return impl_.sceneIndexCount(); }

	/// @brief Reports whether the active CPU scene has a camera.
	bool RenderSystem::hasSceneCamera() const { return impl_.hasSceneCamera(); }

	/// @brief Reports whether the active CPU scene has a directional light.
	bool RenderSystem::hasSceneDirectionalLight() const { return impl_.hasSceneDirectionalLight(); }

	/// @brief Reports whether the active CPU scene has a point light.
	bool RenderSystem::hasScenePointLight() const { return impl_.hasScenePointLight(); }

	/// @brief Reports whether the active CPU scene has a spotlight.
	bool RenderSystem::hasSceneSpotLight() const { return impl_.hasSceneSpotLight(); }

	/// @brief Copies the shadow-depth samples recorded by the last rendered frame into facade data.
	Vector<RenderShadowDepthSample> RenderSystem::shadowDepthSamples() const {
		Vector<RenderShadowDepthSample> result{};
		for (const auto &sample : impl_.shadowDepthSamples()) {
			result.push_back(RenderShadowDepthSample{.light_type = sample.light_type,
																 .light_index = sample.light_index,
																 .face_index = sample.face_index,
																 .layer = sample.layer,
																 .world = sample.world,
																 .light_ndc = sample.light_ndc,
																 .pixel_x = sample.pixel_x,
																 .pixel_y = sample.pixel_y,
																 .expected_depth = sample.expected_depth,
																 .bias = sample.bias,
																 .shadow_factor = sample.shadow_factor,
																 .gpu_depth = sample.gpu_depth,
																 .error = sample.error,
																 .has_gpu = sample.has_gpu});
		}
		return result;
	}

	/// @brief Enables reading the rendered shadow-map texel back for every sample; costs a GPU stall per frame.
	void RenderSystem::setShadowDepthReadback(bool enabled) { impl_.setGpuDebugReadback(enabled); }

	/// @brief Captures a rendered frame to a PNG file using the selected renderer implementation.
	std::expected<void, Error> RenderSystem::captureFrameToPng(const std::filesystem::path &output_path) {
		return impl_.captureFrameToPng(output_path);
	}

	/// @brief Returns the number of frames accepted by the render system.
	std::uint64_t RenderSystem::renderedFrameCount() const { return impl_.renderedFrameCount(); }

	/// @brief Returns measured render-frame throughput, not the display refresh estimate.
	double RenderSystem::renderingFramesPerSecond() const { return impl_.renderingFramesPerSecond(); }

	/// @brief Returns how many visible windows were considered by the last frame.
	std::size_t RenderSystem::lastRenderedWindowCount() const { return impl_.lastRenderedWindowCount(); }

} // namespace vve
