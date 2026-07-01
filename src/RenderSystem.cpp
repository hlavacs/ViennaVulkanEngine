module VEEngine;
import :RenderSystem;
import VEEngine.Simple;

namespace vve {

	namespace {
		/// @brief Recovers the selected implementation render system from the erased facade pointer.
		[[nodiscard]] auto renderSystemImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem *>(implementation);
		}

	} // namespace

	/// @brief Stores the erased implementation reference used by engine-owned render systems.
	RenderSystem::RenderSystem(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Clears the active CPU scene.
	void RenderSystem::clearScene() { renderSystemImpl(impl_).clearScene(); }

	/// @brief Loads the standard three-cube sample scene through facade authoring calls.
	std::expected<void, Error> RenderSystem::loadSampleScene() {
		clearScene();
		const auto minimum = Vec3{-0.5F, -0.5F, -0.5F};
		const auto maximum = Vec3{0.5F, 0.5F, 0.5F};
		const auto cube_color = LinearColor{.value = Vec3{0.55F, 0.55F, 0.55F}};
		if (auto result = addCuboid(minimum, maximum, cube_color); !result) { return result; }
		if (auto result = addCuboid(minimum, maximum, cube_color,
												Transform{.translation = Position{.value = Vec3{-1.5F, 0.0F, 0.0F}}});
			 !result) {
			return result;
		}
		if (auto result = addCuboid(minimum, maximum, cube_color,
												Transform{.translation = Position{.value = Vec3{1.5F, 0.0F, 0.0F}}});
			 !result) {
			return result;
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
	void RenderSystem::setCamera(Camera camera, PixelExtent extent) { renderSystemImpl(impl_).setCamera(std::move(camera), extent); }

	/// @brief Sets the active directional light.
	void RenderSystem::setDirectionalLight(Direction direction_to_light, LinearColor color,
														LightIntensity intensity, LinearColor ambient) {
		renderSystemImpl(impl_).setDirectionalLight(direction_to_light, color, intensity, ambient);
	}

	/// @brief Adds a directional light to the active CPU scene.
	void RenderSystem::addDirectionalLight(Direction direction_to_light, LinearColor color,
														LightIntensity intensity, LinearColor ambient) {
		renderSystemImpl(impl_).addDirectionalLight(direction_to_light, color, intensity, ambient);
	}

	/// @brief Sets the active point light.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
		renderSystemImpl(impl_).setPointLight(position, color, intensity, range);
	}

	/// @brief Sets the active point light with ambient lighting.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range,
											 LinearColor ambient) {
		renderSystemImpl(impl_).setPointLight(position, color, intensity, range, ambient);
	}

	/// @brief Adds a point light to the active CPU scene.
	void RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
		renderSystemImpl(impl_).addPointLight(position, color, intensity, range);
	}

	/// @brief Adds a point light with ambient lighting to the active CPU scene.
	void RenderSystem::addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range,
											 LinearColor ambient) {
		renderSystemImpl(impl_).addPointLight(position, color, intensity, range, ambient);
	}

	/// @brief Sets the active spotlight.
	void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		renderSystemImpl(impl_).setSpotLight(position, direction, color, intensity, range, cone);
	}

	/// @brief Sets the active spotlight with ambient lighting.
	void RenderSystem::setSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
		renderSystemImpl(impl_).setSpotLight(position, direction, color, intensity, range, cone, ambient);
	}

	/// @brief Adds a spotlight to the active CPU scene.
	void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone) {
		renderSystemImpl(impl_).addSpotLight(position, direction, color, intensity, range, cone);
	}

	/// @brief Adds a spotlight with ambient lighting to the active CPU scene.
	void RenderSystem::addSpotLight(Position position, Direction direction, LinearColor color,
											  LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
		renderSystemImpl(impl_).addSpotLight(position, direction, color, intensity, range, cone, ambient);
	}

	/// @brief Adds a colored plane to the active CPU scene.
	std::expected<void, Error> RenderSystem::addPlane(Vec2 half_extent, LinearColor color, Transform transform) {
		return renderSystemImpl(impl_).addPlane(half_extent, color, transform);
	}

	/// @brief Adds a colored cuboid to the active CPU scene.
	std::expected<void, Error> RenderSystem::addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																		Transform transform) {
		return renderSystemImpl(impl_).addCuboid(minimum, maximum, color, transform);
	}

	/// @brief Adds a cuboid using a base-color texture supplied by the application.
	std::expected<void, Error> RenderSystem::addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																				std::filesystem::path base_color_texture,
																				Transform transform) {
		return renderSystemImpl(impl_).addTexturedCuboid(minimum, maximum, std::move(base_color_texture), transform);
	}

	/// @brief Returns mesh count in the active CPU scene.
	std::size_t RenderSystem::sceneMeshCount() const { return renderSystemImpl(impl_).sceneMeshCount(); }

	/// @brief Returns material count in the active CPU scene.
	std::size_t RenderSystem::sceneMaterialCount() const { return renderSystemImpl(impl_).sceneMaterialCount(); }

	/// @brief Returns instance count in the active CPU scene.
	std::size_t RenderSystem::sceneInstanceCount() const { return renderSystemImpl(impl_).sceneInstanceCount(); }

	/// @brief Returns source vertex count in the active CPU scene.
	std::size_t RenderSystem::sceneVertexCount() const { return renderSystemImpl(impl_).sceneVertexCount(); }

	/// @brief Returns source index count in the active CPU scene.
	std::size_t RenderSystem::sceneIndexCount() const { return renderSystemImpl(impl_).sceneIndexCount(); }

	/// @brief Returns directional shadow-depth debug sample count in the active scene.
	std::size_t RenderSystem::sceneShadowDepthSampleCount() const {
		return renderSystemImpl(impl_).sceneShadowDepthSampleCount();
	}

	/// @brief Returns one directional shadow-depth debug sample converted to facade data.
	std::optional<RenderShadowDepthSample> RenderSystem::sceneShadowDepthSample(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneShadowDepthSample(index);
		if (!sample) { return std::nullopt; }
		return RenderShadowDepthSample{.triangle_id = sample->triangle_id,
												 .face_index = sample->face_index,
												 .world = sample->world,
												 .light_ndc = sample->light_ndc,
												 .pixel_x = sample->pixel_x,
												 .pixel_y = sample->pixel_y,
												 .expected_depth = sample->expected_depth,
												 .bias = sample->bias,
												 .shadow_factor = sample->shadow_factor,
												 .gpu_depth = sample->gpu_depth,
												 .error = sample->error,
												 .has_gpu = sample->has_gpu,
												 .valid = sample->valid};
	}

	/// @brief Returns spot-light shadow-depth debug sample count in the active scene.
	std::size_t RenderSystem::sceneSpotShadowDepthSampleCount() const {
		return renderSystemImpl(impl_).sceneSpotShadowDepthSampleCount();
	}

	/// @brief Returns one spot-light shadow-depth debug sample converted to facade data.
	std::optional<SpotShadowDepthSample> RenderSystem::sceneSpotShadowDepthSample(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		if (!sample) { return std::nullopt; }
		return SpotShadowDepthSample{.triangle_id = sample->triangle_id,
											  .face_index = sample->face_index,
											  .world = sample->world,
											  .light_ndc = sample->light_ndc,
											  .pixel_x = sample->pixel_x,
											  .pixel_y = sample->pixel_y,
											  .expected_depth = sample->expected_depth,
											  .bias = sample->bias,
											  .shadow_factor = sample->shadow_factor,
											  .gpu_depth = sample->gpu_depth,
											  .error = sample->error,
											  .has_gpu = sample->has_gpu,
											  .valid = sample->valid};
	}

	/// @brief Returns the selected spot shadow slot for one debug sample.
	std::optional<std::uint32_t> RenderSystem::sceneSpotShadowDepthSampleSlot(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		return sample ? std::optional<std::uint32_t>{sample->face_index} : std::nullopt;
	}

	/// @brief Returns the CPU-computed spot light-space depth for one debug sample.
	std::optional<float> RenderSystem::sceneSpotShadowDepthSampleExpectedDepth(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		return sample ? std::optional<float>{sample->expected_depth} : std::nullopt;
	}

	/// @brief Returns the compare bias used by one spot shadow debug sample.
	std::optional<float> RenderSystem::sceneSpotShadowDepthSampleBias(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		return sample ? std::optional<float>{sample->bias} : std::nullopt;
	}

	/// @brief Returns the final shadow factor for one spot shadow debug sample.
	std::optional<float> RenderSystem::sceneSpotShadowDepthSampleFactor(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		return sample ? std::optional<float>{sample->shadow_factor} : std::nullopt;
	}

	/// @brief Returns the downloaded GPU spot shadow depth for one debug sample.
	std::optional<float> RenderSystem::sceneSpotShadowDepthGpuDepth(std::size_t index) const {
		return renderSystemImpl(impl_).sceneSpotShadowDepthGpuDepth(index);
	}

	/// @brief Reports whether one spot shadow debug sample has downloaded GPU depth.
	std::optional<bool> RenderSystem::sceneSpotShadowDepthHasGpu(std::size_t index) const {
		return renderSystemImpl(impl_).sceneSpotShadowDepthHasGpu(index);
	}

	/// @brief Returns the absolute CPU/GPU spot shadow depth mismatch for one debug sample.
	std::optional<float> RenderSystem::sceneSpotShadowDepthError(std::size_t index) const {
		return renderSystemImpl(impl_).sceneSpotShadowDepthError(index);
	}

	/// @brief Returns point-light shadow-depth debug sample count in the active scene.
	std::size_t RenderSystem::scenePointShadowDepthSampleCount() const {
		return renderSystemImpl(impl_).scenePointShadowDepthSampleCount();
	}

	/// @brief Returns one point-light shadow-depth debug sample converted to facade data.
	std::optional<RenderShadowDepthSample> RenderSystem::scenePointShadowDepthSample(std::size_t index) const {
		const auto sample = renderSystemImpl(impl_).scenePointShadowDepthSample(index);
		if (!sample) { return std::nullopt; }
		return RenderShadowDepthSample{.triangle_id = sample->triangle_id,
												 .face_index = sample->face_index,
												 .world = sample->world,
												 .light_ndc = sample->light_ndc,
												 .pixel_x = sample->pixel_x,
												 .pixel_y = sample->pixel_y,
												 .expected_depth = sample->expected_depth,
												 .bias = sample->bias,
												 .shadow_factor = sample->shadow_factor,
												 .gpu_depth = sample->gpu_depth,
												 .error = sample->error,
												 .has_gpu = sample->has_gpu,
												 .valid = sample->valid};
	}

	/// @brief Returns the downloaded GPU point shadow depth for one debug sample.
	std::optional<float> RenderSystem::scenePointShadowDepthGpuDepth(std::size_t index) const {
		return renderSystemImpl(impl_).scenePointShadowDepthGpuDepth(index);
	}

	/// @brief Reports whether one point shadow debug sample has downloaded GPU depth.
	std::optional<bool> RenderSystem::scenePointShadowDepthHasGpu(std::size_t index) const {
		return renderSystemImpl(impl_).scenePointShadowDepthHasGpu(index);
	}

	/// @brief Returns the absolute CPU/GPU point shadow depth mismatch for one debug sample.
	std::optional<float> RenderSystem::scenePointShadowDepthError(std::size_t index) const {
		return renderSystemImpl(impl_).scenePointShadowDepthError(index);
	}

	/// @brief Reports whether the active CPU scene has a camera.
	bool RenderSystem::hasSceneCamera() const { return renderSystemImpl(impl_).hasSceneCamera(); }

	/// @brief Reports whether the active CPU scene has a directional light.
	bool RenderSystem::hasSceneDirectionalLight() const { return renderSystemImpl(impl_).hasSceneDirectionalLight(); }

	/// @brief Reports whether the active CPU scene has a point light.
	bool RenderSystem::hasScenePointLight() const { return renderSystemImpl(impl_).hasScenePointLight(); }

	/// @brief Reports whether the active CPU scene has a spotlight.
	bool RenderSystem::hasSceneSpotLight() const { return renderSystemImpl(impl_).hasSceneSpotLight(); }

	/// @brief Captures a rendered frame to a PNG file using the selected renderer implementation.
	std::expected<void, Error> RenderSystem::captureFrameToPng(const std::filesystem::path &output_path) {
		return renderSystemImpl(impl_).captureFrameToPng(output_path);
	}

	/// @brief Returns the number of frames accepted by the render system.
	std::uint64_t RenderSystem::renderedFrameCount() const { return renderSystemImpl(impl_).renderedFrameCount(); }

	/// @brief Returns how many visible windows were considered by the last frame.
	std::size_t RenderSystem::lastRenderedWindowCount() const { return renderSystemImpl(impl_).lastRenderedWindowCount(); }

} // namespace vve
