export module VEEngine.Types;
import std;
export import VEEngine.Error;
export import VEEngine.Handle;
export import VEEngine.Math;
export import VEEngine.Vector;
import VEEngine.Entity;

/**
	* @file
	* @brief Public data and descriptor contract declared by the facade layer.
	*/
export namespace vve {

	using vve::EntityTag;												///< Facade ECS entity handle tag.
	using vve::Entity;													///< Facade ECS entity.

	struct SceneHandleTag {};											///< Scene descriptor handle tag.
	struct WindowHandleTag {};											///< Runtime window handle tag.
	struct NodeHandleTag {};											///< Node descriptor handle tag.
	struct MeshHandleTag {};											///< Mesh descriptor handle tag.
	struct MaterialHandleTag {};										///< Material descriptor handle tag.
	struct TextureHandleTag {};										///< Texture descriptor handle tag.
	struct RenderObjectHandleTag {};								///< Render object handle tag.
	struct RenderSceneInstanceHandleTag {};						///< Render scene instance handle tag.
	struct LightHandleTag {};											///< Light descriptor handle tag.
	struct CameraHandleTag {};											///< Camera descriptor handle tag.

	using SceneHandle	= TypedHandle<SceneHandleTag>;		///< Scene descriptor handle.
	using WindowHandle	= TypedHandle<WindowHandleTag>;		///< Runtime window handle.
	using NodeHandle	= TypedHandle<NodeHandleTag>;			///< Node descriptor handle.
	using MeshHandle	= TypedHandle<MeshHandleTag>;			///< Mesh descriptor handle.
	using MaterialHandle = TypedHandle<MaterialHandleTag>;	///< Material descriptor handle.
	using TextureHandle	= TypedHandle<TextureHandleTag>;		///< Texture descriptor handle.
	using RenderObjectHandle = TypedHandle<RenderObjectHandleTag>;	///< Render object handle.
	using RenderSceneInstanceHandle = TypedHandle<RenderSceneInstanceHandleTag>;	///< Render scene instance handle.
	using LightHandle	= TypedHandle<LightHandleTag>;		///< Light descriptor handle.
	using CameraHandle	= TypedHandle<CameraHandleTag>;		///< Camera descriptor handle.

	/// @brief Strong wrapper for world or local position values.
	struct Position {
		Vec3 value{zeroVec3()};													///< Wrapped coordinate.
	};

	/// @brief Strong wrapper for vectors that should be interpreted as directions.
	struct Direction {
		Vec3 value{Vec3(zero(), zero(), -one())};							///< Wrapped direction.
	};

	/// @brief Strong wrapper for non-uniform scale factors.
	struct Scale {
		Vec3 value{oneVec3()};													///< Wrapped scale vector.
	};

	/// @brief Strong wrapper for quaternion rotations.
	struct Rotation {
		Quat value{identityQuat()};											///< Wrapped orientation.
	};

	/// @brief Strong wrapper for linear RGB color values.
	struct LinearColor {
		Vec3 value{oneVec3()};													///< Wrapped linear RGB color.
	};

	/// @brief Strong wrapper for relative light intensity.
	struct LightIntensity {
		Scalar value{one()};														///< Wrapped non-negative intensity scale.
	};

	/// @brief Strong wrapper for finite light influence distance.
	struct LightRange {
		Scalar value{static_cast<Scalar>(10)};								///< Wrapped range in world units.
	};

	/// @brief Strong wrapper for spotlight outer cone angle.
	struct SpotConeAngle {
		Scalar radians{static_cast<Scalar>(0.75)};						///< Wrapped outer cone angle in radians.
	};

	/// @brief Public imported-light category understood by the asset facade.
	enum class LightKind {
		directional,																///< Infinite light with direction only.
		point,																		///< Positional light radiating in all directions.
		spot																			///< Positional light constrained by a cone.
	};

	/// @brief Facade descriptor for imported light data stored by the asset system.
	struct LightDescriptor {
		LightKind kind{LightKind::point};									///< Imported light category.
		LinearColor color{};													///< Linear RGB light color.
		LightIntensity intensity{};											///< Imported or derived intensity scale.
		Direction direction{};													///< Local light direction when available.
		Position position{};													///< Local light position when available.
		LightRange range{};														///< Finite influence range for point and spot lights.
		SpotConeAngle cone{};													///< Outer cone angle for spot lights.
	};

	/// @brief Facade descriptor for directional light setup.
	struct DirectionalLight {
		Direction direction{};													///< Direction from surfaces toward the light.
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Facade descriptor for point light setup.
	struct PointLight {
		Position position{};													///< World-space light position.
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LightRange range{};														///< Finite influence range.
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Facade descriptor for spotlight setup.
	struct SpotLight {
		Position position{};													///< World-space light position.
		Direction direction{};													///< World-space spotlight direction.
		LinearColor color{};													///< Linear RGB direct light color.
		LightIntensity intensity{};											///< Direct light intensity scale.
		LightRange range{};														///< Finite influence range.
		SpotConeAngle cone{};													///< Outer cone angle.
		LinearColor ambient{.value = Vec3(static_cast<Scalar>(0.04), static_cast<Scalar>(0.04),
													 static_cast<Scalar>(0.04))};	///< Linear RGB ambient contribution.
	};

	/// @brief Chainable facade builder for directional light setup.
	class DirectionalLightBuilder {
	public:
		inline DirectionalLightBuilder() = default;

		[[nodiscard]] inline DirectionalLightBuilder &direction(Direction value) {
			direction_ = value;
			return *this;
		}
		[[nodiscard]] inline DirectionalLightBuilder &color(LinearColor value) {
			color_ = value;
			return *this;
		}
		[[nodiscard]] inline DirectionalLightBuilder &intensity(LightIntensity value) {
			intensity_ = value;
			return *this;
		}
		[[nodiscard]] inline DirectionalLightBuilder &ambient(LinearColor value) {
			ambient_ = value;
			return *this;
		}
		[[nodiscard]] inline DirectionalLight build() const {
			return DirectionalLight{.direction = direction_, .color = color_, .intensity = intensity_, .ambient = ambient_};
		}

	private:
		Direction direction_{};													///< Direction from surfaces toward the light.
		LinearColor color_{};													///< Linear RGB direct light color.
		LightIntensity intensity_{};											///< Direct light intensity scale.
		LinearColor ambient_{DirectionalLight{}.ambient};				///< Linear RGB ambient contribution.
	};	///< Public directional light builder using facade-only light types.

	/// @brief Chainable facade builder for point light setup.
	class PointLightBuilder {
	public:
		inline PointLightBuilder() = default;

		[[nodiscard]] inline PointLightBuilder &position(Position value) {
			position_ = value;
			return *this;
		}
		[[nodiscard]] inline PointLightBuilder &color(LinearColor value) {
			color_ = value;
			return *this;
		}
		[[nodiscard]] inline PointLightBuilder &intensity(LightIntensity value) {
			intensity_ = value;
			return *this;
		}
		[[nodiscard]] inline PointLightBuilder &range(LightRange value) {
			range_ = value;
			return *this;
		}
		[[nodiscard]] inline PointLightBuilder &ambient(LinearColor value) {
			ambient_ = value;
			return *this;
		}
		[[nodiscard]] inline PointLight build() const {
			return PointLight{.position = position_, .color = color_, .intensity = intensity_,
									.range = range_, .ambient = ambient_};
		}

	private:
		Position position_{};													///< World-space light position.
		LinearColor color_{};													///< Linear RGB direct light color.
		LightIntensity intensity_{};											///< Direct light intensity scale.
		LightRange range_{};														///< Finite influence range.
		LinearColor ambient_{PointLight{}.ambient};						///< Linear RGB ambient contribution.
	};	///< Public point light builder using facade-only light types.

	/// @brief Chainable facade builder for spotlight setup.
	class SpotLightBuilder {
	public:
		inline SpotLightBuilder() = default;

		[[nodiscard]] inline SpotLightBuilder &position(Position value) {
			position_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &direction(Direction value) {
			direction_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &color(LinearColor value) {
			color_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &intensity(LightIntensity value) {
			intensity_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &range(LightRange value) {
			range_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &cone(SpotConeAngle value) {
			cone_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLightBuilder &ambient(LinearColor value) {
			ambient_ = value;
			return *this;
		}
		[[nodiscard]] inline SpotLight build() const {
			return SpotLight{.position = position_, .direction = direction_, .color = color_, .intensity = intensity_,
								  .range = range_, .cone = cone_, .ambient = ambient_};
		}

	private:
		Position position_{};													///< World-space light position.
		Direction direction_{};													///< World-space spotlight direction.
		LinearColor color_{};													///< Linear RGB direct light color.
		LightIntensity intensity_{};											///< Direct light intensity scale.
		LightRange range_{};														///< Finite influence range.
		SpotConeAngle cone_{};													///< Outer cone angle.
		LinearColor ambient_{SpotLight{}.ambient};						///< Linear RGB ambient contribution.
	};	///< Public spotlight builder using facade-only light types.

	/// @brief Strong wrapper for vertical field-of-view angles.
	struct FovY {
		Scalar radians{static_cast<Scalar>(1.0471975511965976)};		///< Wrapped vertical FOV in radians.
	};

	/// @brief Facade descriptor for imported camera data stored by the asset system.
	struct CameraDescriptor {
		Position position{};													///< Local camera position when available.
		Direction direction{};													///< Local camera look direction.
		Direction up{.value = Vec3(zero(), one(), zero())};			///< Local camera up direction.
		FovY fov{};																///< Vertical field-of-view angle.
		Scalar aspect{one()};													///< Projection aspect ratio.
		Scalar near_clip{static_cast<Scalar>(0.1)};					///< Near clipping distance.
		Scalar far_clip{static_cast<Scalar>(10000.0)};				///< Far clipping distance.
	};

	/// @brief Strong wrapper for near and far clipping planes.
	struct ClipPlanes {
		Scalar near_plane{static_cast<Scalar>(0.1)};						///< Near clip distance.
		Scalar far_plane{static_cast<Scalar>(10000.0)};					///< Far clip distance.
	};

	/// @brief Strong wrapper for frame delta time.
	struct DeltaTime {
		double seconds{1.0 / 60.0};											///< Elapsed seconds.
	};

	/// @brief Strong wrapper for pixel dimensions.
	struct PixelExtent {
		std::uint32_t width{0};													///< Width in pixels.
		std::uint32_t height{0};												///< Height in pixels.
	};

	/// @brief Strong wrapper for human-readable object names.
	struct ObjectName {
		std::string value{};														///< Wrapped display or diagnostic name.
	};

	/// @brief Strong wrapper for renderer selection identifiers.
	struct RendererId {
		std::string value{};														///< Wrapped renderer identifier.
	};

	/// @brief Strong wrapper for frame counts and frame indices.
	struct FrameCount {
		std::uint64_t value{0};													///< Wrapped frame count.
	};

	/// @brief Human-readable application name selected by the user program.
	struct ApplicationName {
		std::string value{"simple"};												///< Name shown in diagnostics and default window titles.
	};

	/// @brief Optional frame cap; zero lets the engine run until a close request.
	struct MaxFrames {
		FrameCount value{};														///< Maximum number of step() calls.
	};

	/// @brief Per-frame timing context passed to user systems.
	struct FrameContext {
		FrameCount frame_index{};												///< Zero-based frame index.
		DeltaTime delta_time{};													///< Time elapsed since the previous frame.
	};

	/// @brief Compact engine configuration kept for simple setup paths.
	struct EngineConfig {
		std::string application_name{"simple"};									///< Human-readable application name.
		FrameCount max_frames{};												///< Maximum frame count; zero means uncapped.
	};

	/// @brief Result of one engine frame.
	enum class FrameStatus {
		running,																		///< Engine can continue stepping.
		stopped,																		///< Engine stopped because a close request or frame cap was reached.
		continue_running = running,											///< Compatibility spelling for running.
		should_close	= stopped												///< Compatibility spelling for stopped.
	};

	/// @brief Strong wrapper for source vertex counts.
	struct VertexCount {
		std::uint64_t value{0};													///< Wrapped vertex count.
	};

	/// @brief Strong wrapper for source index counts.
	struct IndexCount {
		std::uint64_t value{0};													///< Wrapped index count.
	};

	/// @brief Strong wrapper for texture channel counts.
	struct TextureChannelCount {
		std::uint32_t value{0};													///< Wrapped channel count.
	};

	/// @brief Standard transform component shared by all active engine layers.
	struct Transform {
		Position translation{};													///< Local or world-space translation.
		Rotation rotation{};														///< Local or world-space orientation.
		Scale scale{};																///< Local or world-space non-uniform scale.
	};

	/// @brief Facade descriptor for plane scene-object setup.
	struct PlaneDescriptor {
		Vec2 half_extent{};														///< Half-size along the local plane axes.
		LinearColor color{};													///< Linear RGB surface color.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Facade descriptor for cuboid scene-object setup.
	struct CuboidDescriptor {
		Vec3 minimum{};															///< Minimum local-space corner.
		Vec3 maximum{};															///< Maximum local-space corner.
		LinearColor color{};													///< Linear RGB surface color.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Facade descriptor for textured cuboid scene-object setup.
	struct TexturedCuboidDescriptor {
		Vec3 minimum{};															///< Minimum local-space corner.
		Vec3 maximum{};															///< Maximum local-space corner.
		std::filesystem::path base_color_texture{};					///< Base-color texture source path.
		Transform transform{};													///< Local or world-space placement.
	};

	/// @brief Chainable facade builder for plane scene-object setup.
	class PlaneDescriptorBuilder {
	public:
		inline PlaneDescriptorBuilder() = default;

		[[nodiscard]] inline PlaneDescriptorBuilder &halfExtent(Vec2 value) {
			half_extent_ = value;
			return *this;
		}
		[[nodiscard]] inline PlaneDescriptorBuilder &color(LinearColor value) {
			color_ = value;
			return *this;
		}
		[[nodiscard]] inline PlaneDescriptorBuilder &transform(Transform value) {
			transform_ = value;
			return *this;
		}
		[[nodiscard]] inline PlaneDescriptor build() const {
			return PlaneDescriptor{.half_extent = half_extent_, .color = color_, .transform = transform_};
		}

	private:
		Vec2 half_extent_{};													///< Half-size along the local plane axes.
		LinearColor color_{};													///< Linear RGB surface color.
		Transform transform_{};												///< Local or world-space placement.
	};	///< Public plane descriptor builder using facade-only scene-object types.

	/// @brief Chainable facade builder for cuboid scene-object setup.
	class CuboidDescriptorBuilder {
	public:
		inline CuboidDescriptorBuilder() = default;

		[[nodiscard]] inline CuboidDescriptorBuilder &minimum(Vec3 value) {
			minimum_ = value;
			return *this;
		}
		[[nodiscard]] inline CuboidDescriptorBuilder &maximum(Vec3 value) {
			maximum_ = value;
			return *this;
		}
		[[nodiscard]] inline CuboidDescriptorBuilder &color(LinearColor value) {
			color_ = value;
			return *this;
		}
		[[nodiscard]] inline CuboidDescriptorBuilder &transform(Transform value) {
			transform_ = value;
			return *this;
		}
		[[nodiscard]] inline CuboidDescriptor build() const {
			return CuboidDescriptor{.minimum = minimum_, .maximum = maximum_, .color = color_, .transform = transform_};
		}

	private:
		Vec3 minimum_{};															///< Minimum local-space corner.
		Vec3 maximum_{};															///< Maximum local-space corner.
		LinearColor color_{};													///< Linear RGB surface color.
		Transform transform_{};												///< Local or world-space placement.
	};	///< Public cuboid descriptor builder using facade-only scene-object types.

	/// @brief Chainable facade builder for textured cuboid scene-object setup.
	class TexturedCuboidDescriptorBuilder {
	public:
		inline TexturedCuboidDescriptorBuilder() = default;

		[[nodiscard]] inline TexturedCuboidDescriptorBuilder &minimum(Vec3 value) {
			minimum_ = value;
			return *this;
		}
		[[nodiscard]] inline TexturedCuboidDescriptorBuilder &maximum(Vec3 value) {
			maximum_ = value;
			return *this;
		}
		[[nodiscard]] inline TexturedCuboidDescriptorBuilder &baseColorTexture(std::filesystem::path value) {
			base_color_texture_ = std::move(value);
			return *this;
		}
		[[nodiscard]] inline TexturedCuboidDescriptorBuilder &transform(Transform value) {
			transform_ = value;
			return *this;
		}
		[[nodiscard]] inline TexturedCuboidDescriptor build() const {
			return TexturedCuboidDescriptor{.minimum = minimum_, .maximum = maximum_,
													  .base_color_texture = base_color_texture_, .transform = transform_};
		}

	private:
		Vec3 minimum_{};															///< Minimum local-space corner.
		Vec3 maximum_{};															///< Maximum local-space corner.
		std::filesystem::path base_color_texture_{};				///< Base-color texture source path.
		Transform transform_{};												///< Local or world-space placement.
	};	///< Public textured cuboid descriptor builder using facade-only scene-object types.

	/// @brief Public scene loading options that keep default loadScene behavior.
	struct SceneLoadOptions {
		Scalar scale{one()};													///< Import scale factor.
		bool convert_coordinate_system{false};						///< Enables Y-up or handedness conversion.
		std::uint32_t max_texture_size{0};							///< Maximum texture dimension; zero means unlimited.
		bool load_cameras{false};											///< Imports cameras from the scene file.
		bool load_lights{false};											///< Imports lights from the scene file.
		bool use_cache{true};												///< Enables cached loader results.
	};

	/// @brief Public scene instantiation options for imported scene visibility.
	struct SceneInstantiationOptions {
		bool instantiate_geometry{true};									///< Creates render objects from imported geometry.
		bool apply_cameras{false};											///< Applies imported cameras to the render scene.
		bool apply_lights{false};											///< Applies imported lights to the render scene.
	};

	/// @brief Chainable facade builder for scene loading options.
	class SceneLoadOptionsBuilder {
	public:
		inline SceneLoadOptionsBuilder() = default;

		[[nodiscard]] inline SceneLoadOptionsBuilder &scale(Scalar value) {
			scale_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptionsBuilder &convertCoordinateSystem(bool value) {
			convert_coordinate_system_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptionsBuilder &maxTextureSize(std::uint32_t value) {
			max_texture_size_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptionsBuilder &loadCameras(bool value) {
			load_cameras_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptionsBuilder &loadLights(bool value) {
			load_lights_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptionsBuilder &useCache(bool value) {
			use_cache_ = value;
			return *this;
		}
		[[nodiscard]] inline SceneLoadOptions build() const {
			return SceneLoadOptions{.scale = scale_, .convert_coordinate_system = convert_coordinate_system_,
											.max_texture_size = max_texture_size_, .load_cameras = load_cameras_,
											.load_lights = load_lights_, .use_cache = use_cache_};
		}

	private:
		Scalar scale_{one()};												///< Import scale factor.
		bool convert_coordinate_system_{false};					///< Enables Y-up or handedness conversion.
		std::uint32_t max_texture_size_{0};						///< Maximum texture dimension; zero means unlimited.
		bool load_cameras_{false};										///< Imports cameras from the scene file.
		bool load_lights_{false};										///< Imports lights from the scene file.
		bool use_cache_{true};											///< Enables cached loader results.
	};	///< Public scene loading options builder using facade-only scene-load types.

	/// @brief Public renderer configuration kept independent from concrete renderer details.
	struct RendererConfig {
		RendererId renderer{};											///< Renderer kind selected by facade id.
		bool enable_shadows{true};										///< Enables renderer shadow support.
		bool enable_debug_output{false};								///< Enables renderer diagnostics.
	};

	/// @brief Chainable facade builder for renderer configuration.
	class RendererConfigBuilder {
	public:
		inline RendererConfigBuilder() = default;

		[[nodiscard]] inline RendererConfigBuilder &renderer(RendererId value) {
			renderer_ = value;
			return *this;
		}
		[[nodiscard]] inline RendererConfigBuilder &enableShadows(bool value) {
			enable_shadows_ = value;
			return *this;
		}
		[[nodiscard]] inline RendererConfigBuilder &enableDebugOutput(bool value) {
			enable_debug_output_ = value;
			return *this;
		}
		[[nodiscard]] inline RendererConfig build() const {
			return RendererConfig{.renderer = renderer_, .enable_shadows = enable_shadows_,
										 .enable_debug_output = enable_debug_output_};
		}

	private:
		RendererId renderer_{};											///< Renderer kind selected by facade id.
		bool enable_shadows_{true};									///< Enables renderer shadow support.
		bool enable_debug_output_{false};							///< Enables renderer diagnostics.
	};	///< Public renderer configuration builder using facade-only renderer types.

	/// @brief Axis-aligned bounds described by minimum and maximum positions.
	struct Bounds {
		Position minimum{};														///< Minimum corner.
		Position maximum{};														///< Maximum corner.
		bool valid{false};														///< False until at least one point has been included.
	};

	/// @brief Public camera description used by game code and renderers.
	struct Camera {
		Position position{.value = Vec3(zero(), static_cast<Scalar>(1.5), static_cast<Scalar>(6.0))};
		Direction forward{.value = Vec3(zero(), zero(), -one())};	///< View direction.
		Mat4 view_transform{math::translate(identityMat4(),
														Vec3(zero(), static_cast<Scalar>(-1.5), static_cast<Scalar>(-6.0)))};
		FovY fov_y{};																///< Vertical field of view.
		ClipPlanes clip{};														///< Near/far clip planes.

		[[nodiscard]] static inline Camera lookAt(Position position, Position target,
																Direction up = Direction{.value = Vec3(zero(), one(), zero())},
																FovY fov_y = {}, ClipPlanes clip = {}) {
			Camera camera{};
			camera.position = position;
			camera.forward = Direction{.value = math::subtract(target.value, position.value)};
			camera.view_transform = math::lookAt(position.value, target.value, up.value);
			camera.fov_y = fov_y;
			camera.clip = clip;
			return camera;
		}
	};

	/// @brief Chainable facade builder for clear camera setup.
	class CameraBuilder {
	public:
		inline CameraBuilder() = default;

		[[nodiscard]] inline CameraBuilder &position(Position value) {
			position_ = value;
			return *this;
		}
		[[nodiscard]] inline CameraBuilder &lookAt(Position target) {
			target_ = target;
			direction_.reset();
			return *this;
		}
		[[nodiscard]] inline CameraBuilder &direction(Direction value) {
			direction_ = value;
			target_.reset();
			return *this;
		}
		[[nodiscard]] inline CameraBuilder &fieldOfView(FovY value) {
			fov_y_ = value;
			return *this;
		}
		[[nodiscard]] inline CameraBuilder &clipPlanes(ClipPlanes value) {
			clip_ = value;
			return *this;
		}
		[[nodiscard]] inline CameraBuilder &targetExtent(PixelExtent value) {
			target_extent_ = value;
			return *this;
		}
		[[nodiscard]] inline Camera build() const {
			if (target_) {
				return Camera::lookAt(position_, *target_, Direction{.value = Vec3(zero(), one(), zero())}, fov_y_, clip_);
			}
			const auto target = Position{.value = math::add(position_.value, direction_.value().value)};
			return Camera::lookAt(position_, target, Direction{.value = Vec3(zero(), one(), zero())}, fov_y_, clip_);
		}

	private:
		Position position_{Camera{}.position};							///< Camera eye position.
		std::optional<Position> target_{};								///< Optional world-space look-at point.
		std::optional<Direction> direction_{Direction{}};			///< Optional forward direction used as target offset.
		FovY fov_y_{};															///< Vertical field of view.
		ClipPlanes clip_{};													///< Near/far clip planes.
		PixelExtent target_extent_{};										///< Intended render extent for future aspect setup.
	};	///< Public camera builder using facade-only camera types.

} // namespace vve
