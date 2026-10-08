module;
#include <VVPPL.h>

export module VVEngine:RenderSystem;
import std;
import :Implementation;
import VVEngine.Error;
import VVEngine.Types;

/**
	* @file
	* @brief Public render-system facade backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

	class RenderSystem {
	public:
		RenderSystem(const RenderSystem &) = default;
		RenderSystem(RenderSystem &&) noexcept = default;
		RenderSystem &operator=(const RenderSystem &) = delete;
		RenderSystem &operator=(RenderSystem &&) noexcept = delete;

		/// @brief Clears the active CPU scene.
		inline auto clearScene() -> void { impl_.clearScene(); }
		/// @brief Loads the standard three-cube sample scene through facade authoring calls.
		[[nodiscard]] inline auto loadSampleScene() -> std::expected<void, Error> {
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
								LightIntensity{.value = 3.0F}, LightRange{.value = 7.0F},
								LinearColor{.value = Vec3{0.18F, 0.18F, 0.18F}});
			setDirectionalLight(Direction{.value = Vec3{-0.55F, -0.78F, 0.30F}},
										LinearColor{.value = Vec3{0.65F, 0.82F, 1.0F}},
										LightIntensity{.value = 0.75F}, LinearColor{.value = Vec3{0.025F, 0.025F, 0.025F}});
			setSpotLight(Position{.value = Vec3{1.45F, 4.8F, -1.45F}},
							  Direction{.value = Vec3{0.10F, -0.98F, -0.16F}},
							  LinearColor{.value = Vec3{1.0F, 0.58F, 0.38F}},
							  LightIntensity{.value = 2.2F}, LightRange{.value = 5.8F}, SpotConeAngle{.radians = 0.58F},
							  LinearColor{.value = Vec3{0.04F, 0.04F, 0.04F}});
			return {};
		}
		/// @brief Sets the post processing setup
		inline auto setPostProcessSetup(std::function<void(vvppl::PostProcessing &)> setup) -> void { impl_.setPostProcessSetup(std::move(setup)); } /**< @note The callback runs once when the renderer starts.
			The PostProcessing reference and the settings references it hands out stay valid until engine shutdown,
			including across resizes, so they may be kept and changed at any time. */
		/// @brief Sets the default camera used by windows without an explicit camera.
		inline auto setCamera(Camera camera) -> void { impl_.setCamera(std::move(camera)); } ///< Sets the default camera; every window without an override uses it with its own aspect ratio.
		/// @brief Overrides the camera for one window, using its drawable extent for the aspect ratio.
		[[nodiscard]] inline auto setCamera(WindowHandle window, Camera camera) -> std::expected<void, Error> {
			return impl_.setCamera(window, std::move(camera));
		} ///< Overrides one window after engine init, even before its first frame; unknown, closed or opted-out windows return invalid_handle.
		/// @brief Clears one window's override so it follows the current default camera again.
		[[nodiscard]] inline auto clearCamera(WindowHandle window) -> std::expected<void, Error> { return impl_.clearCamera(window); } ///< Restores the current default camera; the same window validation as setCamera applies.
		/// @brief Clears all authored and imported lights, preserving objects, resources, cameras and scene instances.
		inline auto clearLights() -> void { impl_.clearLights(); } ///< Removes all lights; objects, resources, cameras and scene instances stay.
		/// @brief Sets the active directional light.
		inline void setDirectionalLight(Direction direction, LinearColor color,
			LightIntensity intensity, LinearColor ambient) {
			impl_.setDirectionalLight(direction, color, intensity, ambient);
		}
		inline void setDirectionalLight(const DirectionalLight &light) {
			setDirectionalLight(light.direction, light.color, light.intensity, light.ambient);
		}																																		///< Applies a directional light descriptor.
		/// @brief Adds a directional light to the active CPU scene.
		inline void addDirectionalLight(Direction direction, LinearColor color,
			LightIntensity intensity, LinearColor ambient) {
			impl_.addDirectionalLight(direction, color, intensity, ambient);
		}
		inline void addDirectionalLight(const DirectionalLight &light) {
			addDirectionalLight(light.direction, light.color, light.intensity, light.ambient);
		}																																		///< Adds a directional light descriptor.
		/// @brief Sets the active point light.
		inline auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) -> void {
			impl_.setPointLight(position, color, intensity, range);
		}
		/// @brief Sets the active point light with ambient lighting.
		inline auto setPointLight(Position position, LinearColor color, LightIntensity intensity,
			LightRange range, LinearColor ambient) -> void {
			impl_.setPointLight(position, color, intensity, range, ambient);
		}
		inline void setPointLight(const PointLight &light) {
			setPointLight(light.position, light.color, light.intensity, light.range, light.ambient);
		}																																		///< Applies a point light descriptor.
		/// @brief Adds a point light to the active CPU scene.
		inline void addPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range) {
			impl_.addPointLight(position, color, intensity, range);
		}
		/// @brief Adds a point light with ambient lighting to the active CPU scene.
		inline void addPointLight(Position position, LinearColor color, LightIntensity intensity,
			LightRange range, LinearColor ambient) {
			impl_.addPointLight(position, color, intensity, range, ambient);
		}
		inline void addPointLight(const PointLight &light) {
			addPointLight(light.position, light.color, light.intensity, light.range, light.ambient);
		}																																		///< Adds a point light descriptor.
		/// @brief Sets the active spotlight.
		inline void setSpotLight(Position position, Direction direction, LinearColor color,
			LightIntensity intensity, LightRange range, SpotConeAngle cone) {
			impl_.setSpotLight(position, direction, color, intensity, range, cone);
		}
		/// @brief Sets the active spotlight with ambient lighting.
		inline void setSpotLight(Position position, Direction direction, LinearColor color,
			LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
			impl_.setSpotLight(position, direction, color, intensity, range, cone, ambient);
		}
		inline void setSpotLight(const SpotLight &light) {
			setSpotLight(light.position, light.direction, light.color, light.intensity, light.range, light.cone, light.ambient);
		}																																		///< Applies a spotlight descriptor.
		/// @brief Adds a spotlight to the active CPU scene.
		inline void addSpotLight(Position position, Direction direction, LinearColor color,
			LightIntensity intensity, LightRange range, SpotConeAngle cone) {
			impl_.addSpotLight(position, direction, color, intensity, range, cone);
		}
		/// @brief Adds a spotlight with ambient lighting to the active CPU scene.
		inline void addSpotLight(Position position, Direction direction, LinearColor color,
			LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient) {
			impl_.addSpotLight(position, direction, color, intensity, range, cone, ambient);
		}
		/// @brief Adds a colored plane and returns its public render-object handle.
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addPlane(Vec2 half_extent, LinearColor color,
			Transform transform = {}) {
			return impl_.addPlane(half_extent, color, transform);
		}
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addPlane(const PlaneDescriptor &plane) {
			return addPlane(plane.half_extent, plane.color, plane.transform);
		}																																		///< Adds a plane descriptor.
		/// @brief Adds a colored cuboid and returns its public render-object handle.
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
			Transform transform = {}) {
			return impl_.addCuboid(minimum, maximum, color, transform);
		}
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addCuboid(const CuboidDescriptor &cuboid) {
			return addCuboid(cuboid.minimum, cuboid.maximum, cuboid.color, cuboid.transform);
		}																																		///< Adds a cuboid descriptor.
		/// @brief Adds a colored indexed triangle mesh and returns its public render-object handle.
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addTriangleMesh(
			Vector<Vec3> positions, Vector<std::uint32_t> indices, LinearColor color,
			Transform transform = {}) {
			return impl_.addTriangleMesh(
				std::move(positions), std::move(indices), color, transform);
		}
		/// @brief Adds an XZ plane with UVs from zero to uv_scale; values above one repeat the texture.
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addTexturedPlane(Vec2 half_extent,
			std::filesystem::path base_color_texture, Vec2 uv_scale = {1.0F, 1.0F}, Transform transform = {}) {
			return impl_.addTexturedPlane(half_extent, std::move(base_color_texture), uv_scale, transform);
		}
		/// @brief Adds a textured cuboid and returns its public render-object handle.
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
			std::filesystem::path base_color_texture,
			Transform transform = {}) {
			return impl_.addTexturedCuboid(minimum, maximum, std::move(base_color_texture), transform);
		}
		[[nodiscard]] inline std::expected<RenderObjectHandle, Error> addTexturedCuboid(const TexturedCuboidDescriptor &cuboid) {
			return addTexturedCuboid(cuboid.minimum, cuboid.maximum, cuboid.base_color_texture, cuboid.transform);
		}																																		///< Adds a textured cuboid descriptor.
		/// @brief Removes one previously added render object.
		[[nodiscard]] inline auto removeObject(RenderObjectHandle handle) -> std::expected<void, Error> {
			return impl_.removeObject(handle);
		}
		/// @brief Sets whether one render object is visible.
		[[nodiscard]] inline auto setObjectVisible(RenderObjectHandle handle, bool visible) -> std::expected<void, Error> {
			return impl_.setObjectVisible(handle, visible);
		}
		/// @brief Sets whether one render object casts shadows.
		[[nodiscard]] inline auto setObjectCastsShadow(RenderObjectHandle handle, bool casts_shadow) -> std::expected<void, Error> {
			return impl_.setObjectCastsShadow(handle, casts_shadow);
		}
		/// @brief Sets whether one render object is drawn in its flat base color without lighting.
		[[nodiscard]] inline auto setObjectUnlit(RenderObjectHandle handle, bool unlit) -> std::expected<void, Error> {
			return impl_.setObjectUnlit(handle, unlit);
		}
		/// @brief Returns whether one render object is visible.
		[[nodiscard]] inline auto objectVisible(RenderObjectHandle handle) const -> std::expected<bool, Error> {
			return impl_.objectVisible(handle);
		}
		/// @brief Sets one render object's source transform.
		[[nodiscard]] inline auto setObjectTransform(RenderObjectHandle handle, Transform transform) -> std::expected<void, Error> {
			return impl_.setObjectTransform(handle, transform);
		}
		/// @brief Returns one render object's source transform.
		[[nodiscard]] inline auto objectTransform(RenderObjectHandle handle) const -> std::expected<Transform, Error> {
			return impl_.objectTransform(handle);
		}
		/// @brief Updates vertex positions for a fixed-topology triangle mesh.
		[[nodiscard]] inline auto setObjectMeshPositions(RenderObjectHandle handle, Vector<Vec3> positions)
			-> std::expected<void, Error> {
			return impl_.setObjectMeshPositions(handle, std::move(positions));
		}
		/// @brief Returns render objects registered for one scene instance.
		[[nodiscard]] inline auto sceneInstanceObjects(RenderSceneInstanceHandle instance) const -> std::expected<Vector<RenderObjectHandle>, Error> {
			return impl_.sceneInstanceObjects(instance);
		}
		/// @brief Returns the scene instance that created one render object.
		[[nodiscard]] inline auto objectSourceScene(RenderObjectHandle handle) const -> std::expected<RenderSceneInstanceHandle, Error> {
			return impl_.objectSourceScene(handle);
		}
		/// @brief Returns the source asset-scene node that created one render object.
		[[nodiscard]] inline auto objectSourceNode(RenderObjectHandle handle) const -> std::expected<NodeHandle, Error> {
			return impl_.objectSourceNode(handle);
		}
		/// @brief Creates a render-scene instance for a loaded scene.
		[[nodiscard]] inline auto instantiateScene(SceneHandle scene, SceneInstantiationOptions options = {}) -> std::expected<RenderSceneInstanceHandle, Error> {
			return impl_.instantiateScene(scene, options);
		}
		/// @brief Removes one render-scene instance and the objects it created.
		[[nodiscard]] inline auto removeSceneInstance(RenderSceneInstanceHandle instance) -> std::expected<void, Error> {
			return impl_.removeSceneInstance(instance);
		}
		/// @brief Removes one loaded scene when no live render object depends on it.
		[[nodiscard]] inline auto removeScene(SceneHandle handle) -> std::expected<void, Error> {
			return impl_.removeScene(handle);
		}
		/// @brief Removes CPU render assets no live render object references.
		[[nodiscard]] inline auto purgeUnusedAssets() -> std::size_t { return impl_.purgeUnusedAssets(); }
		/// @brief Returns mesh count in the active CPU scene.
		[[nodiscard]] inline auto sceneMeshCount() const -> std::size_t { return impl_.sceneMeshCount(); }
		/// @brief Returns material count in the active CPU scene.
		[[nodiscard]] inline auto sceneMaterialCount() const -> std::size_t { return impl_.sceneMaterialCount(); }
		/// @brief Counts distinct live render texture slots shared by object materials.
		[[nodiscard]] inline auto sceneTextureCount() const -> std::size_t { return impl_.sceneTextureCount(); }
		/// @brief Returns directional-light count in the active CPU scene.
		[[nodiscard]] inline auto sceneDirectionalLightCount() const -> std::size_t {
			return impl_.sceneDirectionalLightCount();
		}
		/// @brief Returns point-light count in the active CPU scene.
		[[nodiscard]] inline auto scenePointLightCount() const -> std::size_t { return impl_.scenePointLightCount(); }
		/// @brief Returns spot-light count in the active CPU scene.
		[[nodiscard]] inline auto sceneSpotLightCount() const -> std::size_t { return impl_.sceneSpotLightCount(); }
		/// @brief Returns imported-camera count in the active CPU scene.
		[[nodiscard]] inline auto sceneCameraCount() const -> std::size_t { return impl_.sceneCameraCount(); }
		/// @brief Returns instance count in the active CPU scene.
		[[nodiscard]] inline auto sceneInstanceCount() const -> std::size_t { return impl_.sceneInstanceCount(); }
		/// @brief Returns source vertex count in the active CPU scene.
		[[nodiscard]] inline auto sceneVertexCount() const -> std::size_t { return impl_.sceneVertexCount(); }
		/// @brief Returns source index count in the active CPU scene.
		[[nodiscard]] inline auto sceneIndexCount() const -> std::size_t { return impl_.sceneIndexCount(); }
		/// @brief Reports whether the active CPU scene has a camera.
		[[nodiscard]] inline auto hasSceneCamera() const -> bool { return impl_.hasSceneCamera(); }
		/// @brief Reports whether the active CPU scene has a directional light.
		[[nodiscard]] inline auto hasSceneDirectionalLight() const -> bool { return impl_.hasSceneDirectionalLight(); }
		/// @brief Reports whether the active CPU scene has a point light.
		[[nodiscard]] inline auto hasScenePointLight() const -> bool { return impl_.hasScenePointLight(); }
		/// @brief Reports whether the active CPU scene has a spotlight.
		[[nodiscard]] inline auto hasSceneSpotLight() const -> bool { return impl_.hasSceneSpotLight(); }
		/// @brief Copies the last rendered frame's samples; empty unless setShadowDepthReadback(true) enabled readback.
		[[nodiscard]] inline auto shadowDepthSamples() const -> Vector<RenderShadowDepthSample> {
			const auto samples = impl_.shadowDepthSamples();
			return {samples.begin(), samples.end()};
		} ///< Empty unless setShadowDepthReadback(true) enabled samples for the rendered frame.
		/// @brief Enables reading the rendered shadow-map texel back for every sample; costs a GPU stall per frame.
		inline auto setShadowDepthReadback(bool enabled) -> void { impl_.setGpuDebugReadback(enabled); }
		/// @brief Captures the selected rendered window; unknown, opted-out or closed windows return invalid_handle.
		[[nodiscard]] inline auto captureFrameToPng(WindowHandle window, const std::filesystem::path &output_path) -> std::expected<void, Error> {
			return impl_.captureFrameToPng(window, output_path);
		}
		/// @brief Captures a rendered frame to a PNG file using the selected renderer implementation.
		[[nodiscard]] inline auto captureFrameToPng(const std::filesystem::path &output_path) -> std::expected<void, Error> {
			return impl_.captureFrameToPng(output_path);
		}
		/// @brief Returns the number of frames accepted by the render system.
		[[nodiscard]] inline auto renderedFrameCount() const -> std::uint64_t { return impl_.renderedFrameCount(); }
		/// @brief Returns measured render-frame throughput, not the display refresh estimate.
		[[nodiscard]] inline auto renderingFramesPerSecond() const -> double { return impl_.renderingFramesPerSecond(); }
		/// @brief Returns how many visible windows were considered by the last frame.
		[[nodiscard]] inline auto lastRenderedWindowCount() const -> std::size_t { return impl_.lastRenderedWindowCount(); }

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::RenderSystemImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit RenderSystem(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public render-system wrapper.

} // namespace vve
