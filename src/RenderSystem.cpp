module VEEngine;
import :RenderSystem;
import VEEngine.Simple;

namespace vve {

	namespace {
		/// @brief Recovers the selected implementation render system from the erased facade pointer.
		[[nodiscard]] auto renderSystemImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem *>(implementation);
		}

		/// @brief Converts implementation render debug samples into facade debug samples.
		template <typename TSample> [[nodiscard]] auto facadeDebugSample(const TSample &sample) -> RenderDebugSample {
			return RenderDebugSample{.vertex_id = sample.vertex_id, .world = sample.world, .clip = sample.clip,
											 .light_clip = sample.light_clip, .spot_light_clip = sample.spot_light_clip,
											 .point_light_clip = sample.point_light_clip, .ndc = sample.ndc,
											 .light_ndc = sample.light_ndc, .spot_light_ndc = sample.spot_light_ndc,
											 .point_light_ndc = sample.point_light_ndc, .normal = sample.normal,
											 .direction_to_light = sample.direction_to_light,
											 .ambient_lighting = sample.ambient_lighting,
											 .direct_lighting = sample.direct_lighting, .point_lighting = sample.point_lighting,
											 .spot_lighting = sample.spot_lighting, .final_lighting = sample.final_lighting,
											 .depth = sample.depth, .light_depth = sample.light_depth,
											 .spot_light_depth = sample.spot_light_depth,
											 .point_light_depth = sample.point_light_depth,
											 .sampled_shadow_depth = sample.sampled_shadow_depth,
											 .shadow_depth_delta = sample.shadow_depth_delta, .shadow_bias = sample.shadow_bias,
											 .shadow_factor = sample.shadow_factor,
											 .sampled_spot_shadow_depth = sample.sampled_spot_shadow_depth,
											 .spot_shadow_depth_delta = sample.spot_shadow_depth_delta,
											 .spot_shadow_bias = sample.spot_shadow_bias,
											 .spot_shadow_factor = sample.spot_shadow_factor,
											 .sampled_point_shadow_depth = sample.sampled_point_shadow_depth,
											 .point_shadow_depth_delta = sample.point_shadow_depth_delta,
											 .point_shadow_bias = sample.point_shadow_bias,
											 .point_shadow_factor = sample.point_shadow_factor,
											 .point_shadow_face = sample.point_shadow_face, .n_dot_l = sample.n_dot_l,
											 .inside_light = sample.inside_light, .inside_spot_light = sample.inside_spot_light,
											 .inside_point_light = sample.inside_point_light, .valid = sample.valid};
		}

		/// @brief Converts implementation shadow-depth samples into facade debug samples.
		template <typename TSample> [[nodiscard]] auto facadeShadowDepthSample(const TSample &sample) -> RenderShadowDepthSample {
			return RenderShadowDepthSample{.triangle_id = sample.triangle_id, .face_index = sample.face_index,
													 .world = sample.world, .light_ndc = sample.light_ndc,
													 .pixel_x = sample.pixel_x, .pixel_y = sample.pixel_y,
													 .expected_depth = sample.expected_depth, .gpu_depth = sample.gpu_depth,
													 .error = sample.error, .has_gpu = sample.has_gpu, .valid = sample.valid};
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

	/// @brief Sets the active point light.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
		renderSystemImpl(impl_).setPointLight(position, color, intensity, range);
	}

	/// @brief Sets the active point light with ambient lighting.
	void RenderSystem::setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range,
											 LinearColor ambient) {
		renderSystemImpl(impl_).setPointLight(position, color, intensity, range, ambient);
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

	/// @brief Returns the number of frames presented by the render system.
	std::uint64_t RenderSystem::presentedFrameCount() const { return renderSystemImpl(impl_).presentedFrameCount(); }

	/// @brief Returns the number of smoke-test triangle draws.
	std::uint64_t RenderSystem::triangleDrawCount() const { return renderSystemImpl(impl_).triangleDrawCount(); }

	/// @brief Returns the number of smoke-test triangle vertices.
	std::uint32_t RenderSystem::triangleVertexCount() const { return renderSystemImpl(impl_).triangleVertexCount(); }

	/// @brief Returns how many scene uploads completed.
	std::uint64_t RenderSystem::sceneUploadCount() const { return renderSystemImpl(impl_).sceneUploadCount(); }

	/// @brief Returns how many source meshes were drawn by the uploaded scene path.
	std::uint64_t RenderSystem::sceneMeshDrawCount() const { return renderSystemImpl(impl_).sceneMeshDrawCount(); }

	/// @brief Returns how many source instances were drawn by the uploaded scene path.
	std::uint64_t RenderSystem::sceneInstanceDrawCount() const { return renderSystemImpl(impl_).sceneInstanceDrawCount(); }

	/// @brief Returns how many vertices were uploaded by the scene draw path.
	std::uint32_t RenderSystem::sceneDrawVertexCount() const { return renderSystemImpl(impl_).sceneDrawVertexCount(); }

	/// @brief Returns how many indices were uploaded by the scene draw path.
	std::uint32_t RenderSystem::sceneDrawIndexCount() const { return renderSystemImpl(impl_).sceneDrawIndexCount(); }

	/// @brief Returns how many debug sample slots are expected for the scene draw.
	std::size_t RenderSystem::sceneDebugSampleCount() const { return renderSystemImpl(impl_).sceneDebugSampleCount(); }

	/// @brief Returns one CPU-computed render debug sample.
	auto RenderSystem::sceneCpuDebugSample(std::size_t index) const -> std::optional<RenderDebugSample> {
		auto sample = renderSystemImpl(impl_).sceneCpuDebugSample(index);
		if (!sample) { return {}; }
		return facadeDebugSample(*sample);
	}

	/// @brief Returns one GPU-computed render debug sample.
	auto RenderSystem::sceneGpuDebugSample(std::size_t index) const -> std::optional<RenderDebugSample> {
		auto sample = renderSystemImpl(impl_).sceneGpuDebugSample(index);
		if (!sample) { return {}; }
		return facadeDebugSample(*sample);
	}

	/// @brief Returns the CPU/GPU clip-space mismatch for one sample.
	auto RenderSystem::sceneDebugClipError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugClipError(index);
	}

	/// @brief Returns the CPU/GPU depth mismatch for one sample.
	auto RenderSystem::sceneDebugDepthError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugDepthError(index);
	}

	/// @brief Returns the CPU/GPU directional-light-space mismatch for one sample.
	auto RenderSystem::sceneDebugLightSpaceError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugLightSpaceError(index);
	}

	/// @brief Returns the CPU/GPU spot-light-space mismatch for one sample.
	auto RenderSystem::sceneDebugSpotLightSpaceError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugSpotLightSpaceError(index);
	}

	/// @brief Returns the CPU/GPU point-light-space mismatch for one sample.
	auto RenderSystem::sceneDebugPointLightSpaceError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugPointLightSpaceError(index);
	}

	/// @brief Returns the CPU/GPU lighting-term mismatch for one sample.
	auto RenderSystem::sceneDebugLightingError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugLightingError(index);
	}

	/// @brief Returns the shader-sampled shadow depth mismatch against the copied shadow map.
	auto RenderSystem::sceneDebugShadowSampleError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugShadowSampleError(index);
	}

	/// @brief Returns the shader-sampled spot shadow depth mismatch against the copied spot map.
	auto RenderSystem::sceneDebugSpotShadowSampleError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugSpotShadowSampleError(index);
	}

	/// @brief Returns the shader-sampled point shadow depth mismatch against the copied point map.
	auto RenderSystem::sceneDebugPointShadowSampleError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneDebugPointShadowSampleError(index);
	}

	/// @brief Returns how many shadow-depth proof samples are available.
	auto RenderSystem::sceneShadowDepthSampleCount() const -> std::size_t {
		return renderSystemImpl(impl_).sceneShadowDepthSampleCount();
	}

	/// @brief Returns one downloaded shadow-depth proof sample.
	auto RenderSystem::sceneShadowDepthSample(std::size_t index) const -> std::optional<RenderShadowDepthSample> {
		const auto sample = renderSystemImpl(impl_).sceneShadowDepthSample(index);
		if (!sample) { return {}; }
		return facadeShadowDepthSample(*sample);
	}

	/// @brief Returns the CPU/GPU shadow-depth mismatch for one proof sample.
	auto RenderSystem::sceneShadowDepthError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneShadowDepthError(index);
	}

	/// @brief Returns how many spot shadow-depth proof samples are available.
	auto RenderSystem::sceneSpotShadowDepthSampleCount() const -> std::size_t {
		return renderSystemImpl(impl_).sceneSpotShadowDepthSampleCount();
	}

	/// @brief Returns one downloaded spot shadow-depth proof sample.
	auto RenderSystem::sceneSpotShadowDepthSample(std::size_t index) const -> std::optional<RenderShadowDepthSample> {
		const auto sample = renderSystemImpl(impl_).sceneSpotShadowDepthSample(index);
		if (!sample) { return {}; }
		return facadeShadowDepthSample(*sample);
	}

	/// @brief Returns the CPU/GPU spot shadow-depth mismatch for one proof sample.
	auto RenderSystem::sceneSpotShadowDepthError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).sceneSpotShadowDepthError(index);
	}

	/// @brief Returns how many point shadow-depth proof samples are available.
	auto RenderSystem::scenePointShadowDepthSampleCount() const -> std::size_t {
		return renderSystemImpl(impl_).scenePointShadowDepthSampleCount();
	}

	/// @brief Returns one downloaded point shadow-depth proof sample.
	auto RenderSystem::scenePointShadowDepthSample(std::size_t index) const -> std::optional<RenderShadowDepthSample> {
		const auto sample = renderSystemImpl(impl_).scenePointShadowDepthSample(index);
		if (!sample) { return {}; }
		return facadeShadowDepthSample(*sample);
	}

	/// @brief Returns the CPU/GPU point shadow-depth mismatch for one proof sample.
	auto RenderSystem::scenePointShadowDepthError(std::size_t index) const -> std::optional<float> {
		return renderSystemImpl(impl_).scenePointShadowDepthError(index);
	}

	/// @brief Returns how many visible windows were considered by the last frame.
	std::size_t RenderSystem::lastRenderedWindowCount() const { return renderSystemImpl(impl_).lastRenderedWindowCount(); }

	/// @brief Returns how many Vulkan targets are prepared.
	std::size_t RenderSystem::preparedGpuTargetCount() const { return renderSystemImpl(impl_).preparedGpuTargetCount(); }

	/// @brief Returns the last clear color used by the renderer.
	std::array<float, 4> RenderSystem::lastClearColor() const { return renderSystemImpl(impl_).lastClearColor(); }

} // namespace vve
