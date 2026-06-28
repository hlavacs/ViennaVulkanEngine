#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>

import std;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;

/**
 * @file
 * @brief Sponza example shell running on the standalone simple renderer.
 */
namespace {

[[nodiscard]] int frameLimit(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] == nullptr || argv[index + 1] == nullptr) {
			continue;
		}
		if (std::string_view{argv[index]} != "--frames") {
			continue;
		}
		int value{};
		const std::string_view text{argv[index + 1]};
		const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
		if (result.ec == std::errc{} && value >= 0) {
			return value;
		}
	}
	return 1;
}

} // namespace

int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[sponza] engine=simple\n";

	SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
	SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		std::cerr << "[sponza] SDL video init failed: " << SDL_GetError() << '\n';
		return 1;
	}

	SDL_Window *const window = SDL_CreateWindow("VVE Simple Sponza", 1280, 720, SDL_WINDOW_VULKAN);
	if (window == nullptr) {
		std::cerr << "[sponza] SDL Vulkan window creation failed: " << SDL_GetError() << '\n';
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 2;
	}

	vve::simple::Renderer renderer{};
	renderer.loadScene(vve::simple::makeSampleScene());
	if (const VkResult result = renderer.init(window); result != VK_SUCCESS) {
		std::cerr << "[sponza] simple renderer init failed: vk_result=" << static_cast<int>(result) << '\n';
		renderer.cleanup();
		SDL_DestroyWindow(window);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 3;
	}

	const int maxFrames = frameLimit(argc, argv);
	for (int frame{}; frame < maxFrames; ++frame) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {}
		renderer.drawFrame(nullptr);
	}

	(void)vkDeviceWaitIdle(renderer.device.device);
	renderer.cleanup();
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	std::cout << "[sponza] frames=" << maxFrames << '\n';
	return 0;
}
