/** @file @brief Captures and verifies immutable IQ-00 preservation fixtures without changing the renderer.
 * Explicit --record[-unorm] <provenance-directory> is permitted only before that set's receipts are frozen.
 * Normal CTest runs require all references and compare fresh captures; missing files always fail.
 */
#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;
#include "support/ImageQualityBaseline.hpp"

namespace {
using namespace vve;
namespace fs = std::filesystem;
constexpr unsigned repeats = 3; ///< Independent engine initializations per fixture.

/// @brief Stable decoded-byte receipt, deliberately independent of PNG container metadata/compression.
std::uint64_t rgbaHash(std::span<const unsigned char> pixels) {
	std::uint64_t hash = 14695981039346656037ULL;
	for (const auto byte : pixels) { hash = (hash ^ byte) * 1099511628211ULL; }
	return hash;
}

/// @brief Fails loudly rather than allowing incomplete graphics coverage to count as a pass.
void require(bool condition, std::string_view message) {
	if (!condition) { throw std::runtime_error{std::string{message}}; }
}

/// @brief Decodes exact RGBA dimensions; imagePixels owns the shared stb loading contract.
auto decoded(const fs::path &path, unsigned extent) -> std::vector<unsigned char> {
	int width{}, height{}, channels{};
	require(stbi_info(path.string().c_str(), &width, &height, &channels) != 0 &&
		width == static_cast<int>(extent) && height == static_cast<int>(extent), "missing/unreadable/wrong-sized PNG: " + path.string());
	auto pixels = test::imagePixels(path);
	require(pixels.has_value() && pixels->size() == extent * extent * 4U, "invalid decoded RGBA: " + path.string());
	return std::move(*pixels);
}

/// @brief Measures changed RGB pixels, maximum channel error and mean RGB byte error, preserving alpha exactly.
auto variation(std::span<const unsigned char> a, std::span<const unsigned char> b) {
	require(a.size() == b.size() && !a.empty(), "incompatible comparison buffers");
	std::size_t changed{};
	unsigned maximum{};
	double total{};
	// A changed alpha or extent is never admitted by RGB tolerances.
	for (auto offset : std::views::iota(std::size_t{}, a.size() / 4)) {
		const auto i = offset * 4;
		require(a[i + 3] == b[i + 3], "alpha mismatch");
		bool different{};
		for (auto channel : {0U, 1U, 2U}) {
			const auto error = static_cast<unsigned>(std::abs(int(a[i + channel]) - int(b[i + channel])));
			maximum = std::max(maximum, error); total += error; different |= error != 0;
		}
		changed += different;
	}
	return std::tuple{changed, maximum, total / (a.size() / 4 * 3)};
}

/// @brief Writes decoded RGB differences or the full-frame preservation mask using the existing PNG writer.
void writeDiagnostic(const fs::path &path, std::span<const unsigned char> a,
	std::span<const unsigned char> b, unsigned extent, bool mask = false) {
	std::vector<std::byte> pixels(a.size());
	for (auto i : std::views::iota(std::size_t{}, a.size())) {
		pixels[i] = static_cast<std::byte>(i % 4 == 3 || mask ? 255U : static_cast<unsigned>(std::abs(int(a[i]) - int(b[i]))));
	}
	require(simple::writeReadbackPng(pixels, {extent, extent}, VK_FORMAT_R8G8B8A8_UNORM, path.string()), "diagnostic PNG write failed");
}

/// @brief Adds the unchanged capture-test sun and view; every preservation input is also written in the manifest.
void litView(simple::RenderSystem &render, bool sponza) {
	render.setCamera(Camera::lookAt(Position{Vec3{0, sponza ? 6.F : .25F, sponza ? 9.F : 2.F}},
		Position{Vec3{0, sponza ? 6.F : .25F, 0}}));
	render.addDirectionalLight(Direction{Vec3{-.35F, -.25F, -1}}, LinearColor{Vec3{1, .95F, .85F}},
		LightIntensity{1.5F}, LinearColor{Vec3{.04F, .04F, .04F}});
}

/// @brief Recreates the lighting/Sponza/background/AA preservation scenes from the existing owning tests.
void setup(simple::Engine &engine, std::string_view kind) {
	auto &render = engine.renderSystem();
	if (kind.starts_with("lighting-") || kind.starts_with("sponza-")) {
		const bool sponza = kind.starts_with("sponza-");
		const auto model = fs::path{VVE_IQ_SOURCE_DIR} / (sponza ? "assets/sea_keep_lonely_watcher/scene.gltf" :
			kind == "lighting-flat" ? "tests/assets/render_material_import_flat.obj" : "tests/assets/render_material_import.obj");
		const auto scene = engine.assets().loadScene(model);
		require(scene.has_value(), "fixture asset import failed: " + model.string());
		render.clearScene();
		const auto instance = render.instantiateScene(*scene);
		require(instance.has_value(), "fixture instantiation failed");
		const auto objects = render.sceneInstanceObjects(*instance);
		require(objects && !objects->empty(), "fixture contains no objects");
		if (kind.ends_with("-unlit")) {
			for (const auto object : *objects) { require(bool(render.setObjectUnlit(object, true)), "unlit setup failed"); }
		}
		litView(render, sponza);
		if (kind == "lighting-plane") {
			render.clearScene();
			const auto plane = render.addPlane(Vec2{4, 4}, LinearColor{Vec3{.5F, .25F, 1}});
			require(plane && render.setObjectUnlit(*plane, true), "plane setup failed");
			litView(render, false);
			render.setCamera(Camera::lookAt(Position{Vec3{0, 2, 0}}, Position{}));
		}
	} else if (kind == "background-set") {
		render.setBackgroundColor(LinearColor{Vec3{.2F, .5F, 1}});
	} else if (kind == "aa") {
		render.setBackgroundColor(LinearColor{Vec3{0, 0, 0}});
		render.setCamera(Camera::lookAt(Position{Vec3{0, 0, 4}}, Position{}));
		const auto triangle = render.addMesh(MeshDescriptor{
			.positions = {{-1.25F, -1.1F, 0}, {1.3F, -1, 0}, {.1F, 1.2F, 0}}, .indices = {0, 1, 2}});
		require(triangle && render.setObjectUnlit(*triangle, true), "AA triangle setup failed");
		render.setDirectionalLight(Direction{}, LinearColor{}, LightIntensity{}, LinearColor{});
		render.setPostProcessSetup([](vvppl::PostProcessing &chain) { chain.addColorGrade().gain[0] = .25F; });
		engine.gui().draw([] { ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, {12, 12}, IM_COL32(128, 128, 128, 255)); });
	}
}

/// @brief Records actual device, presentation and camera metadata from the same process that captured the pixels.
void metadata(std::ostream &manifest, simple::ForwardRenderer &renderer, std::string_view id, unsigned repeat, unsigned warmup) {
	auto &target = renderer.targets.front();
	const auto physical = renderer.physicalDevice.physicalDevice;
	VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
	VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
	vkGetPhysicalDeviceProperties2(physical, &properties);
	unsigned count{};
	require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, target.surface.surface, &count, nullptr) == VK_SUCCESS, "surface formats unavailable");
	std::vector<VkSurfaceFormatKHR> formats(count);
	require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, target.surface.surface, &count, formats.data()) == VK_SUCCESS, "surface query failed");
	const auto selected = std::ranges::find(formats, target.swapchain.imageFormat, &VkSurfaceFormatKHR::format);
	require(selected != formats.end(), "swapchain format absent from surface query");
	const auto chosen = *selected;
	// The current swapchain stores only its format. Infer colour space only when every matching offer agrees.
	require(std::ranges::all_of(formats, [&](const auto &format) {
		return format.format != chosen.format || format.colorSpace == chosen.colorSpace;
	}), "ambiguous surface colour space; cannot record reliable provenance");
	VkBool32 present{};
	require(vkGetPhysicalDeviceSurfaceSupportKHR(physical, target.presentQueueFamily, target.surface.surface, &present) == VK_SUCCESS && present,
		"selected device cannot present");
	const auto &camera = renderer.camera();
	const auto p = camera.position.value, d = camera.forward.value;
	manifest << std::format("fixture={} repeat={} device={} device_type={} driver={} driver_info={} driver_version={} api={}\n",
		id, repeat, std::string_view{properties.properties.deviceName}, int(properties.properties.deviceType),
		std::string_view{driver.driverName}, std::string_view{driver.driverInfo},
		properties.properties.driverVersion, properties.properties.apiVersion);
	manifest << std::format("format={} color_space={} gui_format={} mutable_views={} extent={}x{} samples={} present_support={} SDL_backend={}\n",
		int(chosen.format), int(chosen.colorSpace), int(target.swapchain.guiFormat), !target.imageViews.ownedGuiViews.empty(),
		target.swapchain.extent.width, target.swapchain.extent.height, unsigned(renderer.sampleCount()), present, SDL_GetCurrentVideoDriver());
	manifest << std::format("camera.position={},{},{} camera.forward={},{},{} camera.fov_y={} camera.clip={},{} seed=none animation=none weather=none\n",
		p.x, p.y, p.z, d.x, d.y, d.z, camera.fov_y.radians, camera.clip.near_plane, camera.clip.far_plane);
	manifest << std::format("warmup_frames={} capture_frame={} GUI_visible={} GI_reset=fresh_engine GI_sequence=fixed_renderFrame validation_active={} validation_errors={}\n",
		warmup, warmup + 1, id.starts_with("aa-"), renderer.validationActive(), renderer.validationErrorCount());
	for (const auto &format : formats) { manifest << std::format("available_surface_format={} color_space={}\n", int(format.format), int(format.colorSpace)); }
	manifest << "UNORM_fallback=" << (chosen.format == VK_FORMAT_R8G8B8A8_UNORM || chosen.format == VK_FORMAT_B8G8R8A8_UNORM ?
		"captured" : "unavailable_on_selected_surface_no_production_force_option") << '\n';
}

/// @brief Captures one independently initialized scene; checks real validation including teardown.
auto capture(const fs::path &output, std::ostream &manifest, std::string_view id, std::string_view kind,
	RendererBackend backend, Antialiasing aa, unsigned repeat) {
	const unsigned extent = kind.starts_with("sponza-") ? 256 : kind.starts_with("background-") ? 64 : 128;
	auto engine = test::hiddenEngine(std::string{id}, PixelExtent{extent, extent});
	auto &render = engine.renderSystem();
	require(bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(aa)) && bool(engine.init()), "engine initialization failed");
	// Repeat zero retains the untouched default. Other repeats explicitly select the resource-free output default.
	if (repeat != 0) { require(bool(render.setToneMapping(ToneMapping{})), "explicit default tone mapping failed"); }
	manifest << std::format("explicit_default={} antialiasing={}\n", repeat != 0, int(aa));
	setup(engine, kind);
	const unsigned warmup = backend == RendererBackend::hybrid_gi ? 64 : 8;
	require(test::renderFrames(engine, warmup), "warm-up rendering failed");
	auto &renderer = render.forward();
	if (id.ends_with("-unorm")) {
		const auto format = renderer.targets.front().swapchain.imageFormat;
		require(format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM, "legacy UNORM forcing did not take effect");
	}
	require(renderer.validationActive(), "Debug Khronos validation messenger must be active");
	require(std::ranges::any_of(renderer.instance.extensions, [](const char *extension) {
		return std::string_view{extension} == VK_EXT_LAYER_SETTINGS_EXTENSION_NAME || std::string_view{extension} == VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
	}), "synchronization validation configuration extension unavailable");
	const auto path = output / std::format("{}-{}.png", id, repeat);
	require(bool(render.captureFrameToPng(path)), "PNG capture failed");
	metadata(manifest, renderer, id, repeat, warmup);
	auto pixels = decoded(path, extent);
	manifest << std::format("png={} decoded_rgba_fnv1a64={:016x}\n", path.filename().string(), rgbaHash(pixels));
	render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
	require(renderer.validationErrorCount() == 0, "validation errors during capture/teardown");
	return pixels;
}

/// @brief Freezes pairwise repeat maxima once, or enforces checked-in limits against each immutable reference.
void fixture(const fs::path &baseline, const fs::path &output, std::ostream &manifest, std::ostream &header,
	std::string_view id, std::string_view kind, RendererBackend backend, Antialiasing aa, bool record) {
	std::array<std::vector<unsigned char>, repeats> images;
	std::array<std::uint64_t, repeats> hashes{};
	const unsigned extent = kind.starts_with("sponza-") ? 256 : kind.starts_with("background-") ? 64 : 128;
	std::span<const test::ImageQualityReceipt> limits = test::imageQualityLimits;
	if (id.ends_with("-unorm")) { limits = test::imageQualityUnormLimits; }
	const auto receipt = std::ranges::find_if(limits, [&](const auto &row) { return std::get<0>(row) == id; });
	if (!record) {
		require(receipt != limits.end(), "fixture has no frozen receipt");
		const auto mask = decoded(baseline / std::format("{}-preservation-mask.png", id), extent);
		require(std::ranges::all_of(mask, [](auto byte) { return byte == 255; }), "full-frame preservation mask changed");
	}
	// Each repeat starts from a fresh engine, avoiding candidate-dependent GI convergence or animation state.
	for (const auto repeat : std::views::iota(0U, repeats)) {
		images[repeat] = capture(output, manifest, id, kind, backend, aa, repeat);
		hashes[repeat] = rgbaHash(images[repeat]);
		if (!record) {
			for (const auto reference : std::views::iota(0U, repeats)) {
				const auto pixels = decoded(baseline / std::format("{}-{}.png", id, reference), extent);
				require(rgbaHash(pixels) == std::get<1>(*receipt)[reference], "reference hash differs from frozen receipt");
				const auto [changed, maximum, mean] = variation(images[repeat], pixels);
				std::println("fixture={} repeat={} reference={} changed={} fraction={} max={} mean={}", id, repeat, reference,
					changed, double(changed) / (extent * extent), maximum, mean);
				writeDiagnostic(output / std::format("{}-{}-diff-{}.png", id, repeat, reference), images[repeat], pixels, extent);
				require(changed <= std::get<2>(*receipt) && maximum <= std::get<3>(*receipt) && mean <= std::get<4>(*receipt), "preservation comparison failed");
			}
		}
	}
	std::size_t changedLimit{};
	if (!record) {
		require(images[0] == images[1] && images[0] == images[2], "default vs explicit-default API equality");
		std::println("fixture={} default_vs_explicit_default=equal (separate from immutable-reference comparisons)", id);
	}
	unsigned maxLimit{};
	double meanLimit{};
	// Freeze only measured pairwise variation, without an extra safety margin or a global image average allowance.
	for (auto a : std::views::iota(0U, repeats)) {
		for (auto b : std::views::iota(a + 1, repeats)) {
			const auto [changed, maximum, mean] = variation(images[a], images[b]);
			changedLimit = std::max(changedLimit, changed); maxLimit = std::max(maxLimit, maximum); meanLimit = std::max(meanLimit, mean);
			manifest << std::format("variation.fixture={} pair={},{} changed_rgb_pixels={} fraction={} max_rgb_byte_error={} mean_rgb_byte_error={:.17g}\n",
				id, a, b, changed, double(changed) / (extent * extent), maximum, mean);
			if (record) { writeDiagnostic(output / std::format("{}-repeat-diff-{}-{}.png", id, a, b), images[a], images[b], extent); }
		}
	}
	if (record) {
		writeDiagnostic(output / std::format("{}-preservation-mask.png", id), images[0], images[0], extent, true);
		header << std::format("\t{{\"{}\", {{{}ULL, {}ULL, {}ULL}}, {}, {}, {:.17g}}},\n", id,
			hashes[0], hashes[1], hashes[2], changedLimit, maxLimit, meanLimit);
		manifest << std::format("frozen.fixture={} changed_limit={} max_limit={} mean_limit={:.17g} mask=full_frame samples={} purpose=preservation_only\n",
			id, changedLimit, maxLimit, meanLimit, extent * extent);
	}
	// A deliberate colour defect must fail even where repeat variance is nonzero; exact fixtures reject one byte.
	auto defect = images[0];
	if (maxLimit == 0) { defect[0] ^= 1; }
	else { for (auto i : std::views::iota(std::size_t{}, defect.size())) { if (i % 4 != 3) { defect[i] ^= 255; } } }
	const auto [badChanged, badMax, badMean] = variation(images[0], defect);
	require(badChanged > changedLimit || badMax > maxLimit || badMean > meanLimit, "negative control was accepted");
	std::println("fixture={} repeats=3 negative_control=rejected repeat_max={} repeat_mean={}", id, maxLimit, meanLimit);
}
} // namespace

/// @brief Runs the complete preservation matrix; recording cannot overwrite or silently create missing references in CTest.
int main(int argc, char **argv) try {
	const std::string_view mode = argc > 1 ? argv[1] : "";
	const bool unorm = mode == "--unorm" || mode == "--record-unorm";
	const bool record = argc == 3 && (mode == "--record" || mode == "--record-unorm");
	require(argc == 1 || (argc == 2 && mode == "--unorm") || record,
		"usage: RenderImageQualityBaselineTests [--unorm | --record[-unorm] provenance-directory]");
	const auto baseline = unorm ? fs::path{VVE_IQ_BASELINE_DIR} / "legacy-unorm" : fs::path{VVE_IQ_BASELINE_DIR};
	const auto output = record ? baseline : fs::path{VVE_IQ_BASELINE_DIR}.parent_path() / (unorm ? "current-unorm" : "current");
	std::span<const test::ImageQualityReceipt> limits = test::imageQualityLimits;
	if (unorm) { limits = test::imageQualityUnormLimits; }
	if (record) {
		require(limits.empty(), "receipts are frozen; recording candidate references is forbidden");
		require(!fs::exists(baseline), "baseline already exists; overwriting references is forbidden");
		require(fs::is_regular_file(fs::path{argv[2]} / "provenance.txt"), "recording requires pre-task provenance");
		fs::create_directories(baseline.parent_path());
		fs::copy(argv[2], baseline, fs::copy_options::recursive);
	} else {
		require(!limits.empty() && fs::is_regular_file(baseline / "manifest.txt"), "immutable baseline/receipts missing");
		std::ifstream referenceManifest{baseline / "manifest.txt"};
		const std::string contents{std::istreambuf_iterator<char>{referenceManifest}, {}};
		require(contents.contains(unorm ? "completed_fixtures=4 completed_captures=12 skipped=0" :
			"completed_fixtures=22 completed_captures=66 skipped=0"), "baseline recording is incomplete");
		fs::create_directories(output);
	}
	std::ofstream file;
	if (record) { file.open(baseline / "manifest.txt"); }
	std::ostream &manifest = record ? file : std::cout;
	std::ofstream header;
	if (record) { header.open(baseline / "frozen-entries.txt"); }
	manifest << "IQ-00 schema=1 decoded_hash=FNV-1a64 comparison=RGB_byte_errors alpha=exact extent=exact\n";
	manifest << "reference_revision=" << test::baselineRevision << " compiler=" << VVE_IQ_COMPILER << '\n';
	if (record) { std::ifstream provenance{baseline / "provenance.txt"}; manifest << provenance.rdbuf(); }
	for (const auto name : {"DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE", "SDL_VIDEODRIVER", "SDL_VIDEO_DRIVER", "VK_INSTANCE_LAYERS", "VK_LAYER_VALIDATE_SYNC", "VK_ADD_LAYER_PATH",
		"VK_KHRONOS_PROFILES_EXCLUDE_FORMATS", "VK_KHRONOS_PROFILES_EMULATE_PORTABILITY"}) {
		manifest << name << '=' << (std::getenv(name) ? std::getenv(name) : "<unset>") << '\n';
	}
	// Real layer capability filtering selects the unchanged production UNORM fallback; references stay separate.
	if (unorm) {
		for (const auto kind : {"background-set", "lighting-lit", "sponza-lit", "aa"}) {
			fixture(baseline, output, manifest, header, std::format("{}-forward-unorm", kind), kind, RendererBackend::forward, Antialiasing::none, record);
		}
		manifest << "completed_fixtures=4 completed_captures=12 skipped=0\n";
		return 0;
	}
	constexpr std::array backends{RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi};
	constexpr std::array names{"forward", "deferred", "hybrid_gi"};
	// The existing material/background tests cover forward/deferred; AA additionally freezes hybrid_gi.
	for (auto index : std::views::iota(0U, 3U)) {
		if (index < 2) {
			for (const auto kind : {"lighting-lit", "lighting-unlit", "lighting-flat", "lighting-plane", "sponza-lit", "sponza-unlit", "background-default", "background-set"}) {
				fixture(baseline, output, manifest, header, std::format("{}-{}", kind, names[index]), kind, backends[index], Antialiasing::none, record);
			}
		}
		for (const auto aa : {Antialiasing::none, Antialiasing::msaa4}) {
			fixture(baseline, output, manifest, header, std::format("aa-{}-{}", names[index], aa == Antialiasing::none ? "none" : "msaa4"),
				"aa", backends[index], aa, record);
		}
	}
	manifest << "completed_fixtures=22 completed_captures=66 skipped=0\n";
	return 0;
} catch (const std::exception &error) {
	std::println(stderr, "[RenderImageQualityBaselineTests] FAILED: {}", error.what());
	return 1;
}
