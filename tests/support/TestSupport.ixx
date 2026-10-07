/** @file @brief Shared hidden engines, text fixtures and RGBA capture measurements for tests. */
module;
#include <ranges> // Keep range concepts visible when Clang merges engine imports with Microsoft's std module.
#include <stb_image.h>

export module VVE.TestSupport;
import std;
import VVEngine.Simple;

export namespace vve::test {

/// @brief Mean and population deviation of decoded image luminance.
struct ImageStatistics {
	double mean{};               ///< Mean Rec. 709 luminance in byte units.
	double standard_deviation{}; ///< Population standard deviation of luminance.
};

/// @brief Single-triangle OBJ shared by facade scene-lifetime tests.
inline constexpr std::string_view triangleObj = "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";

/// @brief Configures an uninitialized simple engine with one hidden forward window named main.
[[nodiscard]] simple::Engine hiddenEngine(std::string name, PixelExtent extent, MaxFrames max_frames = {}) {
	return simple::Engine{ApplicationName{name}, max_frames,
		simple::Windows{.value = {WindowDesc{.id = "main", .title = std::move(name), .extent = extent,
			.renderer_id = RendererId{.value = "forward"}, .visible = false}}}};
}

/// @brief Writes a text fixture in its build-tree directory and returns its path for import checks.
[[nodiscard]] std::filesystem::path writeFixture(const std::filesystem::path &directory,
	const std::filesystem::path &name, std::string_view text) {
	std::filesystem::create_directories(directory); // Retain fixtures for inspection after failures.
	const auto path = directory / name;
	std::ofstream file{path};
	file << text;
	return path;
}

/// @brief Borrows one RGBA8 pixel; callers validate the image dimensions and coordinates first.
[[nodiscard]] const unsigned char *pixelAt(const unsigned char *pixels, int width, int x, int y) {
	return pixels + (y * width + x) * 4;
}

/// @brief Decodes one PNG into tightly packed RGBA8 pixels, rejecting missing or empty images.
[[nodiscard]] auto imagePixels(const std::filesystem::path &path) -> std::optional<std::vector<unsigned char>> {
	int width{}, height{}, channels{};
	const auto source = path.string();
	auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{
		stbi_load(source.c_str(), &width, &height, &channels, STBI_rgb_alpha), stbi_image_free};
	if (!pixels || width <= 0 || height <= 0) { return std::nullopt; }
	const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	return std::vector<unsigned char>{pixels.get(), pixels.get() + pixel_count * 4U};
}

/// @brief Computes luminance statistics over a nonempty range of RGBA pixel byte offsets.
template <std::ranges::sized_range TMask>
[[nodiscard]] ImageStatistics imageStatistics(const std::vector<unsigned char> &pixels, const TMask &mask) {
	auto luminance = std::vector<double>{};
	luminance.reserve(std::ranges::size(mask));
	// The caller selects either the object mask or every pixel, preserving its capture metric.
	for (const auto offset : mask) {
		luminance.push_back(0.2126 * pixels[offset] + 0.7152 * pixels[offset + 1U] +
			0.0722 * pixels[offset + 2U]);
	}
	const double mean = std::accumulate(luminance.begin(), luminance.end(), 0.0) /
		static_cast<double>(std::ranges::size(mask));
	const double variance = std::accumulate(luminance.begin(), luminance.end(), 0.0,
		[mean](double total, double value) { const double difference = value - mean; return total + difference * difference; }) /
		static_cast<double>(std::ranges::size(mask));
	return ImageStatistics{.mean = mean, .standard_deviation = std::sqrt(variance)};
}

/// @brief Computes mean absolute RGBA byte difference over a nonempty mask in equally sized captures.
[[nodiscard]] double meanAbsoluteDifference(const std::vector<unsigned char> &left,
	const std::vector<unsigned char> &right, std::span<const std::size_t> mask) {
	double difference{};
	// Compare corresponding object pixels, including alpha just as the capture tests do.
	for (const auto offset : mask) {
		for (const auto channel : std::views::iota(0U, 4U)) {
			difference += std::abs(static_cast<double>(left[offset + channel]) - right[offset + channel]);
		}
	}
	return difference / static_cast<double>(mask.size() * 4U);
}

} // namespace vve::test
