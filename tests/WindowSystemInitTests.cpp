/**
 * @file
 * @brief Checks transactional window initialization and normalized mouse motion.
 */
#include <SDL3/SDL.h>

import std;

import VVEngine.Simple;

/// @brief Rejects invalid startup descriptors, retries successfully, and preserves initialized windows.
int main() {
	auto system = vve::simple::WindowSystem{};
	const auto desc = vve::WindowDesc{.id = "a", .title = "window-system-init-tests",
		.extent = vve::PixelExtent{.width = 80, .height = 40}, .visible = false};

	// Duplicate ids must fail without retaining either window.
	const auto duplicate = system.init(vve::simple::Windows{.value = {desc, desc}});
	std::cout << "duplicate error=" << (duplicate ? "none" : vve::errorName(duplicate.error()))
		<< " windows=" << system.windowCount() << '\n';
	if (duplicate || duplicate.error() != vve::Error::duplicate_object || system.windowCount() != 0U) { return 1; }

	// Validate the whole list before creating even its first, valid window.
	auto empty_id = desc;
	empty_id.id.clear();
	auto zero_width = desc;
	zero_width.id = "b";
	zero_width.extent.width = 0;
	auto zero_height = desc;
	zero_height.id = "b";
	zero_height.extent.height = 0;
	for (const auto &invalid : {empty_id, zero_width, zero_height}) {
		const auto result = system.init(vve::simple::Windows{.value = {desc, invalid}});
		std::cout << "invalid id='" << invalid.id << "' extent=" << invalid.extent.width << 'x' << invalid.extent.height
			<< " error=" << (result ? "none" : vve::errorName(result.error())) << " windows=" << system.windowCount() << '\n';
		if (result || result.error() != vve::Error::invalid_argument || system.windowCount() != 0U) { return 2; }
	}

	// A corrected list succeeds after validation failures.
	const auto valid = vve::simple::Windows{.value = {desc}};
	const auto initialized = system.init(valid);
	std::cout << "valid error=" << (initialized ? "none" : vve::errorName(initialized.error()))
		<< " windows=" << system.windowCount() << '\n';
	if (!initialized || system.windowCount() != 1U) { return 3; }

	// Repeating initialization must not append windows or acquire another SDL video reference.
	const auto repeated = system.init(valid);
	std::cout << "repeated error=" << (repeated ? "none" : vve::errorName(repeated.error()))
		<< " windows=" << system.windowCount() << '\n';
	if (repeated || repeated.error() != vve::Error::already_initialized || system.windowCount() != 1U) { return 4; }

	/// Drain startup events before injecting motion into the hidden, non-square window.
	const auto *window = system.findWindow("a");
	int width{}, height{};
	if (!window || !SDL_GetWindowSize(window->native(), &width, &height) || width <= 0 || height <= 0
		|| !system.poll()) { return 5; }
	SDL_Event motion{};
	motion.type = SDL_EVENT_MOUSE_MOTION;
	motion.motion.windowID = SDL_GetWindowID(window->native());
	motion.motion.x = static_cast<float>(width) / 2.0F;
	motion.motion.y = static_cast<float>(height) / 4.0F;
	motion.motion.xrel = static_cast<float>(width) / 10.0F;
	motion.motion.yrel = -static_cast<float>(height) / 5.0F;
	if (!SDL_PushEvent(&motion) || !system.poll()) { return 6; }

	/// Position and delta use fractions of each window dimension, independent of its size.
	const auto position = system.input().mousePosition(window->info().handle);
	const auto delta = system.input().mouseDelta(window->info().handle);
	std::cout << "mouse extent=" << width << 'x' << height << " position="
		<< (position ? position->x : -1.0F) << ',' << (position ? position->y : -1.0F)
		<< " delta=" << delta.x << ',' << delta.y << '\n';
	if (!position || !(std::abs(position->x - 0.5F) < 1e-5F && std::abs(position->y - 0.25F) < 1e-5F
		&& std::abs(delta.x - 0.1F) < 1e-5F && std::abs(delta.y + 0.2F) < 1e-5F)) { return 7; }
	return 0;
}
