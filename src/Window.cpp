module VEEngine;
import :Window;

namespace vve {

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	InputState::InputState(Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Starts a new input frame in the selected implementation.
	void InputState::beginFrame() { impl_.beginFrame(); }

	/// @brief Marks a key as held in the selected implementation.
	void InputState::holdKey(std::int32_t keycode) { impl_.holdKey(keycode); }

	/// @brief Marks a key press transition in the selected implementation.
	void InputState::pressKey(std::int32_t keycode) { impl_.pressKey(keycode); }

	/// @brief Marks a key release transition in the selected implementation.
	void InputState::releaseKey(std::int32_t keycode) { impl_.releaseKey(keycode); }

	/// @brief Stores a per-window mouse position in the selected implementation.
	void InputState::setMousePosition(WindowHandle window, Vec2 position) {
		impl_.setMousePosition(window, position);
	}

	/// @brief Accumulates a per-window mouse movement delta in the selected implementation.
	void InputState::addMouseDelta(WindowHandle window, Vec2 delta) { impl_.addMouseDelta(window, delta); }

	/// @brief Accumulates a per-window mouse wheel delta in the selected implementation.
	void InputState::addMouseWheelDelta(WindowHandle window, Vec2 delta) {
		impl_.addMouseWheelDelta(window, delta);
	}

	/// @brief Reports whether a raw keycode is currently down.
	bool InputState::isKeyDown(std::int32_t keycode) const { return impl_.isKeyDown(keycode); }

	/// @brief Reports whether a facade key is currently down.
	bool InputState::isKeyDown(Key key) const { return isKeyDown(static_cast<std::int32_t>(key)); }

	/// @brief Reports whether a raw keycode was pressed during the current frame.
	bool InputState::wasKeyPressed(std::int32_t keycode) const { return impl_.wasKeyPressed(keycode); }

	/// @brief Reports whether a facade key was pressed during the current frame.
	bool InputState::wasKeyPressed(Key key) const { return wasKeyPressed(static_cast<std::int32_t>(key)); }

	/// @brief Reports whether a raw keycode was released during the current frame.
	bool InputState::wasKeyReleased(std::int32_t keycode) const { return impl_.wasKeyReleased(keycode); }

	/// @brief Reports whether a facade key was released during the current frame.
	bool InputState::wasKeyReleased(Key key) const { return wasKeyReleased(static_cast<std::int32_t>(key)); }

	/// @brief Returns the last known mouse position for a window when one is available.
	auto InputState::mousePosition(WindowHandle window) const -> std::optional<Vec2> {
		return impl_.mousePosition(window);
	}

	/// @brief Returns the accumulated mouse movement delta for a window.
	Vec2 InputState::mouseDelta(WindowHandle window) const { return impl_.mouseDelta(window); }

	/// @brief Returns the accumulated mouse wheel delta for a window.
	Vec2 InputState::mouseWheelDelta(WindowHandle window) const { return impl_.mouseWheelDelta(window); }

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	Window::Window(const Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Returns the stable runtime handle of the selected implementation window.
	WindowHandle Window::handle() const { return impl_.info().handle; }

	/// @brief Returns the application-local id of the selected implementation window.
	std::string_view Window::id() const { return impl_.info().id; }

	/// @brief Returns the platform title of the selected implementation window.
	std::string_view Window::title() const { return impl_.info().title; }

	/// @brief Returns the current pixel extent of the selected implementation window.
	PixelExtent Window::extent() const { return impl_.info().extent; }

	/// @brief Returns the renderer id associated with the selected implementation window.
	RendererId Window::rendererId() const { return impl_.info().renderer_id; }

	/// @brief Returns the camera assigned to the selected implementation window when one exists.
	std::optional<Entity> Window::camera() const { return impl_.info().camera; }

	/// @brief Reports whether the selected implementation window currently has focus.
	bool Window::focused() const { return impl_.info().focused; }

	/// @brief Reports whether the selected implementation window is minimized.
	bool Window::minimized() const { return impl_.info().minimized; }

	/// @brief Reports whether the selected implementation window received a close request.
	bool Window::shouldClose() const { return impl_.info().should_close; }

	/// @brief Binds the facade wrapper to the implementation object owned by the engine.
	WindowSystem::WindowSystem(Impl &implementation) noexcept : impl_{implementation} {}

	/// @brief Returns the diagnostic name of the selected window system implementation.
	std::string_view WindowSystem::name() const noexcept { return impl_.name(); }

	/// @brief Returns a facade input view for the window system's owned input state.
	InputState WindowSystem::input() { return InputState{impl_.input()}; }

	/// @brief Returns a facade input view for the window system's owned input state.
	InputState WindowSystem::input() const { return InputState{impl_.input()}; }

	/// @brief Returns the number of windows owned by the selected implementation.
	std::size_t WindowSystem::windowCount() const { return impl_.windowCount(); }

	/// @brief Returns facade views over the windows currently owned by the selected implementation.
	auto WindowSystem::windows() const -> Vector<Window> {
		Vector<Window> result{};
		const auto implementation_windows = impl_.windows();
		result.reserve(implementation_windows.size());
		for (const auto window : implementation_windows) {
			result.push_back(Window{window.get()});
		}
		return result;
	}

	/// @brief Finds a facade window view by application-local id.
	auto WindowSystem::findWindow(std::string_view id) const -> std::optional<Window> {
		auto *window = impl_.findWindow(id);
		return window == nullptr ? std::optional<Window>{}
										 : std::optional<Window>{Window{*window}};
	}

	/// @brief Finds a facade window view by runtime handle.
	auto WindowSystem::findWindow(WindowHandle handle) const -> std::optional<Window> {
		auto *window = impl_.findWindow(handle);
		return window == nullptr ? std::optional<Window>{}
										 : std::optional<Window>{Window{*window}};
	}

	/// @brief Assigns a camera entity to a window selected by runtime handle.
	auto WindowSystem::setWindowCamera(WindowHandle window, Entity camera) -> std::expected<void, Error> {
		return impl_.setWindowCamera(window, camera);
	}

	/// @brief Assigns a camera entity to a window selected by application-local id.
	auto WindowSystem::setWindowCamera(std::string_view id, Entity camera) -> std::expected<void, Error> {
		return impl_.setWindowCamera(id, camera);
	}

	/// @brief Clears the camera entity from a window selected by runtime handle.
	auto WindowSystem::clearWindowCamera(WindowHandle window) -> std::expected<void, Error> {
		return impl_.clearWindowCamera(window);
	}

	/// @brief Clears the camera entity from a window selected by application-local id.
	auto WindowSystem::clearWindowCamera(std::string_view id) -> std::expected<void, Error> {
		return impl_.clearWindowCamera(id);
	}

	/// @brief Returns the camera entity assigned to a window selected by runtime handle.
	auto WindowSystem::windowCamera(WindowHandle window) const -> std::optional<Entity> {
		return impl_.windowCamera(window);
	}

	/// @brief Returns the camera entity assigned to a window selected by application-local id.
	auto WindowSystem::windowCamera(std::string_view id) const -> std::optional<Entity> {
		return impl_.windowCamera(id);
	}

	/// @brief Sets the active camera entity used by default rendering paths.
	auto WindowSystem::setActiveCamera(Entity camera) -> std::expected<void, Error> {
		return impl_.setActiveCamera(camera);
	}

	/// @brief Returns the active camera entity when one is selected.
	std::optional<Entity> WindowSystem::activeCamera() const { return impl_.activeCamera(); }

} // namespace vve
