/** @file @brief Shared hidden engines, text fixtures and RGBA capture measurements for tests. */
module;
#include <ranges> // Keep range concepts visible when Clang merges engine imports with Microsoft's std module.
#include <stb_image.h>
#include <vulkan/vulkan_core.h>

export module VVE.TestSupport;
import std;
import VVEngine.Simple;
import VVEngine.Math;
import VVEngine.Simple.Vulkan;
import VVEngine.Simple.IrradianceProbes;

export namespace vve::test {

/// @brief Converts one IEEE binary16 component from an untonemapped HDR readback to linear float.
[[nodiscard]] float linearHalf(std::uint16_t bits) {
	const auto exponent = (bits >> 10) & 31U, mantissa = bits & 1023U;
	const float value = exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24)
		: exponent == 31 ? std::numeric_limits<float>::infinity()
		: std::ldexp(1.0F + static_cast<float>(mantissa) / 1024.0F, static_cast<int>(exponent) - 15);
	return bits & 32768U ? -value : value;
}
/// @brief Opt-in HDR region measurement for small graphics fixtures; reads the completed linear scene before display encoding.
template <typename TRenderer>
[[nodiscard]] auto linearRegion(TRenderer &renderer, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) -> std::optional<Vec3> {
	if (renderer.targets.empty() || width == 0 || height == 0) { return {}; }
	auto &target = renderer.targets.front();
	const auto extent = target.hdrImage.extent;
	if (extent.width * extent.height > 256U * 256U || x + width > extent.width || y + height > extent.height) { return {}; }
	simple::VulkanReadback readback;
	if (readback.create(renderer.allocator, renderer.device.device, renderer.device.graphicsQueue, renderer.commandPool.commandPool,
		extent, VK_FORMAT_R16G16B16A16_SFLOAT) != VK_SUCCESS ||
		readback.capture(target.hdrImage.image, 0, VK_IMAGE_LAYOUT_GENERAL) != VK_SUCCESS) { return {}; }
	Vec3 sum{};
	// Average only the explicit receiver region; sky and the colored wall cannot dilute the measurement.
	for (auto row : std::views::iota(y, y + height)) {
		for (auto column : std::views::iota(x, x + width)) {
			std::array<std::uint16_t,4> texel{};
			std::memcpy(texel.data(), readback.pixelBytes().data() + (row * extent.width + column) * 8U, 8U);
			sum = math::add(sum, Vec3{linearHalf(texel[0]), linearHalf(texel[1]), linearHalf(texel[2])});
		}
	}
	return math::scale(sum, math::one() / static_cast<Scalar>(width * height));
}

/// @brief Reads actual GPU validity and hit metadata only at a diagnostic checkpoint; verifies every stored float is finite.
template <typename TRenderer>
std::optional<std::pair<unsigned,unsigned>> probeValidity(TRenderer &renderer, unsigned cascade = 0) {
    using namespace vve::simple;
    const auto &p = cascade == 0 ? renderer.targets.front().probes : renderer.targets.front().coarseProbes;
    VulkanReadback readback;
    if (readback.create(renderer.allocator,renderer.device.device,renderer.device.graphicsQueue,renderer.commandPool.commandPool,
        p.coefficients.extent,VK_FORMAT_R32G32B32A32_SFLOAT) != VK_SUCCESS ||
        readback.capture(p.coefficients.image,0,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != VK_SUCCESS) return {};
    const auto bytes = readback.pixelBytes();
    unsigned active{}, inactive{};
    // Untouched rows have zero moments; inactive captured rows retain moments but have zero SH alpha.
    for (auto row : std::views::iota(0U,IrradianceProbes::count)) {
        std::array<float,4> sh{}, moments{};
        const auto offset = row * IrradianceProbes::atlasColumns * sizeof(sh);
        std::memcpy(sh.data(),bytes.data() + offset,sizeof(sh));
        std::memcpy(moments.data(),bytes.data() + offset + IrradianceProbes::shCoefficients * sizeof(sh),sizeof(moments));
        active += sh[3] > .5F;
        inactive += sh[3] == 0 && moments[0] > 0;
        for (auto column : std::views::iota(0U,IrradianceProbes::atlasColumns * 4)) {
            float value{}; std::memcpy(&value,bytes.data() + offset + column * sizeof(float),sizeof(float));
            if (!std::isfinite(value)) return {};
        }
    }
    std::println("gi.cascade={} gi.probes.valid={} gi.probes.inactive={} gi.probes.stale={} gi.probes.updated={} gi.cascade.gpu_bytes={} gi.storage.finite=1",
        cascade,active,inactive,p.stale,p.updated,p.gpuBytes);
    const auto &target = renderer.targets.front();
    std::println("gi.total_gpu_bytes={} gi.history_resets={} gpu.live_allocations={} gpu.retired_owners={}",
        renderer.giGpuBytes(), target.probeResets, renderer.gpuAllocationCount(), renderer.retiredResourceCount());
    std::println("gi.cascades={} gi.gpu_bytes={} gi.near.gpu_bytes={} gi.coarse.gpu_bytes={} gi.probe_shadow.gpu_bytes={} gi.probe_shadow.resolution={} gi.probe_shadow.renders={}",
        unsigned(target.probes.coefficients.image!=VK_NULL_HANDLE)+unsigned(target.coarseProbes.coefficients.image!=VK_NULL_HANDLE),
        target.probes.gpuBytes+target.coarseProbes.gpuBytes+target.probeShadowBytes,
        target.probes.gpuBytes,target.coarseProbes.gpuBytes,target.probeShadowBytes,target.probeShadow.extent.width,target.probeShadowRenders);
    std::println("ao.enabled={} ao.gpu_bytes={} ao.gpu_ms={}",target.aoEnabled,target.aoGpuBytes,
        target.aoGpuMs ? std::format("{:.6f}",*target.aoGpuMs) : "unavailable");
    return std::pair{active,inactive};
}

/// @brief Mean and population deviation of decoded image luminance.
struct ImageStatistics {
	double mean{};               ///< Mean Rec. 709 luminance in byte units.
	double standard_deviation{}; ///< Population standard deviation of luminance.
};

/// @brief Single-triangle OBJ shared by facade scene-lifetime tests.
inline constexpr std::string_view triangleObj = "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";

/// @brief Selects the runtime backend requested by a regression's CTest environment; absent means unchanged default.
template <typename TRender> void configureRenderer(TRender &render) {
	const char *configured = std::getenv("VVE_TEST_RENDERER");
	if (configured == nullptr) { return; }
	const std::string_view name{configured};
	if (name != "forward" && name != "deferred") { throw std::invalid_argument{"VVE_TEST_RENDERER must be forward or deferred"}; }
	const auto backend = name == "deferred" ? RendererBackend::deferred : RendererBackend::forward;
	if (const auto result = render.setRendererBackend(backend); !result) {
		throw std::runtime_error{std::format("renderer selection {} failed: {}", name, errorName(result.error()))};
	}
	std::println("[VVE.TestSupport] renderer={}", name);
}

/// @brief Configures an uninitialized simple engine with one hidden forward window named main.
[[nodiscard]] simple::Engine hiddenEngine(std::string name, PixelExtent extent, MaxFrames max_frames = {}) {
	return simple::Engine{ApplicationName{name}, max_frames,
		simple::Windows{.value = {WindowDesc{.id = "main", .title = std::move(name), .extent = extent,
			.renderer_id = RendererId{.value = "forward"}, .visible = false}}}};
}

/// @brief Writes a fixture byte for byte in its build-tree directory and returns its path for import checks.
[[nodiscard]] std::filesystem::path writeFixture(const std::filesystem::path &directory,
	const std::filesystem::path &name, std::string_view text) {
	std::filesystem::create_directories(directory); // Retain fixtures for inspection after failures.
	const auto path = directory / name;
	// Binary mode: on Windows a text stream would turn every \n into \r\n and shift the pixels of binary images (PPM).
	std::ofstream file{path, std::ios::binary};
	file << text;
	return path;
}

/// @brief Advances a fixed capture budget with paused animation; no CPU readback enters ordinary rendering.
template <typename TEngine> bool renderFrames(TEngine &engine, unsigned count = 64) {
	for ([[maybe_unused]] auto frame : std::views::iota(0U, count)) if (!engine.renderFrame()) return false;
	return true;
}
/// @brief Projects a known surface point into a two-square-pixel linear HDR region and prints its exact rectangle.
template <typename TRenderer> std::optional<Vec3> linearPoint(TRenderer &renderer, Vec3 point, std::string_view name) {
	const auto &f = renderer.frameUniforms();
	const auto p = math::multiply(math::multiply(f.projection, f.view), Vec4{point.x, point.y, point.z, 1});
	const int x = int((p.x / p.w * .5F + .5F) * f.viewport.x) - 1;
	const int y = int((p.y / p.w * .5F + .5F) * f.viewport.y) - 1;
	if (x < 0 || y < 0) return {};
	const auto value = linearRegion(renderer, x, y, 2, 2);
	if (value) std::println("region={} rect={},{},2,2 linear={:.7f},{:.7f},{:.7f}", name, x, y, value->x, value->y, value->z);
	return value;
}
/// @brief Maximum RGB error avoids hiding a color change in a luminance average.
inline float rgbDelta(Vec3 a, Vec3 b) { return std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z)}); }


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
