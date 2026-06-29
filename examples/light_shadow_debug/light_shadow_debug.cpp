#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vulkan/vulkan.h>

import std;
import VEEngine.Simple.Math;
import VEEngine.Simple.Mesh;
import VEEngine.Simple.Renderer;
import VEEngine.Simple.Scene;
import VEEngine.Simple.Vulkan;

/**
 * @file
 * @brief Deterministic simple-renderer smoke scene for light/shadow example wiring.
 */
namespace {

[[nodiscard]] vve::simple::Mesh coloredMesh(vve::simple::Mesh mesh, std::array<float, 3U> color) {
	for (auto &vertex : mesh.vertices) { vertex.color = color; }
	return mesh;
}

[[nodiscard]] vve::simple::Scene makeShadowTestScene() {
	const vve::simple::Vec3 lightPosition{2.0F, 3.7F, -2.4F};
	const auto cubeModel = vve::simple::scale(
		vve::simple::translate(vve::simple::identityMat4(), vve::simple::Vec3{0.0F, 1.0F, -0.75F}),
		vve::simple::Vec3{0.8F, 2.0F, 0.8F}); // Tall cube rests on the XZ floor inside the fixed debug camera.
	const auto lightMarkerModel = vve::simple::scale(
		vve::simple::translate(vve::simple::identityMat4(), lightPosition),
		vve::simple::Vec3{0.25F, 0.25F, 0.25F}); // Visible marker at the active point-light position.
	return vve::simple::Scene{.objects{
		vve::simple::Object{.mesh = vve::simple::makePlane(vve::simple::Vec2{4.0F, 4.0F}), .model = vve::simple::identityMat4()}, ///< Large centered floor.
		vve::simple::Object{.mesh = vve::simple::makeCube(), .model = cubeModel},                                                ///< Single shadow caster.
		vve::simple::Object{.mesh = coloredMesh(vve::simple::makeCube(), {1.0F, 0.9F, 0.1F}), .model = lightMarkerModel},       ///< Visible point-light marker.
	}, .pointLight = vve::simple::PointLight{
		.position = lightPosition,
		.color = vve::simple::Vec3{1.0F, 0.92F, 0.55F},
		.intensity = 4.5F,
		.range = 7.5F,
		.ambient = 0.08F,
	}, .directionalLight = vve::simple::DirectionalLight{
		.direction = vve::simple::Vec3{-0.55F, -0.78F, 0.30F}, ///< Cool grazing light crosses the floor.
		.color = vve::simple::Vec3{0.65F, 0.82F, 1.0F},
		.intensity = {.value = 0.75F},
		.ambient = 0.025F,
	}, .spotLight = vve::simple::SpotLight{
		.position = vve::simple::Vec3{1.45F, 4.8F, -1.45F}, ///< Warm cone hangs over the lit floor marker region.
		.direction = vve::simple::Vec3{0.10F, -0.98F, -0.16F},
		.color = vve::simple::Vec3{1.0F, 0.58F, 0.38F},
		.intensity = {.value = 2.2F},
		.range = {.value = 5.8F},
		.innerConeAngle = {.radians = 0.28F},
		.outerConeAngle = {.radians = 0.58F},
		.ambient = 0.02F,
	}};
}

[[nodiscard]] std::filesystem::path outputPath(int argc, char **argv) {
	for (int index = 1; index + 1 < argc; ++index) {
		if (argv[index] != nullptr && argv[index + 1] != nullptr && std::string_view{argv[index]} == "--output") {
			return std::filesystem::path{argv[index + 1]};
		}
	}
	if (argc > 0 && argv[0] != nullptr) {
		const auto executableDirectory = std::filesystem::absolute(std::filesystem::path{argv[0]}).parent_path();
		return executableDirectory.parent_path() / "verify" / "light_shadow_debug.txt";
	}
	return std::filesystem::path{"verify"} / "light_shadow_debug.txt";
}

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

[[nodiscard]] std::filesystem::path pngOutputPath(const std::filesystem::path &textPath) {
	const auto baseDirectory = textPath.has_parent_path() ? textPath.parent_path() : std::filesystem::current_path();
	const auto verifyDirectory = baseDirectory.filename() == "verify" ? baseDirectory : baseDirectory / "verify";
	return verifyDirectory / "light_shadow_debug.png";
}

} // namespace

int main(int argc, char **argv) {
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;
	std::cout << "[light_shadow_debug] engine=simple\n";

	SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
	SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		std::cerr << "[light_shadow_debug] SDL video init failed: " << SDL_GetError() << '\n';
		return 1;
	}

	SDL_Window *const window = SDL_CreateWindow("VVE Simple Light Shadow Debug", 960, 540, SDL_WINDOW_VULKAN);
	if (window == nullptr) {
		std::cerr << "[light_shadow_debug] SDL Vulkan window creation failed: " << SDL_GetError() << '\n';
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 2;
	}

	vve::simple::Renderer renderer{};
	renderer.loadScene(makeShadowTestScene());
	if (const VkResult result = renderer.init(window); result != VK_SUCCESS) {
		std::cerr << "[light_shadow_debug] simple renderer init failed: vk_result=" << static_cast<int>(result) << '\n';
		renderer.cleanup();
		SDL_DestroyWindow(window);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 3;
	}

	const int maxFrames = frameLimit(argc, argv);
	const auto path = outputPath(argc, argv);
	const auto pngPath = pngOutputPath(path);
	const std::string pngPathText = pngPath.string(); // Encoder accepts string_view, so keep backing storage alive.
	vve::simple::VulkanReadback readback{}; // Readback owns the CPU-visible capture buffer until renderer teardown.
	std::array<std::array<double, 9U>, 9U> bandGridLuminance{}; // Coarse center-band samples locate the shadow in two axes.
	for (auto &row : bandGridLuminance) { row.fill(-1.0); }
	bool shadowDetected{};                   // True when the expected shadow sample is darker than lit floor.
	const VkResult readbackCreateResult = readback.create(
		renderer.physicalDevice.physicalDevice,
		renderer.device.device,
		renderer.device.graphicsQueue,
		renderer.commandPool.commandPool,
		renderer.swapchain.extent,
		renderer.swapchain.imageFormat);
	std::cout << "[light_shadow_debug] readback_create=" << static_cast<int>(readbackCreateResult) << '\n';

	for (int frame{}; frame < maxFrames; ++frame) {
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {}
		renderer.drawFrame(frame == 0 && readbackCreateResult == VK_SUCCESS ? &readback : nullptr);

		// Capture the first frame after submitted work has reached idle, matching the forward demo path.
		if (frame == 0) {
			const VkResult captureWaitResult = vkDeviceWaitIdle(renderer.device.device);
			const VkResult captureResult = captureWaitResult == VK_SUCCESS
				? renderer.lastReadbackCaptureResult.value_or(VK_ERROR_INITIALIZATION_FAILED)
				: captureWaitResult;
			if (auto parent = pngPath.parent_path(); captureResult == VK_SUCCESS && !parent.empty()) {
				std::filesystem::create_directories(parent);
			}
			const bool pngWritten = captureResult == VK_SUCCESS && vve::simple::writeReadbackPng(
				readback.pixelBytes(),
				renderer.swapchain.extent,
				renderer.swapchain.imageFormat,
				pngPathText);
			const bool rgbaFormat = renderer.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_UNORM || renderer.swapchain.imageFormat == VK_FORMAT_R8G8B8A8_SRGB;
			const bool bgraFormat = renderer.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM || renderer.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_SRGB;
			const auto sampleLuminance = [&](float xFraction, float yFraction) -> std::optional<double> {
				constexpr std::size_t channels{4U}; // Swapchain formats sampled here are four 8-bit color channels.
				const std::size_t width = static_cast<std::size_t>(renderer.swapchain.extent.width);
				const std::size_t height = static_cast<std::size_t>(renderer.swapchain.extent.height);
				if (captureResult != VK_SUCCESS || (!rgbaFormat && !bgraFormat) || width == 0U || height == 0U || readback.pixelBytes().size() != width * height * channels) {
					return std::nullopt;
				}
				const std::size_t x = std::min(width - 1U, static_cast<std::size_t>(xFraction * static_cast<float>(width)));
				const std::size_t y = std::min(height - 1U, static_cast<std::size_t>(yFraction * static_cast<float>(height)));
				const std::size_t offset = (y * width + x) * channels; // Vulkan copy uses tightly packed top-left row order.
				const double first = static_cast<double>(std::to_integer<unsigned char>(readback.pixelBytes()[offset + 0U]));
				const double second = static_cast<double>(std::to_integer<unsigned char>(readback.pixelBytes()[offset + 1U]));
				const double third = static_cast<double>(std::to_integer<unsigned char>(readback.pixelBytes()[offset + 2U]));
				const double red = bgraFormat ? third : first;
				const double green = second;
				const double blue = bgraFormat ? first : third;
				return (0.2126 * red + 0.7152 * green + 0.0722 * blue) / 255.0;
			};
			// Sample a coarse resolved-content band so lit floor, cube, and shadow can be separated in image space.
			for (const std::size_t row : std::views::iota(std::size_t{}, bandGridLuminance.size())) {
				const float yFraction = 0.34F + (0.24F * static_cast<float>(row) / static_cast<float>(bandGridLuminance.size() - 1U));
				for (const std::size_t column : std::views::iota(std::size_t{}, bandGridLuminance[row].size())) {
					const float xFraction = 0.20F + (0.60F * static_cast<float>(column) / static_cast<float>(bandGridLuminance[row].size() - 1U));
					if (const auto value = sampleLuminance(xFraction, yFraction)) { bandGridLuminance[row][column] = *value; }
				}
			}
			double darkestBandFloorLuminance{std::numeric_limits<double>::max()}; // Darkest center-floor sample inside the already captured band.
			double adjacentBandFloorLuminance{-1.0};                              // Brighter same-row floor sample next to that darkest sample.
			// Inspect the lower center band where the plane is visible and the cube casts its floor shadow.
			for (const std::size_t row : std::views::iota(std::size_t{3U}, bandGridLuminance.size())) {
				for (const std::size_t column : std::views::iota(std::size_t{2U}, bandGridLuminance[row].size() - 2U)) {
					const double current = bandGridLuminance[row][column];
					if (current < 0.0) { continue; }
					const double adjacent = std::max(bandGridLuminance[row][column - 1U], bandGridLuminance[row][column + 1U]);
					if (adjacent >= 0.0 && current < darkestBandFloorLuminance) {
						darkestBandFloorLuminance = current;
						adjacentBandFloorLuminance = adjacent;
					}
				}
			}
			shadowDetected = adjacentBandFloorLuminance >= 0.0 && darkestBandFloorLuminance + 0.08 <= adjacentBandFloorLuminance;
			std::cout << "[light_shadow_debug] readback_capture=" << static_cast<int>(captureResult) << '\n';
			std::cout << "[light_shadow_debug] png_written=" << pngWritten << " path=" << pngPath.string() << '\n';
		}
	}

	(void)vkDeviceWaitIdle(renderer.device.device);
	readback.cleanup();
	renderer.cleanup();
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if (auto parent = path.parent_path(); !parent.empty()) {
		std::filesystem::create_directories(parent);
	}
	std::ofstream output{path, std::ios::trunc};
	if (!output) {
		std::cerr << "[light_shadow_debug] could not write output: " << path.string() << '\n';
		return 4;
	}
	output << "engine=simple\n";
	output << "frames=" << maxFrames << '\n';
	output << "renderer=standalone-simple-forward\n";
	output << "luminance_band_grid=";
	for (const std::size_t row : std::views::iota(std::size_t{}, bandGridLuminance.size())) {
		if (row != 0U) { output << ';'; }
		for (const std::size_t column : std::views::iota(std::size_t{}, bandGridLuminance[row].size())) {
			if (column != 0U) { output << ','; }
			output << bandGridLuminance[row][column];
		}
	}
	output << '\n';
	output << "shadow_detected=" << shadowDetected << '\n';
	std::cout << "[light_shadow_debug] luminance_band_grid=";
	for (const std::size_t row : std::views::iota(std::size_t{}, bandGridLuminance.size())) {
		if (row != 0U) { std::cout << ';'; }
		for (const std::size_t column : std::views::iota(std::size_t{}, bandGridLuminance[row].size())) {
			if (column != 0U) { std::cout << ','; }
			std::cout << bandGridLuminance[row][column];
		}
	}
	std::cout << '\n';
	std::cout << "[light_shadow_debug] shadow_detected=" << shadowDetected << '\n';
	std::cout << "[light_shadow_debug] frames=" << maxFrames << '\n';
	return 0;
}
