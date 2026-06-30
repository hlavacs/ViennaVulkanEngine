# Builder API Discovery

Discovery-only notes for adding builder-pattern APIs at the public `vve` facade level.

## Project Instructions Read

- `AGENTS.md`

Relevant rules for this task:

- The user-facing common interface is in `src` and lives in namespace `vve`.
- User programs must call only the facade layer, never concrete engine details.
- Concrete engines live under `src/versions/*` and must stay isolated.
- Facade wrappers hide implementation details behind the facade/implementation split.
- Prefer strong semantic types over ambiguous primitives where practical.
- Keep the API educational, small, documented, and avoid unnecessary abstractions.
- For this discovery task, only `docs/builder_api_discovery.md` was created; no source, header, or build files were changed.

## Facade/Implementation Boundary

The public facade surface relevant to this job is declared in:

- `src/Engine.ixx`
- `src/Window.ixx`
- `src/World.ixx`
- `src/Types.ixx`
- `src/RenderSystem.ixx`
- `src/Assets.ixx`

The public wrappers hold erased implementation pointers such as `void *impl_{}` and the implementation-specific recovery is kept in `.cpp` files. This keeps selected-engine details out of user code; future builder APIs should remain in namespace `vve` and use only facade-defined types.

## Engine Factory

Path: `src/Engine.ixx:222`

Current `vve::makeEngine` is an inline callable facade object, with the user-facing call signature provided by `MakeEngine::operator()`.

```cpp
struct MakeEngine {
	template <typename... TOptions> [[nodiscard]] auto operator()(TOptions &&...options) const;
};														///< Callable facade engine factory.

template <typename... TOptions> auto MakeEngine::operator()(TOptions &&...options) const {
	using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
	using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;
	return TEngine(std::forward<TOptions>(options)...);
}

inline constexpr MakeEngine makeEngine{};	///< Facade engine factory.
```

Related engine option handling:

- `src/Engine.ixx:92`: `explicit Engine(EngineConfig config);`
- `src/Engine.ixx:94`: `template <typename... TOptions>`
- `src/Engine.ixx:96`: `explicit Engine(TOptions &&...options);`
- `src/Engine.ixx:284`: `EngineConfig`
- `src/Engine.ixx:286`: `ApplicationName`
- `src/Engine.ixx:288`: `MaxFrames`
- `src/Engine.ixx:297`: `WindowSetups`
- `src/Engine.ixx:314`: `const UserSystems<TUserSystems...> &systems`
- `src/Engine.ixx:321`: `UserSystems<TUserSystems...> &systems`

## User Systems Bundle

Path: `src/World.ixx:69`

```cpp
template <typename... TSystems> struct UserSystems {
	std::tuple<TSystems...> value{};		///< User systems stored by value.
};																		///< User systems bundle.

struct MakeUserSystems {
	template <typename... TSystems> [[nodiscard]] auto operator()(TSystems &&...systems) const {
		return UserSystems<std::remove_cvref_t<TSystems>...>{
			.value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
	}
};																		///< Callable facade user-system bundle factory.

inline constexpr MakeUserSystems makeUserSystems{};	///< Facade user-system bundle factory.
```

## WindowSetup Chaining Model

Path: `src/Window.ixx:14`

Full current `WindowSetup` definition:

```cpp
class WindowSetup {
public:
	inline WindowSetup() = default;

	[[nodiscard]] inline WindowSetup &id(std::string value) {
		id_ = std::move(value);
		return *this;
	}
	[[nodiscard]] inline WindowSetup &title(std::string value) {
		title_ = std::move(value);
		return *this;
	}
	[[nodiscard]] inline WindowSetup &extent(PixelExtent value) {
		extent_ = value;
		return *this;
	}
	[[nodiscard]] inline WindowSetup &position(int x, int y) {
		x_ = x;
		y_ = y;
		return *this;
	}
	[[nodiscard]] inline WindowSetup &renderer(RendererId value) {
		renderer_id_ = std::move(value);
		return *this;
	}
	[[nodiscard]] inline WindowSetup &resizable(bool value) {
		resizable_ = value;
		return *this;
	}
	[[nodiscard]] inline WindowSetup &visible(bool value) {
		visible_ = value;
		return *this;
	}

private:
	template <typename... TSystems> friend class Engine;

	std::string id_{"main"};													///< Stable application-local window id.
	std::string title_{"VVE simple"};										///< Platform window title.
	PixelExtent extent_{.width = 960, .height = 540};				///< Initial pixel dimensions.
	std::optional<int> x_{};													///< Optional initial screen x coordinate.
	std::optional<int> y_{};													///< Optional initial screen y coordinate.
	RendererId renderer_id_{};												///< Renderer id selected for this window.
	bool resizable_{true};													///< Enables platform resizing.
	bool visible_{true};														///< Shows the window after creation.
};	///< Facade startup window option.
```

Chaining style: each mutator stores one option in the private facade descriptor, returns `WindowSetup &`, and therefore supports calls such as `WindowSetup{}.title(...).extent(...).visible(...)`. `Engine` is a friend and converts the private facade fields into opaque startup options.

Window collection option:

Path: `src/Window.ixx:61`

```cpp
class WindowSetups {
public:
	inline WindowSetups() = default;
	inline WindowSetups(std::initializer_list<WindowSetup> windows) {
		value_.clear();
		value_.reserve(windows.size());
		for (const auto &window : windows) { value_.push_back(window); }
	}

	inline void add(WindowSetup window) { value_.push_back(std::move(window)); }

private:
	template <typename... TSystems> friend class Engine;

	std::vector<WindowSetup> value_{WindowSetup{}};	///< Startup windows; defaults to one main window.
};	///< Facade startup window collection option.
```

Renderer selection currently appears here through `WindowSetup::renderer(RendererId value)` and the stored `RendererId renderer_id_{}` field.

## Camera Configuration

Path: `src/Types.ixx:172`

Current camera descriptor and helper:

```cpp
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
```

Render-system camera submission:

Path: `src/RenderSystem.ixx:80`

```cpp
auto setCamera(Camera camera, PixelExtent extent)																		-> void;
```

Camera-related strong types:

- `src/Types.ixx:36`: `struct Position`
- `src/Types.ixx:41`: `struct Direction`
- `src/Types.ixx:76`: `struct FovY`
- `src/Types.ixx:81`: `struct ClipPlanes`
- `src/Types.ixx:92`: `struct PixelExtent`

## Light Setup Calls

Path: `src/RenderSystem.ixx:81`

Current facade declarations:

```cpp
void setDirectionalLight(Direction direction_to_light, LinearColor color,
									LightIntensity intensity, LinearColor ambient);
auto setPointLight(Position position, LinearColor color, LightIntensity intensity, LightRange range)	-> void;
auto setPointLight(Position position, LinearColor color, LightIntensity intensity,
						 LightRange range, LinearColor ambient)															-> void;
void setSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone);
void setSpotLight(Position position, Direction direction, LinearColor color,
						LightIntensity intensity, LightRange range, SpotConeAngle cone, LinearColor ambient);
```

Light-related strong types:

- `src/Types.ixx:56`: `struct LinearColor`
- `src/Types.ixx:61`: `struct LightIntensity`
- `src/Types.ixx:66`: `struct LightRange`
- `src/Types.ixx:71`: `struct SpotConeAngle`

## Simple Scene Object Creation Calls

Path: `src/RenderSystem.ixx:90`

Current facade declarations:

```cpp
[[nodiscard]] std::expected<void, Error> addPlane(Vec2 half_extent, LinearColor color,
																	Transform transform = {});
[[nodiscard]] std::expected<void, Error> addCuboid(Vec3 minimum, Vec3 maximum, LinearColor color,
																	Transform transform = {});
[[nodiscard]] std::expected<void, Error> addTexturedCuboid(Vec3 minimum, Vec3 maximum,
																			 std::filesystem::path base_color_texture,
																			 Transform transform = {});
```

Transform descriptor:

Path: `src/Types.ixx:158`

```cpp
/// @brief Standard transform component shared by all active engine layers.
struct Transform {
	Position translation{};													///< Local or world-space translation.
	Rotation rotation{};														///< Local or world-space orientation.
	Scale scale{};																///< Local or world-space non-uniform scale.
};
```

## Scene Loading

Path: `src/Assets.ixx:22`

Current facade scene-loading declaration:

```cpp
[[nodiscard]] auto loadScene(const std::filesystem::path &source)		-> std::expected<SceneHandle, Error>;
```

Path: `src/Assets.cpp:31`

Current facade forwarding definition:

```cpp
auto AssetSystem::loadScene(const std::filesystem::path &source) -> std::expected<SceneHandle, Error> {
	return assetSystemImpl(impl_).loadScene(source);
}
```

There is no public facade `RenderSystem::loadScene(path)` declaration in `src/RenderSystem.ixx`; scene file loading currently belongs to `vve::AssetSystem` and returns a facade `SceneHandle`.

## Renderer Configuration Surface

No standalone public renderer configuration descriptor or builder was found in the facade headers read for this task. Current user-facing renderer selection is limited to `RendererId` and startup window configuration:

Path: `src/Types.ixx:103`

```cpp
/// @brief Strong wrapper for renderer selection identifiers.
struct RendererId {
	std::string value{};														///< Wrapped renderer identifier.
};
```

Path: `src/Window.ixx:35`

```cpp
[[nodiscard]] inline WindowSetup &renderer(RendererId value) {
	renderer_id_ = std::move(value);
	return *this;
}
```
