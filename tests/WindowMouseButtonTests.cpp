/**
 * @file
 * @brief Named keys, key(char), mouse buttons through the SDL event queue, and mouse capture on a hidden window.
 */
#include <SDL3/SDL.h>

import std;
import VVEngine;
import VVEngine.Simple;

namespace {

/// @brief Builds a mouse-button event for this test's hidden window.
[[nodiscard]] SDL_Event buttonEvent(SDL_WindowID window, std::uint32_t type, std::uint8_t button) {
	SDL_Event event{};
	event.type = type;
	event.button.windowID = window;
	event.button.button = button;
	event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
	return event;
}

} // namespace

/// @brief Checks key names at compile time, then pushes button events through poll.
int main() {
	using vve::Key;
	static_assert(vve::key('W') == Key::w && vve::key('w') == Key::w && vve::key(' ') == Key::space);
	static_assert(static_cast<std::int32_t>(vve::key('1')) == '1' && static_cast<std::int32_t>(vve::key('-')) == '-');
	static_assert(static_cast<std::int32_t>(Key::f12) == SDLK_F12 && static_cast<std::int32_t>(Key::page_down) == SDLK_PAGEDOWN &&
		static_cast<std::int32_t>(Key::left_ctrl) == SDLK_LCTRL && static_cast<std::int32_t>(Key::delete_key) == SDLK_DELETE &&
		static_cast<std::int32_t>(Key::enter) == SDLK_RETURN && static_cast<std::int32_t>(Key::right_alt) == SDLK_RALT);

	vve::simple::WindowSystem windows{};
	if (!windows.init(vve::simple::Windows{.value = {
		vve::WindowDesc{.id = "input", .extent = {64, 64}, .renderer_id = {"none"}, .visible = false}}})) { return 1; }
	const auto *window = windows.findWindow("input");
	if (!window || !windows.poll()) { return 2; } // Drain startup events before the synthetic input.
	const auto id = SDL_GetWindowID(window->native());
	auto left_down = buttonEvent(id, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT);
	auto left_up = buttonEvent(id, SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT);
	auto right_down = buttonEvent(id, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT);
	const auto &input = windows.input();
	const auto left = static_cast<std::int32_t>(Key::mouse_left);
	const auto right = static_cast<std::int32_t>(Key::mouse_right);

	// Buttons behave like keys: press, hold, release.
	if (!SDL_PushEvent(&left_down) || !SDL_PushEvent(&right_down) || !windows.poll()) { return 3; }
	const bool pressed = input.wasKeyPressed(left) && input.isKeyDown(left) && input.isKeyDown(right);
	if (!SDL_PushEvent(&left_up) || !windows.poll()) { return 4; }
	const bool released = input.wasKeyReleased(left) && !input.isKeyDown(left) && input.isKeyDown(right);
	std::println("[WindowMouseButtonTests] pressed={} released={}", pressed, released);
	if (!pressed || !released) { return 5; }

	// A press the GUI claims never reaches the game; the release is always delivered.
	windows.setGuiEventSink([](const SDL_Event &event) { return event.type == SDL_EVENT_MOUSE_BUTTON_DOWN; });
	if (!SDL_PushEvent(&left_down) || !windows.poll()) { return 6; }
	const bool claimed = !input.wasKeyPressed(left) && !input.isKeyDown(left);
	windows.setGuiEventSink({});
	if (!SDL_PushEvent(&left_up) || !windows.poll()) { return 7; }
	std::println("[WindowMouseButtonTests] claimed_press_ignored={}", claimed);
	if (!claimed || input.wasKeyReleased(left)) { return 8; }

	// Capture toggles on a known window; an unknown handle is rejected.
	const auto capture = windows.setMouseCaptured(window->info().handle, true);
	const auto release = windows.setMouseCaptured(window->info().handle, false);
	const auto unknown = windows.setMouseCaptured(vve::WindowHandle{}, true);
	std::println("[WindowMouseButtonTests] capture={} release={} unknown={}", capture ? "ok" : vve::errorName(capture.error()),
		release ? "ok" : vve::errorName(release.error()), unknown ? "ok" : vve::errorName(unknown.error()));
	return capture && release && !unknown && unknown.error() == vve::Error::invalid_handle ? 0 : 9;
}
