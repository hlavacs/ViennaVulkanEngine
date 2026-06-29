module VEEngine;
import :Window;
import VEEngine.Simple;

namespace vve {

	namespace {
		/// @brief Recovers the selected implementation input state from the erased facade pointer.
		[[nodiscard]] auto inputImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState *>(implementation);
		}

		/// @brief Recovers the selected implementation window from the erased facade pointer.
		[[nodiscard]] auto windowImpl(void *implementation) -> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window *>(implementation);
		}

		/// @brief Recovers the selected implementation window system from the erased facade pointer.
		[[nodiscard]] auto windowSystemImpl(void *implementation)
			-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem & {
			return *static_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem *>(implementation);
		}
	} // namespace

	/// @brief Stores the erased implementation reference used by engine-owned input state views.
	InputState::InputState(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Starts a new input frame in the selected implementation.
	void InputState::beginFrame() { inputImpl(impl_).beginFrame(); }

	/// @brief Marks a key as held in the selected implementation.
	void InputState::holdKey(std::int32_t keycode) { inputImpl(impl_).holdKey(keycode); }

	/// @brief Marks a key press transition in the selected implementation.
	void InputState::pressKey(std::int32_t keycode) { inputImpl(impl_).pressKey(keycode); }

	/// @brief Marks a key release transition in the selected implementation.
	void InputState::releaseKey(std::int32_t keycode) { inputImpl(impl_).releaseKey(keycode); }

	/// @brief Stores a per-window mouse position in the selected implementation.
	void InputState::setMousePosition(WindowHandle window, Vec2 position) {
		inputImpl(impl_).setMousePosition(window, position);
	}

	/// @brief Accumulates a per-window mouse movement delta in the selected implementation.
	void InputState::addMouseDelta(WindowHandle window, Vec2 delta) { inputImpl(impl_).addMouseDelta(window, delta); }

	/// @brief Accumulates a per-window mouse wheel delta in the selected implementation.
	void InputState::addMouseWheelDelta(WindowHandle window, Vec2 delta) {
		inputImpl(impl_).addMouseWheelDelta(window, delta);
	}

	/// @brief Reports whether a raw keycode is currently down.
	bool InputState::isKeyDown(std::int32_t keycode) const { return inputImpl(impl_).isKeyDown(keycode); }

	/// @brief Reports whether a facade key is currently down.
	bool InputState::isKeyDown(Key key) const { return isKeyDown(static_cast<std::int32_t>(key)); }

	/// @brief Reports whether a raw keycode was pressed during the current frame.
	bool InputState::wasKeyPressed(std::int32_t keycode) const { return inputImpl(impl_).wasKeyPressed(keycode); }

	/// @brief Reports whether a facade key was pressed during the current frame.
	bool InputState::wasKeyPressed(Key key) const { return wasKeyPressed(static_cast<std::int32_t>(key)); }

	/// @brief Reports whether a raw keycode was released during the current frame.
	bool InputState::wasKeyReleased(std::int32_t keycode) const { return inputImpl(impl_).wasKeyReleased(keycode); }

	/// @brief Reports whether a facade key was released during the current frame.
	bool InputState::wasKeyReleased(Key key) const { return wasKeyReleased(static_cast<std::int32_t>(key)); }

	/// @brief Returns the last known mouse position for a window when one is available.
	auto InputState::mousePosition(WindowHandle window) const -> std::optional<Vec2> {
		return inputImpl(impl_).mousePosition(window);
	}

	/// @brief Returns the accumulated mouse movement delta for a window.
	Vec2 InputState::mouseDelta(WindowHandle window) const { return inputImpl(impl_).mouseDelta(window); }

	/// @brief Returns the accumulated mouse wheel delta for a window.
	Vec2 InputState::mouseWheelDelta(WindowHandle window) const { return inputImpl(impl_).mouseWheelDelta(window); }

	/// @brief Stores the erased implementation reference used by engine-owned window views.
	Window::Window(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Returns the stable runtime handle of the selected implementation window.
	WindowHandle Window::handle() const { return windowImpl(impl_).info().handle; }

	/// @brief Returns the application-local id of the selected implementation window.
	std::string_view Window::id() const { return windowImpl(impl_).info().id; }

	/// @brief Returns the platform title of the selected implementation window.
	std::string_view Window::title() const { return windowImpl(impl_).info().title; }

	/// @brief Returns the current pixel extent of the selected implementation window.
	PixelExtent Window::extent() const { return windowImpl(impl_).info().extent; }

	/// @brief Returns the renderer id associated with the selected implementation window.
	RendererId Window::rendererId() const { return windowImpl(impl_).info().renderer_id; }

	/// @brief Returns the camera assigned to the selected implementation window when one exists.
	std::optional<Entity> Window::camera() const { return windowImpl(impl_).info().camera; }

	/// @brief Reports whether the selected implementation window currently has focus.
	bool Window::focused() const { return windowImpl(impl_).info().focused; }

	/// @brief Reports whether the selected implementation window is minimized.
	bool Window::minimized() const { return windowImpl(impl_).info().minimized; }

	/// @brief Reports whether the selected implementation window received a close request.
	bool Window::shouldClose() const { return windowImpl(impl_).info().should_close; }

	/// @brief Stores the erased implementation reference used by engine-owned window system views.
	WindowSystem::WindowSystem(void *implementation) noexcept : impl_{implementation} {}

	/// @brief Returns the diagnostic name of the selected window system implementation.
	std::string_view WindowSystem::name() const noexcept { return windowSystemImpl(impl_).name(); }

	/// @brief Returns a facade input view for the window system's owned input state.
	InputState WindowSystem::input() { return InputState{std::addressof(windowSystemImpl(impl_).input())}; }

	/// @brief Returns a facade input view for the window system's owned input state.
	InputState WindowSystem::input() const { return InputState{std::addressof(windowSystemImpl(impl_).input())}; }

	/// @brief Returns the number of windows owned by the selected implementation.
	std::size_t WindowSystem::windowCount() const { return windowSystemImpl(impl_).windowCount(); }

	/// @brief Returns facade views over the windows currently owned by the selected implementation.
	auto WindowSystem::windows() const -> Vector<Window> {
		Vector<Window> result{};
		const auto implementation_windows = windowSystemImpl(impl_).windows();
		result.reserve(implementation_windows.size());
		for (const auto window : implementation_windows) {
			result.push_back(Window{std::addressof(const_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window &>(window.get()))});
		}
		return result;
	}

	/// @brief Finds a facade window view by application-local id.
	auto WindowSystem::findWindow(std::string_view id) const -> std::optional<Window> {
		auto *window = windowSystemImpl(impl_).findWindow(id);
		return window == nullptr ? std::optional<Window>{}
										 : std::optional<Window>{Window{
												 std::addressof(const_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window &>(*window))}};
	}

	/// @brief Finds a facade window view by runtime handle.
	auto WindowSystem::findWindow(WindowHandle handle) const -> std::optional<Window> {
		auto *window = windowSystemImpl(impl_).findWindow(handle);
		return window == nullptr ? std::optional<Window>{}
										 : std::optional<Window>{Window{
												 std::addressof(const_cast<VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window &>(*window))}};
	}

	/// @brief Assigns a camera entity to a window selected by runtime handle.
	auto WindowSystem::setWindowCamera(WindowHandle window, Entity camera) -> std::expected<void, Error> {
		return windowSystemImpl(impl_).setWindowCamera(window, camera);
	}

	/// @brief Assigns a camera entity to a window selected by application-local id.
	auto WindowSystem::setWindowCamera(std::string_view id, Entity camera) -> std::expected<void, Error> {
		return windowSystemImpl(impl_).setWindowCamera(id, camera);
	}

	/// @brief Clears the camera entity from a window selected by runtime handle.
	auto WindowSystem::clearWindowCamera(WindowHandle window) -> std::expected<void, Error> {
		return windowSystemImpl(impl_).clearWindowCamera(window);
	}

	/// @brief Clears the camera entity from a window selected by application-local id.
	auto WindowSystem::clearWindowCamera(std::string_view id) -> std::expected<void, Error> {
		return windowSystemImpl(impl_).clearWindowCamera(id);
	}

	/// @brief Returns the camera entity assigned to a window selected by runtime handle.
	auto WindowSystem::windowCamera(WindowHandle window) const -> std::optional<Entity> {
		return windowSystemImpl(impl_).windowCamera(window);
	}

	/// @brief Returns the camera entity assigned to a window selected by application-local id.
	auto WindowSystem::windowCamera(std::string_view id) const -> std::optional<Entity> {
		return windowSystemImpl(impl_).windowCamera(id);
	}

	/// @brief Sets the active camera entity used by default rendering paths.
	auto WindowSystem::setActiveCamera(Entity camera) -> std::expected<void, Error> {
		return windowSystemImpl(impl_).setActiveCamera(camera);
	}

	/// @brief Returns the active camera entity when one is selected.
	std::optional<Entity> WindowSystem::activeCamera() const { return windowSystemImpl(impl_).activeCamera(); }

} // namespace vve
