export module VEEngine.Types;
import std;
export import VEEngine.Error;
export import VEEngine.Handle;
export import VEEngine.Math;
export import VEEngine.Vector;

/**
	* @file
	* @brief Public data and descriptor contract declared by the facade layer.
	*/
export namespace vve {

	struct EntityTag {};													///< Facade ECS entity handle tag.
	struct SceneHandleTag {};											///< Scene descriptor handle tag.
	struct WindowHandleTag {};											///< Runtime window handle tag.
	struct NodeHandleTag {};											///< Node descriptor handle tag.
	struct MeshHandleTag {};											///< Mesh descriptor handle tag.
	struct MaterialHandleTag {};										///< Material descriptor handle tag.
	struct TextureHandleTag {};										///< Texture descriptor handle tag.
	struct LightHandleTag {};											///< Light descriptor handle tag.
	struct CameraHandleTag {};											///< Camera descriptor handle tag.

	using Entity	= TypedHandle<EntityTag>;				///< Facade ECS entity.
	using SceneHandle	= TypedHandle<SceneHandleTag>;		///< Scene descriptor handle.
	using WindowHandle	= TypedHandle<WindowHandleTag>;		///< Runtime window handle.
	using NodeHandle	= TypedHandle<NodeHandleTag>;			///< Node descriptor handle.
	using MeshHandle	= TypedHandle<MeshHandleTag>;			///< Mesh descriptor handle.
	using MaterialHandle = TypedHandle<MaterialHandleTag>;	///< Material descriptor handle.
	using TextureHandle	= TypedHandle<TextureHandleTag>;		///< Texture descriptor handle.
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

	/// @brief Strong wrapper for vertical field-of-view angles.
	struct FovY {
		Scalar radians{static_cast<Scalar>(1.0471975511965976)};		///< Wrapped vertical FOV in radians.
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
		std::string value{"v5"};												///< Name shown in diagnostics and default window titles.
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
		std::string application_name{"v5"};									///< Human-readable application name.
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
