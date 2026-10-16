export module VVEngine:Window;
import std;
import :Implementation;
import VVEngine.Error;
import VVEngine.Types;
import VVEngine.Vector;

/**
	* @file
	* @brief Public window/input contract backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

	/// @brief Fluent startup option owning one shared window descriptor.
	class WindowSetup {
	public:
		inline WindowSetup() = default;

		[[nodiscard]] inline WindowSetup &id(std::string value) {
			value_.id = std::move(value);
			return *this;
		}
		[[nodiscard]] inline WindowSetup &title(std::string value) {
			value_.title = std::move(value);
			return *this;
		}
		/// @brief Sets the initial size in window coordinates.
		[[nodiscard]] inline WindowSetup &extent(PixelExtent value) {
			value_.extent = value;
			return *this;
		}
		[[nodiscard]] inline WindowSetup &position(int x, int y) {
			value_.x = x;
			value_.y = y;
			return *this;
		}
		/// @brief Selects forward rendering (empty or "forward"), or "none" to opt out; other ids make init return invalid_argument.
		[[nodiscard]] inline WindowSetup &renderer(RendererId value) {
			value_.renderer_id = std::move(value);
			return *this;
		}
		[[nodiscard]] inline WindowSetup &resizable(bool value) {
			value_.resizable = value;
			return *this;
		}
		[[nodiscard]] inline WindowSetup &visible(bool value) {
			value_.visible = value;
			return *this;
		}
		/// @brief Starts the window in fullscreen at the desktop resolution.
		[[nodiscard]] inline WindowSetup &fullscreen(bool value) {
			value_.fullscreen = value;
			return *this;
		}

	private:
		template <typename... TSystems> friend class Engine;

		WindowDesc value_{};	///< Shared startup descriptor passed to the engine.
	};	///< Facade startup window option.

	class WindowSetups {
	public:
		inline WindowSetups() = default;
		inline WindowSetups(std::initializer_list<WindowSetup> windows) {
			value_.reserve(windows.size());
			for (const auto &window : windows) { value_.push_back(window); }
		}

		inline void add(WindowSetup window) { value_.push_back(std::move(window)); }

	private:
		template <typename... TSystems> friend class Engine;

		std::vector<WindowSetup> value_{};	///< Explicit startup windows; the engine supplies a main window when empty.
	};	///< Facade startup window collection option.

	enum class Key : std::int32_t {
		escape = 27,				///< Escape key SDL keycode.
		o = 111,						///< O key SDL keycode.
		p = 112,						///< P key SDL keycode.
		l = 108,						///< L key SDL keycode.
		q = 113,						///< Q key SDL keycode.
		e = 101,						///< E key SDL keycode.
		w = 119,						///< W key SDL keycode.
		a = 97,						///< A key SDL keycode.
		s = 115,						///< S key SDL keycode.
		d = 100,						///< D key SDL keycode.
		left = 1073741904,		///< Left arrow SDL keycode.
		right = 1073741903,		///< Right arrow SDL keycode.
		up = 1073741906,			///< Up arrow SDL keycode.
		down = 1073741905,		///< Down arrow SDL keycode.
		left_shift = 1073742049,	///< Left Shift SDL keycode.
		right_shift = 1073742053,	///< Right Shift SDL keycode.
		space = 32,					///< Space bar SDL keycode.
		enter = 13,					///< Return key SDL keycode.
		tab = 9,						///< Tab key SDL keycode.
		backspace = 8,				///< Backspace SDL keycode.
		delete_key = 127,			///< Delete SDL keycode.
		left_ctrl = 1073742048,	///< Left Control SDL keycode.
		right_ctrl = 1073742052,	///< Right Control SDL keycode.
		left_alt = 1073742050,	///< Left Alt SDL keycode.
		right_alt = 1073742054,	///< Right Alt SDL keycode.
		f1 = 1073741882,			///< F1 SDL keycode.
		f2 = 1073741883,			///< F2 SDL keycode.
		f3 = 1073741884,			///< F3 SDL keycode.
		f4 = 1073741885,			///< F4 SDL keycode.
		f5 = 1073741886,			///< F5 SDL keycode.
		f6 = 1073741887,			///< F6 SDL keycode.
		f7 = 1073741888,			///< F7 SDL keycode.
		f8 = 1073741889,			///< F8 SDL keycode.
		f9 = 1073741890,			///< F9 SDL keycode.
		f10 = 1073741891,			///< F10 SDL keycode.
		f11 = 1073741892,			///< F11 SDL keycode.
		f12 = 1073741893,			///< F12 SDL keycode.
		insert = 1073741897,		///< Insert SDL keycode.
		home = 1073741898,			///< Home SDL keycode.
		page_up = 1073741899,		///< Page Up SDL keycode.
		end = 1073741901,			///< End SDL keycode.
		page_down = 1073741902,	///< Page Down SDL keycode.
		mouse_left = -1,			///< Left mouse button (negative codes are reserved for buttons).
		mouse_middle = -2,			///< Middle mouse button.
		mouse_right = -3,			///< Right mouse button.
	};	///< SDL-free facade key names used by application input queries.

	/// @brief Names the key of a letter (either case), digit or ASCII punctuation character, e.g. key('w') or key('1').
	[[nodiscard]] constexpr Key key(char c) {
		return static_cast<Key>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
	}

	class WindowSystem;

	class InputState {
	public:
		InputState(const InputState &) = default;
		InputState(InputState &&) noexcept = default;
		InputState &operator=(const InputState &) = delete;
		InputState &operator=(InputState &&) noexcept = delete;

		/// @brief Starts a new input frame in the selected implementation.
		inline void beginFrame() { impl_.beginFrame(); }
		/// @brief Marks a key as held in the selected implementation.
		inline void holdKey(std::int32_t keycode) { impl_.holdKey(keycode); }
		/// @brief Marks a key press transition in the selected implementation.
		inline void pressKey(std::int32_t keycode) { impl_.pressKey(keycode); }
		/// @brief Marks a key release transition in the selected implementation.
		inline void releaseKey(std::int32_t keycode) { impl_.releaseKey(keycode); }
		/// @brief Stores a per-window mouse position in the selected implementation.
		inline void setMousePosition(WindowHandle window, Vec2 position) {
			impl_.setMousePosition(window, position);
		}
		/// @brief Accumulates a per-window mouse movement delta in the selected implementation.
		inline void addMouseDelta(WindowHandle window, Vec2 delta) { impl_.addMouseDelta(window, delta); }
		/// @brief Accumulates a per-window mouse wheel delta in the selected implementation.
		inline void addMouseWheelDelta(WindowHandle window, Vec2 delta) {
			impl_.addMouseWheelDelta(window, delta);
		}

		/// @brief Reports whether a raw keycode is currently down.
		[[nodiscard]] inline bool isKeyDown(std::int32_t keycode) const { return impl_.isKeyDown(keycode); }
		/// @brief Reports whether a facade key is currently down.
		[[nodiscard]] inline bool isKeyDown(Key key) const { return isKeyDown(static_cast<std::int32_t>(key)); }
		/// @brief Reports whether a raw keycode was pressed during the current frame.
		[[nodiscard]] inline bool wasKeyPressed(std::int32_t keycode) const { return impl_.wasKeyPressed(keycode); }
		/// @brief Reports whether a facade key was pressed during the current frame.
		[[nodiscard]] inline bool wasKeyPressed(Key key) const { return wasKeyPressed(static_cast<std::int32_t>(key)); }
		/// @brief Reports whether a raw keycode was released during the current frame.
		[[nodiscard]] inline bool wasKeyReleased(std::int32_t keycode) const { return impl_.wasKeyReleased(keycode); }
		/// @brief Reports whether a facade key was released during the current frame.
		[[nodiscard]] inline bool wasKeyReleased(Key key) const { return wasKeyReleased(static_cast<std::int32_t>(key)); }
		/// @brief Returns the last known mouse position for a window when one is available.
		[[nodiscard]] inline auto mousePosition(WindowHandle window) const -> std::optional<Vec2> {
			return impl_.mousePosition(window);
		} ///< Normalised window coordinates: (0,0) top left, (1,1) bottom right, y down.
		///< Values can leave 0..1 while a drag continues outside the window.
		/// @brief Returns the accumulated mouse movement delta for a window.
		[[nodiscard]] inline Vec2 mouseDelta(WindowHandle window) const { return impl_.mouseDelta(window); } ///< Uses the same normalised units; mouse-look code must scale x by the aspect ratio for equal angles per distance.
		/// @brief Returns the accumulated mouse wheel delta for a window.
		[[nodiscard]] inline Vec2 mouseWheelDelta(WindowHandle window) const { return impl_.mouseWheelDelta(window); } ///< Wheel movement in scroll ticks.

	private:
		friend class WindowSystem;

		using Impl = detail::InputStateImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit InputState(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Facade input snapshot.

	/// @brief Reusable keyboard-driven camera controller for application cameras.
	class DefaultCameraController {
	public:
		/**
			* @brief Applies the default keyboard camera motion and returns the resulting facade camera.
			* @param input Current facade input snapshot used for continuous movement and turning.
			* @param dt Elapsed seconds, clamped to [0, 0.1] to bound motion after a stall.
			* @return Camera looking from the updated eye position along the updated forward vector.
			*/
		[[nodiscard]] inline auto update(const InputState &input, DeltaTime dt) -> Camera {
			const Vec3 worldUp{zero(), one(), zero()};	///< Stable up axis for view and flight.
			const auto seconds = static_cast<Scalar>(std::clamp(dt.seconds, 0.0, 0.1));

			// Shift doubles both turning and movement for the current frame.
			const Scalar boost = input.isKeyDown(Key::left_shift) || input.isKeyDown(Key::right_shift) ? static_cast<Scalar>(2) : one();
			const Scalar turnStep = turn_speed * seconds * boost;
			const Scalar movementStep = move_speed * seconds * boost;

			// Update view angles before movement so the current frame moves in the new direction.
			if (input.isKeyDown(Key::left)) { yaw -= turnStep; }
			if (input.isKeyDown(Key::right)) { yaw += turnStep; }
			if (input.isKeyDown(Key::up)) { pitch -= turnStep; }
			if (input.isKeyDown(Key::down)) { pitch += turnStep; }
			pitch = math::clamp(pitch, -max_pitch, max_pitch);

			// Rebuild camera basis after clamping to preserve the original example feel.
			const auto forward = math::normalize(Vec3{std::cos(pitch) * std::sin(yaw), std::sin(pitch),
											 -std::cos(pitch) * std::cos(yaw)});
			const Vec3 right = math::normalize(math::cross(forward, worldUp));
			if (input.isKeyDown(Key::w)) { eye.value = math::add(eye.value, math::scale(forward, movementStep)); }
			if (input.isKeyDown(Key::s)) { eye.value = math::subtract(eye.value, math::scale(forward, movementStep)); }
			if (input.isKeyDown(Key::a)) { eye.value = math::subtract(eye.value, math::scale(right, movementStep)); }
			if (input.isKeyDown(Key::d)) { eye.value = math::add(eye.value, math::scale(right, movementStep)); }
			if (input.isKeyDown(Key::q)) { eye.value = math::subtract(eye.value, math::scale(worldUp, movementStep)); }
			if (input.isKeyDown(Key::e)) { eye.value = math::add(eye.value, math::scale(worldUp, movementStep)); }

			return Camera::lookAt(eye, Position{.value = math::add(eye.value, forward)}, Direction{.value = worldUp});
		}

		Position eye{.value = Vec3{0.0F, 6.0F, 9.0F}};	///< Camera eye position.
		Scalar yaw{};													///< Horizontal look angle around the up axis.
		Scalar pitch{};												///< Vertical look angle from the ground plane.
		Scalar move_speed{static_cast<Scalar>(4.8)};			///< Movement speed in world units per second.
		Scalar turn_speed{static_cast<Scalar>(1.5)};		///< Turning speed in radians per second.
		Scalar max_pitch{static_cast<Scalar>(1.45)};			///< Absolute pitch clamp before the view singularity.
	};

	class Window {
	public:
		Window(const Window &) = default;
		Window(Window &&) noexcept = default;
		Window &operator=(const Window &) = delete;
		Window &operator=(Window &&) noexcept = delete;

		/// @brief Returns the stable runtime handle of the selected implementation window.
		[[nodiscard]] inline WindowHandle handle() const { return impl_.info().handle; }
		/// @brief Returns the application-local id of the selected implementation window.
		[[nodiscard]] inline std::string_view id() const { return impl_.info().id; }
		/// @brief Returns the platform title of the selected implementation window.
		[[nodiscard]] inline std::string_view title() const { return impl_.info().title; }
		/// @brief Returns the current pixel extent of the selected implementation window.
		[[nodiscard]] inline PixelExtent extent() const { return impl_.info().extent; } ///< Drawable size in pixels.
		/// @brief Returns the renderer id associated with the selected implementation window.
		[[nodiscard]] inline RendererId rendererId() const { return impl_.info().renderer_id; }
		/// @brief Reports whether the selected implementation window currently has focus.
		[[nodiscard]] inline bool focused() const { return impl_.info().focused; }
		/// @brief Reports whether the selected implementation window is minimized.
		[[nodiscard]] inline bool minimized() const { return impl_.info().minimized; }
		/// @brief Reports whether the selected implementation window received a close request.
		[[nodiscard]] inline bool shouldClose() const { return impl_.info().should_close; }

	private:
		friend class WindowSystem;

		using Impl = detail::WindowImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit Window(const Impl &implementation) noexcept : impl_{implementation} {}

		const Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Read-only facade window view.

	class WindowSystem {
	public:
		WindowSystem(const WindowSystem &) = default;
		WindowSystem(WindowSystem &&) noexcept = default;
		WindowSystem &operator=(const WindowSystem &) = delete;
		WindowSystem &operator=(WindowSystem &&) noexcept = delete;

		/// @brief Returns the diagnostic name of the selected window system implementation.
		[[nodiscard]] inline std::string_view name() const noexcept { return impl_.name(); }
		/// @brief Returns a facade input view for the window system's owned input state.
		[[nodiscard]] inline InputState input() { return InputState{impl_.input()}; }
		/// @brief Returns a facade input view for the window system's owned input state.
		[[nodiscard]] inline InputState input() const { return InputState{impl_.input()}; }
		/// @brief Returns the number of windows owned by the selected implementation.
		[[nodiscard]] inline std::size_t windowCount() const { return impl_.windowCount(); }
		/// @brief Returns facade views over the windows currently owned by the selected implementation.
		[[nodiscard]] inline auto windows() const -> Vector<Window> {
			Vector<Window> result{};
			const auto implementation_windows = impl_.windows();
			result.reserve(implementation_windows.size());
			for (const auto window : implementation_windows) {
				result.push_back(Window{window.get()});
			}
			return result;
		}
		/// @brief Captures or releases the mouse for one window: hidden pointer and unbounded relative mouseDelta, for mouse look.
		[[nodiscard]] inline auto setMouseCaptured(WindowHandle window, bool captured) -> std::expected<void, Error> {
			return impl_.setMouseCaptured(window, captured);
		} ///< Unknown windows return invalid_handle; a platform refusal returns platform_error.
		/// @brief Switches one window between fullscreen at the desktop resolution and a normal window.
		[[nodiscard]] inline auto setFullscreen(WindowHandle window, bool fullscreen) -> std::expected<void, Error> {
			return impl_.setFullscreen(window, fullscreen);
		} ///< Unknown windows return invalid_handle; the renderer follows the new drawable size.
		/// @brief Finds a facade window view by application-local id.
		[[nodiscard]] inline auto findWindow(std::string_view id) const -> std::optional<Window> {
			auto *window = impl_.findWindow(id);
			return window == nullptr ? std::optional<Window>{}
											 : std::optional<Window>{Window{*window}};
		}
		/// @brief Finds a facade window view by runtime handle.
		[[nodiscard]] inline auto findWindow(WindowHandle handle) const -> std::optional<Window> {
			auto *window = impl_.findWindow(handle);
			return window == nullptr ? std::optional<Window>{}
											 : std::optional<Window>{Window{*window}};
		}

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::WindowSystemImpl;	///< Wrapped implementation class.
		/// @brief Binds the facade wrapper to the implementation object owned by the engine.
		inline explicit WindowSystem(Impl &implementation) noexcept : impl_{implementation} {}

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public window-system wrapper.

} // namespace vve
