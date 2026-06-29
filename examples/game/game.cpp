#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>

import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;

/**
 * @file
 * @brief Interactive example that drives the standalone simple renderer.
 */
namespace {

[[nodiscard]] vve::simple::Scene makeGameScene() {
	constexpr vve::simple::Vec2 groundHalfExtent{6.0F, 4.0F}; ///< One centered XZ plane gives a compact play area.
	constexpr float cubeCenterY = 0.5F;                       ///< Unit cube bottom sits on the y=0 ground plane.
	return vve::simple::Scene{.objects{
		vve::simple::Object{.mesh = vve::simple::makePlane(groundHalfExtent), .model = vve::simple::identityMat4()}, ///< Green floor.
		vve::simple::Object{.mesh = vve::simple::makeCube(), .model = vve::simple::translate(vve::simple::identityMat4(), vve::simple::Vec3{-1.5F, cubeCenterY, -0.5F}), .useBaseColorTexture = 1U}, ///< Left cube.
		vve::simple::Object{.mesh = vve::simple::makeCube(), .model = vve::simple::translate(vve::simple::identityMat4(), vve::simple::Vec3{0.0F, cubeCenterY, 0.75F}), .useBaseColorTexture = 1U},   ///< Center cube.
		vve::simple::Object{.mesh = vve::simple::makeCube(), .model = vve::simple::translate(vve::simple::identityMat4(), vve::simple::Vec3{1.5F, cubeCenterY, -0.5F}), .useBaseColorTexture = 1U},  ///< Right cube.
	}, .baseColorTexture = std::filesystem::path{"assets/game/crate0/diffuse.png"}}; ///< Crate diffuse texture request exercises optional binding 2.
}

[[nodiscard]] std::optional<int> frameLimit(int argc, char **argv) {
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
	return std::nullopt;
}

} // namespace

/**
 * @brief Runs a small SDL/Vulkan loop using only the simple renderer.
 */
int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[game] engine=simple\n";

	SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
	SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		std::cerr << "[game] SDL video init failed: " << SDL_GetError() << '\n';
		return 1;
	}

	SDL_Window *const window = SDL_CreateWindow("VVE Simple Game", 960, 540, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (window == nullptr) {
		std::cerr << "[game] SDL Vulkan window creation failed: " << SDL_GetError() << '\n';
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 2;
	}

	vve::simple::Renderer renderer{};
	renderer.loadScene(makeGameScene());
	if (const VkResult result = renderer.init(window); result != VK_SUCCESS) {
		std::cerr << "[game] simple renderer init failed: vk_result=" << static_cast<int>(result)
				  << ", sdl_error=" << SDL_GetError() << '\n';
		renderer.cleanup();
		SDL_DestroyWindow(window);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 3;
	}

	const int maxFrames = frameLimit(argc, argv).value_or(0);
	int frame{};
	bool running = true;
	vve::simple::Vec3 cameraEye{0.0F, 6.0F, 9.0F};                                      ///< Matches the renderer default eye.
	const vve::simple::Vec3 initialTarget{0.0F, 1.0F, 0.0F};                              ///< Matches the renderer default target.
	const vve::simple::Vec3 worldUp{0.0F, 1.0F, 0.0F};                                    ///< Stable up axis for view and strafing.
	const vve::simple::Vec3 initialForward = vve::simple::normalize(vve::simple::subtract(initialTarget, cameraEye)); ///< Default view direction.
	float cameraYaw = std::atan2(initialForward.x, -initialForward.z);                    ///< Horizontal look angle around Y.
	float cameraPitch = std::asin(initialForward.y);                                      ///< Vertical look angle from the XZ plane.
	constexpr float moveStep = 0.08F;                                                     ///< Small fixed per-frame movement distance.
	constexpr float turnStep = 0.025F;                                                    ///< Small fixed per-frame rotation angle.
	constexpr float maxPitch = 1.45F;                                                     ///< Keeps the view away from the up-axis singularity.
	while (running && (maxFrames == 0 || frame < maxFrames)) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				running = false;
			}
		}

		const bool *const keys = SDL_GetKeyboardState(nullptr);
		if (keys[SDL_SCANCODE_LEFT]) {
			cameraYaw -= turnStep;
		}
		if (keys[SDL_SCANCODE_RIGHT]) {
			cameraYaw += turnStep;
		}
		if (keys[SDL_SCANCODE_UP]) {
			cameraPitch += turnStep;
		}
		if (keys[SDL_SCANCODE_DOWN]) {
			cameraPitch -= turnStep;
		}
		cameraPitch = vve::simple::clamp(cameraPitch, -maxPitch, maxPitch);

		const vve::simple::Vec3 forward = vve::simple::normalize(vve::simple::Vec3{
			std::cos(cameraPitch) * std::sin(cameraYaw), std::sin(cameraPitch), -std::cos(cameraPitch) * std::cos(cameraYaw)});
		const vve::simple::Vec3 right = vve::simple::normalize(vve::simple::cross(forward, worldUp));
		if (keys[SDL_SCANCODE_W]) {
			cameraEye = vve::simple::add(cameraEye, vve::simple::scale(forward, moveStep));
		}
		if (keys[SDL_SCANCODE_S]) {
			cameraEye = vve::simple::subtract(cameraEye, vve::simple::scale(forward, moveStep));
		}
		if (keys[SDL_SCANCODE_A]) {
			cameraEye = vve::simple::subtract(cameraEye, vve::simple::scale(right, moveStep));
		}
		if (keys[SDL_SCANCODE_D]) {
			cameraEye = vve::simple::add(cameraEye, vve::simple::scale(right, moveStep));
		}

		renderer.setCamera(cameraEye, vve::simple::add(cameraEye, forward));
		renderer.drawFrame(nullptr);
		++frame;
	}

	(void)vkDeviceWaitIdle(renderer.device.device);
	renderer.cleanup();
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	std::cout << "[game] frames=" << frame << '\n';
	return 0;
}
