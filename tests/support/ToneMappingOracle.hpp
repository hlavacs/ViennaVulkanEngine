/** @file @brief Test-only independent Neutral reference, fixed vectors and same-process presentation evidence. */
#pragma once
namespace vve::test::tone {
using namespace vve;
using RGB = std::array<double, 3>; ///< Double oracle, independent of shader and FP16 storage.
/// @brief Missing presentation, validation, pixels or expected errors fail rather than skip.
void require(bool condition, std::string_view message) { if (!condition) throw std::runtime_error{std::string{message}}; }
/// @brief Display transfer curve; no GPU output enters the expected value.
double srgb(double value) { return value <= .0031308 ? 12.92 * value : 1.055 * std::pow(value, 1.0 / 2.4) - .055; }
/// @brief CPU twin of the pinned operator; fixed rational vectors below independently constrain the transcription.
RGB neutral(RGB color, double exposure = 1) {
	for (auto &c : color) { c = std::isnan(c) || c < 0 ? 0 : std::min(c, 65504.0); c = std::min(c * exposure, 1e6); }
	const double minimum = std::ranges::min(color);
	const double offset = minimum < .08 ? minimum - 6.25 * minimum * minimum : .04;
	for (auto &c : color) c -= offset;
	const double peak = std::ranges::max(color);
	if (peak >= .76) {
		const double compressed = 1 - .24 * .24 / (peak + .24 - .76);
		const double desaturate = 1 - 1 / (1 + .15 * (peak - compressed));
		for (auto &c : color) c = (1 - desaturate) * c * compressed / peak + desaturate * compressed;
	}
	return color;
}
/// @brief Fixed vectors from decimal/rational substitution: boundary .80-.04=.76; HDR peak 99/25, denominator 86/25.
void fixedVectors() {
	constexpr std::array inputs{RGB{0,0,0}, RGB{.08,.08,.08}, RGB{.8,.1,.1}, RGB{.81,.1,.1}, RGB{4,2,.5}, RGB{.02,.18,1}};
	constexpr std::array expected{RGB{0,0,0}, RGB{.04,.04,.04}, RGB{.76,.06,.06},
		RGB{.7696,.060011406484442066,.060011406484442066},
		RGB{.9832558139534884,.6399513870477753,.3824730668684905},
		RGB{.016026753424200017,.15634229726669133,.8754594594594595}};
	for (auto i : std::views::iota(0U, unsigned(inputs.size()))) {
		const auto actual = neutral(inputs[i]);
		for (auto c : std::views::iota(0U, 3U)) require(std::abs(actual[c]-expected[i][c]) < 1e-12, "independent fixed vector");
	}
}
/// @brief Assert actual window system, environment and Vulkan surface presentation from this rendering process.
void presentation(const simple::ForwardRenderer &renderer) {
	for (const auto *name : {"DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE", "SDL_VIDEODRIVER", "SDL_VIDEO_DRIVER"}) {
		std::println("environment.{}={}", name, std::getenv(name) ? std::getenv(name) : "unset");
	}
	VkBool32 supported{};
	require(vkGetPhysicalDeviceSurfaceSupportKHR(renderer.physicalDevice.physicalDevice, renderer.targets.front().presentQueueFamily,
		renderer.targets.front().surface.surface, &supported) == VK_SUCCESS && supported, "Vulkan surface presentation");
	VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice, &device);
	std::println("sdl.backend={} vulkan.presentation={} device={} device.type={} evidence={}", SDL_GetCurrentVideoDriver(), supported,
		device.deviceName, int(device.deviceType), device.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "software-renderer" : "physical-GPU");
}
} // namespace vve::test::tone

