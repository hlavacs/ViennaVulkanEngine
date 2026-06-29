export module VEEngine:Window;
import std;
import VEEngine.Simple;
import VEEngine.Types;
import VEEngine.Vector;

/**
	* @file
	* @brief Public window/input contract backed by the selected engine implementation.
	*/
export namespace vve {

	class WindowSetup {
		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc;

	public:
		inline WindowSetup() = default;

		[[nodiscard]] inline WindowSetup &id(std::string value) {
			impl_.id = std::move(value);
			return *this;
		}
		[[nodiscard]] inline WindowSetup &title(std::string value) {
			impl_.title = std::move(value);
			return *this;
		}
		[[nodiscard]] inline WindowSetup &extent(PixelExtent value) {
			impl_.extent = value;
			return *this;
		}
		[[nodiscard]] inline WindowSetup &position(int x, int y) {
			impl_.x = x;
			impl_.y = y;
			return *this;
		}
		[[nodiscard]] inline WindowSetup &renderer(RendererId value) {
			impl_.renderer_id = std::move(value);
			return *this;
		}
		[[nodiscard]] inline WindowSetup &resizable(bool value) {
			impl_.resizable = value;
			return *this;
		}
		[[nodiscard]] inline WindowSetup &visible(bool value) {
			impl_.visible = value;
			return *this;
		}

		[[nodiscard]] inline operator Impl() const { return impl_; }

	private:
		Impl impl_{};
	};	///< Facade startup window option.

	class WindowSetups {
		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;

	public:
		inline WindowSetups() = default;
		inline WindowSetups(std::initializer_list<WindowSetup> windows) {
			impl_.value.clear();
			impl_.value.reserve(windows.size());
			for (const auto &window : windows) { impl_.value.push_back(window); }
		}

		inline void add(WindowSetup window) { impl_.value.push_back(std::move(window)); }

		[[nodiscard]] inline operator Impl() const { return impl_; }

	private:
		Impl impl_{};
	};	///< Facade startup window collection option.

	class InputState {
		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;

	public:
		inline explicit InputState(Impl &implementation) : impl_{implementation} {}
		InputState(const InputState &) = default;
		InputState(InputState &&) noexcept = default;
		InputState &operator=(const InputState &) = delete;
		InputState &operator=(InputState &&) noexcept = delete;

		inline void beginFrame() { impl_.beginFrame(); }
		inline void holdKey(std::int32_t keycode) { impl_.holdKey(keycode); }
		inline void pressKey(std::int32_t keycode) { impl_.pressKey(keycode); }
		inline void releaseKey(std::int32_t keycode) { impl_.releaseKey(keycode); }
		inline void setMousePosition(WindowHandle window, Vec2 position) { impl_.setMousePosition(window, position); }
		inline void addMouseDelta(WindowHandle window, Vec2 delta) { impl_.addMouseDelta(window, delta); }
		inline void addMouseWheelDelta(WindowHandle window, Vec2 delta) { impl_.addMouseWheelDelta(window, delta); }

		[[nodiscard]] inline bool isKeyDown(std::int32_t keycode) const { return impl_.isKeyDown(keycode); }
		[[nodiscard]] inline bool wasKeyPressed(std::int32_t keycode) const { return impl_.wasKeyPressed(keycode); }
		[[nodiscard]] inline bool wasKeyReleased(std::int32_t keycode) const { return impl_.wasKeyReleased(keycode); }
		[[nodiscard]] inline auto mousePosition(WindowHandle window) const					-> std::optional<Vec2>{
			return impl_.mousePosition(window);
		}
		[[nodiscard]] inline Vec2 mouseDelta(WindowHandle window) const { return impl_.mouseDelta(window); }
		[[nodiscard]] inline Vec2 mouseWheelDelta(WindowHandle window) const { return impl_.mouseWheelDelta(window); }

	private:
		Impl &impl_;
	};	///< Facade input snapshot.

	class Window {
		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window;

	public:
		inline explicit Window(Impl &implementation) : impl_{implementation} {}
		Window(const Window &) = default;
		Window(Window &&) noexcept = default;
		Window &operator=(const Window &) = delete;
		Window &operator=(Window &&) noexcept = delete;

		[[nodiscard]] inline WindowHandle handle() const { return impl_.info().handle; }
		[[nodiscard]] inline std::string_view id() const { return impl_.info().id; }
		[[nodiscard]] inline std::string_view title() const { return impl_.info().title; }
		[[nodiscard]] inline PixelExtent extent() const { return impl_.info().extent; }
		[[nodiscard]] inline RendererId rendererId() const { return impl_.info().renderer_id; }
		[[nodiscard]] inline std::optional<Entity> camera() const { return impl_.info().camera; }
		[[nodiscard]] inline bool focused() const { return impl_.info().focused; }
		[[nodiscard]] inline bool minimized() const { return impl_.info().minimized; }
		[[nodiscard]] inline bool shouldClose() const { return impl_.info().should_close; }

	private:
		Impl &impl_;
	};	///< Read-only facade window view.

	class WindowSystem {
		using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem;

	public:
		inline explicit WindowSystem(Impl &implementation) : impl_{implementation} {}
		WindowSystem(const WindowSystem &) = default;
		WindowSystem(WindowSystem &&) noexcept = default;
		WindowSystem &operator=(const WindowSystem &) = delete;
		WindowSystem &operator=(WindowSystem &&) noexcept = delete;

		[[nodiscard]] inline std::string_view name() const noexcept { return impl_.name(); }
		[[nodiscard]] inline InputState input() { return InputState{impl_.input()}; }
		[[nodiscard]] inline InputState input() const { return InputState{const_cast<Impl &>(impl_).input()}; }
		[[nodiscard]] inline std::size_t windowCount() const { return impl_.windowCount(); }
		[[nodiscard]] inline auto windows() const													-> Vector<Window>{
			Vector<Window> result{};
			const auto implementation_windows = impl_.windows();
			result.reserve(implementation_windows.size());
			for (const auto window : implementation_windows) { result.push_back(Window{window.get()}); }
			return result;
		}
		[[nodiscard]] inline auto findWindow(std::string_view id) const						-> std::optional<Window>{
			auto *window = impl_.findWindow(id);
			return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
		}
		[[nodiscard]] inline auto findWindow(WindowHandle handle) const						-> std::optional<Window>{
			auto *window = impl_.findWindow(handle);
			return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
		}
		[[nodiscard]] inline auto setWindowCamera(WindowHandle window, Entity camera)	-> std::expected<void, Error>{
			return impl_.setWindowCamera(window, camera);
		}
		[[nodiscard]] inline auto setWindowCamera(std::string_view id, Entity camera)	-> std::expected<void, Error>{
			return impl_.setWindowCamera(id, camera);
		}
		[[nodiscard]] inline auto clearWindowCamera(WindowHandle window)					-> std::expected<void, Error>{
			return impl_.clearWindowCamera(window);
		}
		[[nodiscard]] inline auto clearWindowCamera(std::string_view id)					-> std::expected<void, Error>{
			return impl_.clearWindowCamera(id);
		}
		[[nodiscard]] inline auto windowCamera(WindowHandle window) const					-> std::optional<Entity>{
			return impl_.windowCamera(window);
		}
		[[nodiscard]] inline auto windowCamera(std::string_view id) const					-> std::optional<Entity>{
			return impl_.windowCamera(id);
		}
		[[nodiscard]] inline auto setActiveCamera(Entity camera)								-> std::expected<void, Error>{
			return impl_.setActiveCamera(camera);
		}
		[[nodiscard]] inline std::optional<Entity> activeCamera() const { return impl_.activeCamera(); }

	private:
		Impl &impl_;
	};	///< Public window-system wrapper.

} // namespace vve
