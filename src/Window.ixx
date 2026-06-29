export module VEEngine:Window;
import std;
import VEEngine.Types;
import VEEngine.Vector;

/**
	* @file
	* @brief Public window/input contract backed by the selected engine implementation.
	*/
export namespace vve {

	template <typename... TSystems> class Engine;

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

	enum class Key : std::int32_t {
		escape = 27,				///< Escape key SDL keycode.
		o = 111,						///< O key SDL keycode.
		p = 112,						///< P key SDL keycode.
		l = 108,						///< L key SDL keycode.
		w = 119,						///< W key SDL keycode.
		a = 97,						///< A key SDL keycode.
		s = 115,						///< S key SDL keycode.
		d = 100,						///< D key SDL keycode.
		left = 1073741904,		///< Left arrow SDL keycode.
		right = 1073741903,		///< Right arrow SDL keycode.
		up = 1073741906,			///< Up arrow SDL keycode.
		down = 1073741905,		///< Down arrow SDL keycode.
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
		[[nodiscard]] auto mousePosition(WindowHandle window) const -> std::optional<Vec2>;
		[[nodiscard]] Vec2 mouseDelta(WindowHandle window) const;
		[[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const;

	private:
		friend class WindowSystem;

		explicit InputState(void *implementation) noexcept;

		void *impl_{};	///< Opaque non-owning implementation pointer.
	};	///< Facade input snapshot.

	class Window {
	public:
		Window(const Window &) = default;
		Window(Window &&) noexcept = default;
		Window &operator=(const Window &) = delete;
		Window &operator=(Window &&) noexcept = delete;

		[[nodiscard]] WindowHandle handle() const;
		[[nodiscard]] std::string_view id() const;
		[[nodiscard]] std::string_view title() const;
		[[nodiscard]] PixelExtent extent() const;
		[[nodiscard]] RendererId rendererId() const;
		[[nodiscard]] std::optional<Entity> camera() const;
		[[nodiscard]] bool focused() const;
		[[nodiscard]] bool minimized() const;
		[[nodiscard]] bool shouldClose() const;

	private:
		friend class WindowSystem;

		explicit Window(void *implementation) noexcept;

		void *impl_{};	///< Opaque non-owning implementation pointer.
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
		[[nodiscard]] auto setWindowCamera(WindowHandle window, Entity camera)			-> std::expected<void, Error>;
		[[nodiscard]] auto setWindowCamera(std::string_view id, Entity camera)			-> std::expected<void, Error>;
		[[nodiscard]] auto clearWindowCamera(WindowHandle window)							-> std::expected<void, Error>;
		[[nodiscard]] auto clearWindowCamera(std::string_view id)							-> std::expected<void, Error>;
		[[nodiscard]] auto windowCamera(WindowHandle window) const							-> std::optional<Entity>;
		[[nodiscard]] auto windowCamera(std::string_view id) const							-> std::optional<Entity>;
		[[nodiscard]] auto setActiveCamera(Entity camera)										-> std::expected<void, Error>;
		[[nodiscard]] std::optional<Entity> activeCamera() const;

	private:
		template <typename... TSystems> friend class Engine;

		explicit WindowSystem(void *implementation) noexcept;

		void *impl_{};	///< Opaque non-owning implementation pointer.
	};	///< Public window-system wrapper.

} // namespace vve
