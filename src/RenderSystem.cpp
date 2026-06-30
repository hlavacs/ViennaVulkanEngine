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

	/// @brief Returns how many visible windows were considered by the last frame.
	std::size_t RenderSystem::lastRenderedWindowCount() const { return renderSystemImpl(impl_).lastRenderedWindowCount(); }

} // namespace vve
