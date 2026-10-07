module VVEngine;
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

	/**
		* @brief Applies the default keyboard camera motion and returns the resulting facade camera.
		* @param input Current facade input snapshot used for continuous movement and turning.
		* @param dt Elapsed seconds, clamped to [0, 0.1] to bound motion after a stall.
		* @return Camera looking from the updated eye position along the updated forward vector.
		*/
	auto DefaultCameraController::update(const InputState &input, DeltaTime dt) -> Camera {
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

} // namespace vve
