/**
 * @file
 * @brief Fullscreen checks: WindowSetup::fullscreen reaches SDL window creation, and setFullscreen switches it.
 */
#include <SDL3/SDL.h>

import std;
import VVEngine;

namespace {

/// @brief Reads the fullscreen flag of the only SDL window this test creates.
[[nodiscard]] std::optional<bool> sdlFullscreen() {
	int count{};
	auto *windows = SDL_GetWindows(&count);
	const auto flag = count == 1 ? std::optional{(SDL_GetWindowFlags(windows[0]) & SDL_WINDOW_FULLSCREEN) != 0U} : std::nullopt;
	SDL_free(windows);
	return flag;
}

} // namespace

/// @brief Starts a hidden fullscreen window through the facade, then switches it off and on again.
int main() {
	auto engine = vve::EngineBuilder<>{}.applicationName("window-fullscreen-tests")
		.addWindow(vve::WindowSetup{}.id("main").extent(vve::PixelExtent{.width = 64, .height = 64}).visible(false).fullscreen(true))
		.build();
	if (!engine.init()) { return 1; }
	auto &windows = engine.world().get<vve::WindowSystem>();
	const auto window = windows.findWindow("main");
	if (!window) { return 2; }
	const auto at_start = sdlFullscreen();
	const auto off = windows.setFullscreen(window->handle(), false);
	const auto after_off = sdlFullscreen();
	const auto on = windows.setFullscreen(window->handle(), true);
	const auto after_on = sdlFullscreen();
	const auto unknown = windows.setFullscreen(vve::WindowHandle{}, true);
	std::println("[WindowFullscreenTests] start={} off={} after_off={} on={} after_on={} unknown={}", at_start.value_or(false),
		off.has_value(), after_off.value_or(true), on.has_value(), after_on.value_or(false),
		unknown ? "ok" : vve::errorName(unknown.error()));
	return at_start == true && off && after_off == false && on && after_on == true && !unknown &&
		unknown.error() == vve::Error::invalid_handle ? 0 : 3;
}
