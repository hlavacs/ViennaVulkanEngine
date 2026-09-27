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
		Direction direction{};													///< World-space light direction (light toward scene) when available.
		Position position{};													///< World-space light position when available.
		LightRange range{};														///< Finite influence range for point and spot lights.
		SpotConeAngle cone{};													///< Outer cone angle for spot lights.
		SpotConeAngle inner_cone{.radians = static_cast<Scalar>(0.35)};	///< Inner cone angle of full intensity for spot lights.
	};

	/// @brief Facade descriptor for directional light setup.
	struct DirectionalLight {
		Direction direction{};													///< Direction in which the light travels (from the light toward the scene).
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

	/// @brief Strong wrapper for vertical field-of-view angles.
	struct FovY {
		Scalar radians{static_cast<Scalar>(1.0471975511965976)};		///< Wrapped vertical FOV in radians.
	};

	/// @brief Facade descriptor for imported camera data stored by the asset system.
	struct CameraDescriptor {
		Position position{};													///< World-space camera position when available.
		Direction direction{};													///< World-space camera look direction.
		Direction up{.value = Vec3(zero(), one(), zero())};			///< World-space camera up direction.
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
	};

	/// @brief Strong wrapper for source vertex counts.
	struct VertexCount {
		std::uint64_t value{0};													///< Wrapped vertex count.
	};

	/// @brief Strong wrapper for source index counts.
	struct IndexCount {
		std::uint64_t value{0};													///< Wrapped index count.
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

	/// @brief Public scene instantiation options for imported scene visibility.
	struct SceneInstantiationOptions {
		bool instantiate_geometry{true};									///< Creates render objects from imported geometry.
		bool apply_cameras{false};											///< Applies imported cameras to the render scene.
		bool apply_lights{false};											///< Applies imported lights to the render scene.
	};

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

} // namespace vve
