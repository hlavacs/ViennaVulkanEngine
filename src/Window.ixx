export module VVEngine:Window;
import std;
import :Implementation;
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
	};	///< SDL-free facade key names used by application input queries.

	class WindowSystem;

	class InputState {
	public:
		InputState(const InputState &) = default;
		InputState(InputState &&) noexcept = default;
		InputState &operator=(const InputState &) = delete;
		InputState &operator=(InputState &&) noexcept = delete;

		void beginFrame();
		void holdKey(std::int32_t keycode);
		void pressKey(std::int32_t keycode);
		void releaseKey(std::int32_t keycode);
		void setMousePosition(WindowHandle window, Vec2 position);
		void addMouseDelta(WindowHandle window, Vec2 delta);
		void addMouseWheelDelta(WindowHandle window, Vec2 delta);

		[[nodiscard]] bool isKeyDown(std::int32_t keycode) const;
		[[nodiscard]] bool isKeyDown(Key key) const;
		[[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const;
		[[nodiscard]] bool wasKeyPressed(Key key) const;
		[[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const;
		[[nodiscard]] bool wasKeyReleased(Key key) const;
		[[nodiscard]] auto mousePosition(WindowHandle window) const -> std::optional<Vec2>; ///< Normalised window coordinates: (0,0) top left, (1,1) bottom right, y down.
		///< Values can leave 0..1 while a drag continues outside the window.
		[[nodiscard]] Vec2 mouseDelta(WindowHandle window) const; ///< Uses the same normalised units; mouse-look code must scale x by the aspect ratio for equal angles per distance.
		[[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const; ///< Wheel movement in scroll ticks.

	private:
		friend class WindowSystem;

		using Impl = detail::InputStateImpl;	///< Wrapped implementation class.
		explicit InputState(Impl &implementation) noexcept;

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Facade input snapshot.

	/// @brief Reusable keyboard-driven camera controller for application cameras.
	class DefaultCameraController {
	public:
		[[nodiscard]] auto update(const InputState &input, DeltaTime dt) -> Camera;

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

		[[nodiscard]] WindowHandle handle() const;
		[[nodiscard]] std::string_view id() const;
		[[nodiscard]] std::string_view title() const;
		[[nodiscard]] PixelExtent extent() const; ///< Drawable size in pixels.
		[[nodiscard]] RendererId rendererId() const;
		[[nodiscard]] bool focused() const;
		[[nodiscard]] bool minimized() const;
		[[nodiscard]] bool shouldClose() const;

	private:
		friend class WindowSystem;

		using Impl = detail::WindowImpl;	///< Wrapped implementation class.
		explicit Window(const Impl &implementation) noexcept;

		const Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Read-only facade window view.

	class WindowSystem {
	public:
		WindowSystem(const WindowSystem &) = default;
		WindowSystem(WindowSystem &&) noexcept = default;
		WindowSystem &operator=(const WindowSystem &) = delete;
		WindowSystem &operator=(WindowSystem &&) noexcept = delete;

		[[nodiscard]] std::string_view name() const noexcept;
		[[nodiscard]] InputState input();
		[[nodiscard]] InputState input() const;
		[[nodiscard]] std::size_t windowCount() const;
		[[nodiscard]] auto windows() const														-> Vector<Window>;
		[[nodiscard]] auto findWindow(std::string_view id) const							-> std::optional<Window>;
		[[nodiscard]] auto findWindow(WindowHandle handle) const							-> std::optional<Window>;

	private:
		template <typename... TSystems> friend class Engine;

		using Impl = detail::WindowSystemImpl;	///< Wrapped implementation class.
		explicit WindowSystem(Impl &implementation) noexcept;

		Impl &impl_;	///< Non-owning reference to the wrapped implementation.
	};	///< Public window-system wrapper.

} // namespace vve
