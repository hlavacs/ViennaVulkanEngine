/** @file @brief Active SMAA gates against immutable independent references, plus shared failing controls. */
#include <SDL3/SDL_video.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <vulkan/vulkan.h>
#include "../third_party/SMAA/AreaTex.h"
#include "../third_party/SMAA/SearchTex.h"
#include "fixtureVertex.h"
#include "fixtureForward.h"
#include "fixtureGbuffer.h"
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {
using namespace vve;
const std::filesystem::path output{VVE_SMAA_QUALITY_DIR}; ///< Evidence remains below the configured build directory.
const auto frozenReferences = std::filesystem::path{VVE_SMAA_HISTORICAL_DIR} / "smaa" / "frozen"; ///< Shipped sealed fixtures, independent of generated evidence.
/// @brief Missing graphics/data, validation or failed controls are hard failures.
void require(bool condition, std::string_view reason) {
	if (!condition) { throw std::runtime_error{std::string{reason}}; }
}
/// @brief Propagate native Vulkan failures through the callback contract.
void requireVk(VkResult result) {
	if (result != VK_SUCCESS) { throw Error::platform_error; }
}
#include "support/SmaaQualityReference.hpp"
#include "support/SmaaNativeFixture.hpp"
using Images = std::array<std::vector<unsigned char>, names.size() * offsets.size()>; ///< One capture per fixture/subpixel phase.
using References = std::array<std::vector<double>, names.size() * offsets.size()>; ///< Independent linear coverage grids.

/// @brief Read immutable receipts; missing provenance is an error, never permission to recreate history.
auto receiptText(const std::filesystem::path &path) {
	std::ifstream file{path}; require(bool(file), std::format("missing receipt: {}", path.string()));
	std::ostringstream text; text << file.rdbuf(); return text.str();
}
/// @brief Extract the first frozen receipt field, preserving the complete device/driver spelling.
auto receiptField(const std::string &receipt, std::string_view first, std::string_view last) {
	const auto begin = receipt.find(first); require(begin != std::string::npos, "missing provenance field");
	const auto start = begin + first.size(), end = receipt.find(last, start);
	require(end != std::string::npos, "unterminated provenance field");
	return receipt.substr(start, end - start);
}
/// @brief Exact equality is applicable only to identical GPU, driver, API, surface and capture provenance.
bool matchingProvenance(const std::vector<std::string> &actual, const std::vector<std::string> &frozen) {
	return !frozen.empty() && actual == frozen;
}
/// @brief Compare with the sealed Linux receipt and freeze log; no cross-device historical pixel claims.
bool historicalBaseline(simple::ForwardRenderer &gpu) {
	const std::filesystem::path historical{VVE_SMAA_HISTORICAL_DIR};
	const auto receipt = receiptText(historical / "baseline" / "manifest.txt");
	const auto freezeLog = receiptText(historical / "smaa" / "freeze.log");
	const std::vector<std::string> frozen{
		receiptField(receipt, " repeat=0 device=", " device_type="), receiptField(receipt, " driver=", " driver_info="),
		receiptField(receipt, " driver_info=", " driver_version="), receiptField(receipt, " driver_version=", " api="),
		receiptField(receipt, " api=", "\n"), receiptField(receipt, "\nformat=", " color_space="),
		receiptField(receipt, " color_space=", " gui_format="), receiptField(receipt, " SDL_backend=", "\n"),
		receiptField(receipt, " mutable_views=", " extent="), "128x128/native_unlit/no_GUI/3_frames_then_PNG/standard_samples"};
	require(freezeLog.contains("[vve::simple] device: " + frozen[0]) && freezeLog.contains("IQ-03a grid=16 support=24"), "freeze capture provenance");
	VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
	VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
	vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice, &properties);
	const auto &target = gpu.targets.front();
	const std::vector<std::string> actual{properties.properties.deviceName, driver.driverName, driver.driverInfo,
		std::to_string(properties.properties.driverVersion), std::to_string(properties.properties.apiVersion),
		std::to_string(int(target.swapchain.imageFormat)), std::to_string(int(target.swapchain.colorSpace)), SDL_GetCurrentVideoDriver(),
		gpu.device.swapchainMutableFormat ? "true" : "false", "128x128/native_unlit/no_GUI/3_frames_then_PNG/standard_samples"};
	const bool applies = matchingProvenance(actual, frozen);
	std::println("historical.llvmpipe={} actual.provenance={} frozen.provenance={} comparison={}", applies ? "applicable" : "inapplicable",
		actual, frozen, applies ? "exact_RGBA+hash+metrics" : "provenance_mismatch;analytic_standard_sample_coverage+transfer_quantization");
	return applies;
}

/// @brief Verify measure and the ratio/regression gates using a hand-counted square and altered boundary pixels.
void metricChecks() {
	const std::vector<std::string> provenance{"GPU", "driver", "format", "capture"};
	require(matchingProvenance(provenance, provenance) && !matchingProvenance({}, {}), "provenance equality");
	for (unsigned field : std::views::iota(0U, unsigned(provenance.size()))) {
		auto changed = provenance; changed[field] += "-different";
		require(!matchingProvenance(changed, provenance), "mismatched provenance accepted");
	}
	std::vector<double> ref(side * side);
	std::vector<unsigned char> rgba(side * side * 4, 255);
	// A 64x64 white square has area 4096. Its radius-2 boundary band is a 70x70 outer square
	// minus four corner pixels and a 58x58 inner square: 4900-4-3364=1532 samples.
	for (unsigned y : std::views::iota(0U, side)) {
		for (unsigned x : std::views::iota(0U, side)) {
			const bool inside = x >= 32 && x < 96 && y >= 32 && y < 96;
			ref[y * side + x] = inside;
			for (unsigned c : {0U, 1U, 2U}) { rgba[(y * side + x) * 4 + c] = inside ? 255 : 0; }
		}
	}
	require(std::ranges::count(mask(ref, edgeRadius), true) == 1532, "hand-counted geometry band");
	const auto set = [&](unsigned x, unsigned char value, unsigned y = 50) {
		for (unsigned c : {0U, 1U, 2U}) { rgba[(y * side + x) * 4 + c] = value; }
	};
	// The 128-byte sRGB midpoint decodes to this independently calculated IEC 61966-2-1 value, not 128/255.
	constexpr double grey = .21586050011389926;
	require(std::abs(decode(128) - grey) < 1e-14 && encode(.5) == 188, "independent transfer vectors");
	set(32, 0);
	const auto none = measure(rgba, ref);
	set(33, 128);
	const auto filtered = measure(rgba, ref);
	set(33, 0);
	const auto combined = measure(rgba, ref);
	const auto check = [](const auto &metrics, double missing) {
		require(std::abs(metrics[0] - missing / 1532) < 1e-14 &&
			std::abs(metrics[1] - missing / 4096) < 1e-14 && metrics[2] == 1, "hand-computed linear MAE/coverage/contrast");
	};
	check(none, 1);
	check(filtered, 2 - grey);
	check(combined, 2);
	const double ratio = filtered[0] / none[0], regression = combined[0] - filtered[0];
	require(std::abs(ratio - (2 - grey)) < 1e-14 && std::abs(regression - grey / 1532) < 1e-14, "same-pixel thin ratio/combined regression");
	require(!evaluateSmaaEffect(filtered[0], none[0], 1) && evaluateRegressionVsMsaa(combined[0], filtered[0]), "synthetic metric verdicts");
	// Eight further missing edge pixels exceed 1/256 absolute regression, without touching any frozen limit.
	for (unsigned y : std::views::iota(52U, 60U)) { set(32, 0, y); }
	require(!evaluateRegressionVsMsaa(measure(rgba, ref)[0], combined[0]), "synthetic excess-regression control");
	std::println("cpu.metrics.edge.samples=1532 area=4096 none={} filtered={} combined={} ratio={} regression={} PASS",
		none, filtered, combined, ratio, regression);
}

/// @brief Load complete RGBA display data; missing files never become replacement references.
auto pixels(const std::filesystem::path &path, unsigned width = side, unsigned height = side) {
	int w = 0, h = 0, channels = 0;
	auto *data = stbi_load(path.c_str(), &w, &h, &channels, 4);
	require(data != nullptr, std::format("missing capture: {}", path.string()));
	std::vector<unsigned char> result(data, data + w * h * 4);
	stbi_image_free(data);
	require(w == width && h == height, "wrong capture extent");
	return result;
}
/// @brief Persist CPU masks/references or compare their decoded contents with the sealed freeze.
void verifyImage(const std::filesystem::path &path, const std::vector<unsigned char> &rgb, bool freeze) {
	if (freeze) { require(stbi_write_png(path.c_str(), side, side, 3, rgb.data(), side * 3), "reference/mask write"); }
	const auto saved = pixels(path);
	for (auto i : std::views::iota(0U, side * side)) {
		for (auto c : std::views::iota(0U, 3U)) { require(saved[i * 4 + c] == rgb[i * 3 + c], "CPU reference/mask changed"); }
		require(saved[i * 4 + 3] == 255, "reference alpha changed");
	}
}
/// @brief Verify fixed area vectors and geometry-only masks before any graphics measurements.
auto prepareReferences(bool freeze) {
	References references;
	const auto directory = freeze ? output / "frozen" : frozenReferences;
	for (auto f : std::views::iota(0U, unsigned(names.size()))) {
		for (auto o : std::views::iota(0U, unsigned(offsets.size()))) {
			auto &ref = references[f * offsets.size() + o];
			ref = reference(f, offsets[o]);
			const auto support = mask(ref, filterSupport), edges = mask(ref, edgeRadius);
			require(std::ranges::count(edges, true) > 200 && std::ranges::count(support, false) > 100, "empty mask region");
			require(std::ranges::all_of(ref, [](double v) { return v >= 0 && v <= 1; }), "invalid CPU coverage");
			std::vector<unsigned char> rgb(side * side * 3), region(rgb.size());
			for (auto i : std::views::iota(0U, side * side)) {
				for (auto c : std::views::iota(0U, 3U)) {
					rgb[i * 3 + c] = encode(ref[i]);
					region[i * 3 + c] = edges[i] ? (c == 0 ? 255 : 0) : support[i] ? (c == 1 ? 128 : 0) : 0;
				}
				require(!edges[i] || support[i], "edge outside filter support");
			}
			verifyImage(directory / std::format("reference-f{}-o{}.png", f, o), rgb, freeze);
			verifyImage(directory / std::format("mask-f{}-o{}.png", f, o), region, freeze);
			std::println("reference={} offset={} area={} edge.samples={} outside.samples={}", names[f], o,
				std::accumulate(ref.begin(), ref.end(), 0.0), std::ranges::count(edges, true), std::ranges::count(support, false));
		}
	}
	// Independently calculated polygon areas: 3*(2*72), shoelace tilted quad and diagonal parallelogram.
	require(std::abs(std::accumulate(references[4].begin(), references[4].end(), 0.0) - 432) < 1e-9, "fixed thin area vector");
	for (const auto [fixture, area] : {std::pair{0U, 4441.0}, std::pair{3U, 795.0}}) {
		const auto &ref = references[fixture * offsets.size()];
		require(std::abs(std::accumulate(ref.begin(), ref.end(), 0.0) - area) < 1, "fixed polygon area vector");
	}
	const Triangle fixed{{{0, 0}, {2, 0}, {0, 2}}};
	require(contains(fixed, {.5, .5}) && !contains(fixed, {1.5, 1.5}), "half-plane vectors");
	return references;
}
/// @brief Assemble candidate, CPU reference, amplified linear error and geometry-only masks for inspection.
auto sheet(const std::vector<unsigned char> &rgba, const std::vector<double> &ref) {
	const auto edges = mask(ref, edgeRadius), affected = mask(ref, filterSupport);
	std::vector<unsigned char> rgb(side * side * 4 * 3);
	for (auto i : std::views::iota(0U, side * side)) {
		const auto p = (i / side * side * 4 + i % side) * 3;
		for (auto c : std::views::iota(0U, 3U)) {
			rgb[p + c] = rgba[i * 4 + c];
			rgb[p + side * 3 + c] = encode(ref[i]);
			rgb[p + side * 6 + c] = std::lround(255 * std::min(1.0, 8 * std::abs(decode(rgba[i * 4 + c]) - ref[i])));
			rgb[p + side * 9 + c] = edges[i] ? (c == 0 ? 255 : 0) : affected[i] ? (c == 1 ? 128 : 0) : 0;
		}
	}
	return rgb;
}
/// @brief Save compact sheets containing every capture, ordered by fixture row and offset column.
void inspectionSheet(const Images &images, const References &references, const std::filesystem::path &path) {
	constexpr unsigned width = side * 4 * offsets.size(), height = side * names.size();
	std::vector<unsigned char> contact(width * height * 3);
	for (auto i : std::views::iota(0U, unsigned(images.size()))) {
		const auto rgb = sheet(images[i], references[i]);
		for (auto y : std::views::iota(0U, side)) {
			const auto destination = ((i / offsets.size() * side + y) * width + i % offsets.size() * side * 4) * 3;
			std::copy_n(rgb.begin() + y * side * 12, side * 12, contact.begin() + destination);
		}
	}
	require(stbi_write_png(path.c_str(), width, height, 3, contact.data(), width * 3), "inspection sheet write");
}
/// @brief Exact clearing contract used by real readbacks and stale-data controls.
bool stageCleared(std::span<const std::byte> bytes) {
	return !bytes.empty() && std::ranges::all_of(bytes, [](auto value) { return value == std::byte{}; });
}
/// @brief Save raw edge/weight data as viewable RGBA; host access follows an observed submission completion.
void intermediateSheet(simple::WindowTarget &target, const std::filesystem::path &path, bool flat, bool checkOccupancy = true) {
	std::vector<unsigned char> rgb(side * side * 6);
	for (auto stage : std::views::iota(0U, 2U)) {
		const auto bytes = target.smaaReadback[stage].bytes();
		require(bytes.size() == side * side * (stage == 0 ? 2 : 4), "intermediate extent");
		std::ofstream raw{path.string() + std::format(".stage{}.raw", stage), std::ios::binary};
		raw.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
		require(bool(raw), "raw intermediate write"); // Retain all weight channels, including alpha, for independent inspection.
		const auto nonzero = std::ranges::count_if(bytes, [](std::byte value) { return value != std::byte{}; });
		if (checkOccupancy) { require(flat ? stageCleared(bytes) : nonzero > 100, "stale or missing edge/weight data"); }
		std::println("intermediate={} flat={} nonzero={}", stage, flat, nonzero);
		for (auto i : std::views::iota(0U, side * side)) {
			for (auto c : std::views::iota(0U, 3U)) {
				rgb[(i / side * side * 2 + stage * side + i % side) * 3 + c] =
					std::to_integer<unsigned char>(bytes[i * (stage == 0 ? 2 : 4) + (stage == 0 ? c % 2 : c)]);
			}
		}
	}
	require(stbi_write_png(path.c_str(), side * 2, side, 3, rgb.data(), side * 6), "intermediate sheet write");
}
#include "support/SmaaStageOracle.hpp"
/// @brief Evaluate conformance separately from intrinsic HIGH-filter limitations, using CPU-only inputs/tables.
bool evaluateFilteredMode(const Images &candidate, const Images &base, const References &references, bool combined, bool fixed8) {
	std::array<double, names.size()> candidateErrors{}, baseErrors{}, referenceErrors{}, referenceBaseErrors{}, precision{};
	bool accepted = true;
	for (auto i : std::views::iota(0U, unsigned(candidate.size()))) {
		const auto fixture = i / offsets.size();
		const auto input = stageInput(triangles(fixture), offsets[i % offsets.size()], fixture == residualFixture, combined ? 4 : 1);
		const auto weights = stageWeights(stageEdges(input), fixed8), expected = stageBlend(input, weights);
		const auto [low, high] = stageOutputBounds(input, weights, fixed8);
		const auto metrics = measure(candidate[i], references[i]), referenceMetrics = measure(expected, references[i]);
		const auto edges = mask(references[i], edgeRadius);
		double massBound = 0, edgeBound = 0;
		for (auto p : std::views::iota(0U, side * side)) {
			const double bound = std::max(decode(expected[p * 4]) - decode(low[p * 4]), decode(high[p * 4]) - decode(expected[p * 4]));
			massBound += bound; if (edges[p]) { edgeBound += bound; }
		}
		const double area = std::accumulate(references[i].begin(), references[i].end(), 0.0);
		edgeBound /= std::ranges::count(edges, true);
		const bool conformance = stageWithin(candidate[i], low, high);
		const bool detail = metrics[1] <= std::max(maxAreaRelative, referenceMetrics[1]) + massBound / area &&
			(fixture != 1 || metrics[2] >= minThinContrast);
		const bool outside = evaluateOutsideMask(candidate[i], base[i], mask(references[i], filterSupport));
		accepted = conformance && detail && outside && accepted;
		std::println("filtered.fixture={} offset={} conformance={} area.error={} reference.area.error={} precision.area={} contrast={} detail={} outside={}",
			names[fixture], i % offsets.size(), conformance, metrics[1], referenceMetrics[1], massBound / area, metrics[2], detail, outside);
		candidateErrors[fixture] += metrics[0] / offsets.size();
		baseErrors[fixture] += measure(base[i], references[i])[0] / offsets.size();
		referenceErrors[fixture] += referenceMetrics[0] / offsets.size();
		referenceBaseErrors[fixture] += measure(stageRgba(input), references[i])[0] / offsets.size();
		precision[fixture] += edgeBound / offsets.size();
	}
	for (auto fixture : std::views::iota(0U, unsigned(names.size()))) {
		// Only the resolved thin bars fail the reference's original standalone improvement rule.
		const bool referenceImproves = evaluateSmaaEffect(referenceErrors[fixture], referenceBaseErrors[fixture], fixture);
		require(referenceImproves || fixture == 1 || combined, "unexpected reference improvement classification");
		const bool effect = combined || !referenceImproves || evaluateSmaaEffect(candidateErrors[fixture], baseErrors[fixture], fixture);
		// Bound additional error beyond CPU HIGH itself, rather than treating its smoothing of MSAA as an engine defect.
		const bool regression = candidateErrors[fixture] <= referenceErrors[fixture] + precision[fixture];
		std::println("filtered.combined={} fixture={} candidate.mae={} base.mae={} reference.mae={} reference.base.mae={} precision.edge={} ratio={} regression={} reference.regression={} improvement.required={} accepted={}",
			combined, names[fixture], candidateErrors[fixture], baseErrors[fixture], referenceErrors[fixture], referenceBaseErrors[fixture], precision[fixture],
			candidateErrors[fixture] / baseErrors[fixture], candidateErrors[fixture] - baseErrors[fixture], referenceErrors[fixture] - referenceBaseErrors[fixture], !combined && referenceImproves, effect && regression);
		accepted = effect && regression && accepted;
	}
	return (!combined || evaluateCombinedResidual(candidateErrors[residualFixture], baseErrors[residualFixture])) && accepted;
}
/// @brief Correct CPU reference and deliberate faults all traverse the same filtered-mode/stage/clearing gates as captures.
void negativeControls(const Images &none, const Images &msaa, const References &references,
	const std::filesystem::path &directory, bool fixed8) {
	std::filesystem::create_directories(directory);
	std::array<Images, 2> correct;
	for (unsigned combined : {0U, 1U}) {
		for (unsigned i : std::views::iota(0U, unsigned(references.size()))) {
			const unsigned fixture = i / offsets.size();
			const auto input = stageInput(triangles(fixture), offsets[i % offsets.size()], fixture == residualFixture, combined ? 4 : 1);
			const auto edges = stageEdges(input), weights = stageWeights(edges, fixed8);
			correct[combined][i] = stageBlend(input, weights);
		}
		require(evaluateFilteredMode(correct[combined], combined ? msaa : none, references, combined, fixed8), "correct CPU reference rejected");
	}
	require(!evaluateFilteredMode(none, none, references, false, fixed8), "bypass accepted as SMAA");
	require(!evaluateFilteredMode(msaa, msaa, references, true, fixed8), "no-op accepted as combined");
	auto blurred = correct[0], damaged = correct[0], alpha = correct[0], regressed = correct[1];
	for (auto o : std::views::iota(0U, unsigned(offsets.size()))) {
		const auto i = offsets.size() + o;
		blurred[i] = boxBlur(correct[0][i]);
		require(measure(blurred[i], references[i])[2] < minThinContrast, "blur must lose thin-feature contrast");
	}
	const auto support = mask(references[0], filterSupport);
	const auto outside = std::ranges::find(support, false) - support.begin();
	damaged[0][outside * 4] += 2;
	alpha[0][3] = 0;
	// Remove a resolved feature in every phase: a real additional edge/area regression, beyond reference smoothing.
	for (unsigned i : std::views::iota(0U, unsigned(offsets.size()))) {
		for (unsigned y : std::views::iota(28U, 100U)) {
			for (unsigned x : std::views::iota(39U, 44U)) {
				for (unsigned c : {0U, 1U, 2U}) { regressed[offsets.size() + i][(y * side + x) * 4 + c] = 0; }
			}
		}
	}
	require(measure(regressed[offsets.size()], references[offsets.size()])[0] >
		measure(correct[1][offsets.size()], references[offsets.size()])[0] + 2 * maxCombinedRegression, "extra regression control too small");
	require(!evaluateFilteredMode(blurred, none, references, false, fixed8), "blur accepted");
	require(!evaluateFilteredMode(damaged, none, references, false, fixed8), "outside-mask change accepted");
	require(!evaluateFilteredMode(alpha, none, references, false, fixed8), "missing alpha accepted");
	require(!evaluateFilteredMode(regressed, msaa, references, true, fixed8), "extra regression accepted");
	const auto input = stageInput(triangles(0), {}, false, 1);
	const auto edges = stageEdges(input), weights = stageWeights(edges, fixed8);
	for (unsigned stage : {0U, 1U}) {
		const auto &expected = stage == 0 ? edges : weights;
		require(stageCompare(std::as_bytes(std::span{expected}), expected, stage == 0 ? 2 : 4, "correct-control", stage), "correct stage rejected");
		auto corrupt = expected; corrupt[side * 64] ^= 255;
		require(!stageCompare(std::as_bytes(std::span{corrupt}), expected, stage == 0 ? 2 : 4, "corrupt-control", stage), "corrupt stage accepted");
		std::vector<std::byte> flat(expected.size());
		require(stageCleared(flat), "zero attachment rejected"); flat[0] = std::byte{1};
		require(!stageCleared(flat), "stale patterned-to-flat data accepted");
	}
	inspectionSheet(blurred, references, directory / "negative-blur.png");
	inspectionSheet(damaged, references, directory / "negative-outside.png");
	inspectionSheet(regressed, references, directory / "negative-extra-regression.png");
	std::println("negative.correct_reference=accepted bypass=no-op=blur=edges=weights=outside=stale=alpha=extra_regression=rejected");
}
/// @brief A real pattern followed immediately by flat output must leave both cleared reference attachments empty.
void staleData(simple::Engine &engine, NativeFixture &native, const std::filesystem::path &directory, unsigned backend, unsigned mode) {
	auto &render = engine.renderSystem(); auto &gpu = render.forward();
	gpu.debugSmaaReadback = true;
	require(bool(engine.renderFrame()), "pattern frame");
	render.waitIdle();
	intermediateSheet(gpu.targets.front(), directory / std::format("pattern-b{}-m{}.png", backend, mode), false);
	native.triangles.clear();
	render.notifyNativeDrawChanged();
	gpu.debugOutputReadback = true;
	require(bool(engine.renderFrame()), "immediately following flat frame");
	render.waitIdle();
	intermediateSheet(gpu.targets.front(), directory / std::format("flat-intermediates-b{}-m{}.png", backend, mode), true);
	require(gpu.outputDiagnostics.size() == 1, "flat frame receipt");
	const auto path = directory / std::format("flat-output-b{}-m{}.png", backend, mode);
	require(bool(gpu.writeOutputDiagnostic(gpu.outputDiagnostics.front(), path)), "flat frame output");
	const auto rgba = pixels(path);
	const auto flat = stageRgba(std::vector<unsigned char>(side * side));
	require(stageWithin(rgba, flat, flat), "stale output");
	gpu.debugOutputReadback = gpu.debugSmaaReadback = false;
	gpu.outputDiagnostics.clear();
}

/// @brief Exercise engine-owned geometry against the same oracle; pixel-frequency cutout remains the native shader fixture.
bool engineGeometry(RendererBackend backend, const References &references, std::array<Images, 4> captured,
	const std::filesystem::path &directory) {
	bool accepted = true;
	bool fixed8 = false;
	constexpr std::array modes{Antialiasing::none, Antialiasing::msaa4, Antialiasing::smaa, Antialiasing::msaa4_smaa};
	for (auto mode : std::views::iota(0U, 4U)) {
		auto engine = test::hiddenEngine("smaa-engine-geometry", PixelExtent{side, side});
		auto &render = engine.renderSystem(); auto &gpu = render.forward();
		require(bool(engine.init()) && bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(modes[mode])), "engine fixture startup");
		render.setBackgroundColor(LinearColor{Vec3{0}});
		render.clearLights();
		render.setCamera(Camera::lookAt(Position{Vec3{0, 0, 4}}, Position{}, {}, FovY{std::numbers::pi_v<float> / 2}));
		VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
		RenderObjectHandle object{};
		for (auto i : std::views::iota(0U, residualFixture * unsigned(offsets.size()))) {
			const auto f = i / offsets.size(), o = i % offsets.size();
			if (object.value) { require(bool(render.removeObject(object)), "engine fixture removal"); }
			MeshDescriptor mesh;
			// A 90-degree pinhole at z=4 spans eight world units, independently mapping drawable geometry to world coordinates.
			for (const auto &triangle : triangles(f)) {
				const auto first = unsigned(mesh.positions.size());
				for (const auto &point : triangle) {
					mesh.positions.push_back(Vec3{float((point[0] + offsets[o][0] - side / 2) * 8 / side),
						float((side / 2 - point[1] - offsets[o][1]) * 8 / side), 0});
				}
				mesh.indices.insert(mesh.indices.end(), {first, first + 2, first + 1});
			}
			const auto created = render.addMesh(std::move(mesh));
			require(bool(created), "engine mesh creation"); object = *created;
			require(bool(render.setObjectUnlit(object, true)), "engine unlit state");
			for (auto frame : std::views::iota(0, 3)) { require(bool(engine.renderFrame()), "engine ordinary frame"); }
			VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
			vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice, &properties);
			fixed8 = driver.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;
			const auto path = directory / std::format("engine-b{}-m{}-f{}-o{}.png", unsigned(backend), mode, f, o);
			require(bool(render.captureFrameToPng(path)), "engine capture");
			captured[mode][i] = pixels(path);
			const auto metrics = measure(captured[mode][i], references[i]);
			std::println("engine.backend={} mode={} fixture={} offset={} edge.mae={} area.error={} contrast={}",
				unsigned(backend), mode, f, o, metrics[0], metrics[1], metrics[2]);
			if (mode < 2) { accepted = evaluateThinFeature(metrics, f) && accepted; }
		}
		inspectionSheet(captured[mode], references, directory / std::format("engine-inspection-b{}-m{}.png", unsigned(backend), mode));
		if (mode >= 2) { accepted = evaluateFilteredMode(captured[mode], captured[mode - 2], references, mode == 3, fixed8) && accepted; }
		render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
		require(gpu.validationErrorCount() == 0, "engine geometry validation including teardown");
	}
	return accepted;
}

/// @brief Apply historical equality only to matching provenance, then check independent analytic/reference contracts.
int run(bool freeze) {
	bool accepted = true;
	std::println("IQ-03 reconciliation (authorized 2026-11-04): provenance-gated history; independent CPU HIGH conformance and reference-relative quality");
	const auto directory = output / (freeze ? "frozen" : "current");
	require(!freeze || !std::filesystem::exists(directory), "freeze already exists; never overwrite references");
	require(freeze || frozenHashes[0] != 0, "freeze receipts not populated");
	std::filesystem::create_directories(directory);
	const auto references = prepareReferences(freeze);
	std::println("IQ-03a grid={} support={} edge.ratios={} residual={} residual.ratio={} regression={} area={} thin.contrast={} outside.bytes={} repeat.bytes={} repeat.metric={}",
		grid, filterSupport, maxSmaaRatio, names[residualFixture], maxCombinedResidualRatio, maxCombinedRegression,
		maxAreaRelative, minThinContrast, outsideByteLimit, repeatByteLimit, repeatMetricLimit);
	for (auto backend : {RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		std::array<Images, 4> captured;
		bool fixed8 = false;
		constexpr std::array modes{Antialiasing::none, Antialiasing::msaa4, Antialiasing::smaa, Antialiasing::msaa4_smaa};
		for (auto mode : std::views::iota(0U, freeze ? 2U : 4U)) {
			auto engine = test::hiddenEngine("smaa-quality-freeze", PixelExtent{side, side});
			auto &render = engine.renderSystem();
			auto &gpu = render.forward();
			gpu.debugSmaaReadback = mode >= 2;
			require(bool(engine.init()) && bool(render.setRendererBackend(backend)), "init/backend");
			require(bool(render.setAntialiasing(modes[mode])), "AA selection");
			render.setBackgroundColor(LinearColor{Vec3{0}});
			render.clearLights();
			auto native = std::make_shared<NativeFixture>();
			require(bool(render.setNativeDraw([native](const NativeDrawContext &c) { return native->record(c); })), "native registration");
			std::array<Images, repeats> repeated;
			std::array<std::array<double, names.size()>, repeats> errors{};
			std::array<std::uint64_t, repeats> hashes{};
			hashes.fill(14695981039346656037ULL);
			bool historical = false;
			for (auto i : std::views::iota(0U, unsigned(references.size()))) {
				const auto f = i / offsets.size(), o = i % offsets.size();
				native->triangles = triangles(f);
				native->offset = offsets[o];
				native->detail = f == residualFixture;
				render.notifyNativeDrawChanged();
				for (auto repeat : std::views::iota(0U, repeats)) {
					for (auto frame : std::views::iota(0U, 3U)) { require(bool(engine.renderFrame()), "ordinary frame"); }
#ifndef NDEBUG
					require(gpu.validationActive(), "Debug requires active Khronos validation");
#endif
					if (i == 0 && repeat == 0) {
						historical = historicalBaseline(gpu);
						VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
						VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
						vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice, &properties);
						require(properties.properties.limits.standardSampleLocations, "analytic oracle requires standard sample locations");
						fixed8 = driver.driverID == VK_DRIVER_ID_MESA_LLVMPIPE;
					}
					const auto stem = std::format("b{}-m{}-f{}-o{}-r{}", unsigned(backend), mode, f, o, repeat);
					require(bool(render.captureFrameToPng(directory / (stem + ".png"))), "capture");
					auto &rgba = repeated[repeat][i];
					rgba = pixels(directory / (stem + ".png"));
					if (mode < 2) {
						const auto baseline = freeze ? repeated[0][i] : pixels(frozenReferences / (stem + ".png"));
						if (historical || freeze) { require(rgba == baseline, "none/MSAA differs from frozen zero-variation decoded RGBA"); }
						const auto format = gpu.targets.front().swapchain.imageFormat;
						const bool unorm = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_B8G8R8A8_UNORM;
						const auto analytic = stageRgba(stageInput(native->triangles, native->offset, native->detail, native->samples, unorm));
						require(stageCompare(std::as_bytes(std::span{rgba}), analytic, 4, stem, 2), "analytic coverage/transfer/format quantization");
					}
					const auto metrics = measure(rgba, references[i]);
					errors[repeat][f] += metrics[0] / offsets.size();
					hashPixels(hashes[repeat], rgba);
					std::println("fixture={} edge.mae={} coverage.error={} contrast={}", stem, metrics[0], metrics[1], metrics[2]);
					if (mode < 2 && !evaluateThinFeature(metrics, f)) {
						const auto rgb = sheet(rgba, references[i]);
						require(stbi_write_png((directory / (stem + "-failure.png")).c_str(), side * 4, side, 3, rgb.data(), side * 12), "failure sheet");
						if (mode >= 2 && repeat == 0) { intermediateSheet(gpu.targets.front(), directory / (stem + "-intermediates.png"), false); }
						accepted = false; // Collect every fixture for diagnosis, then fail the executable below.
					}
				}
			}
			require(native->draws > 0 && native->samples == (mode % 2 == 0 ? 1 : 4) && native->attachments == (backend == RendererBackend::forward ? 1 : 3), "native metadata");
			VkPhysicalDeviceDriverProperties driver{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
			VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &driver};
			vkGetPhysicalDeviceProperties2(gpu.physicalDevice.physicalDevice, &properties);
			const auto &target = gpu.targets.front();
			std::println("device={} driver={} info={} type={} SDL={} format={} colorspace={} mutable={}",
				properties.properties.deviceName, driver.driverName, driver.driverInfo, int(properties.properties.deviceType), SDL_GetCurrentVideoDriver(),
				int(target.swapchain.imageFormat), int(target.swapchain.colorSpace), gpu.device.swapchainMutableFormat);
			for (auto repeat : std::views::iota(0U, repeats)) {
				inspectionSheet(repeated[repeat], references, directory / std::format("inspection-b{}-m{}-r{}.png", unsigned(backend), mode, repeat));
				std::println("backend={} mode={} repeat={} rgba.fnv64={:016x} edge.mae={}", unsigned(backend), mode, repeat, hashes[repeat], errors[repeat]);
				if (mode < 2 && (historical || freeze)) { require(errors[repeat] == frozenErrors[mode], "none/MSAA frozen metric changed beyond zero repeat variation"); }
				require(hashes[repeat] == hashes[0], "nonzero repeat variation");
				if (!freeze && historical && mode < 2) { require(hashes[repeat] == frozenHashes[mode], "frozen pixel receipt changed"); }
			}
			captured[mode] = std::move(repeated[0]);
			if (mode >= 2) {
				const auto &base = captured[mode - 2];
				const auto modeAccepted = evaluateFilteredMode(captured[mode], base, references, mode == 3, fixed8);
				std::println("quality.backend={} mode={} edge.mae={} accepted={}", unsigned(backend), mode, errors[0], modeAccepted);
				accepted = modeAccepted && accepted;
				accepted = stageOracle(engine, *native, references, directory) && accepted;
				staleData(engine, *native, directory, unsigned(backend), mode);
			}
			native.reset();
			render.waitIdle();
			engine.gui().shutdownVulkan();
			render.shutdown();
			require(gpu.validationErrorCount() == 0, "validation errors including teardown");
			std::println("backend={} mode={} validation.errors=0", unsigned(backend), mode);
		}
		negativeControls(captured[0], captured[1], references, directory / std::format("b{}", unsigned(backend)), fixed8);
		if (!freeze) { accepted = engineGeometry(backend, references, captured, directory) && accepted; }
	}
	require(accepted, "SMAA reference-conformance/effect/detail/outside gates failed (see per-fixture metrics and images)");
	std::println("IQ-03a independent references, preservation, SMAA/combined effect/detail/outside gates and stale-data checks PASS");
	return 0;
}
} // namespace

/// @brief --cpu exercises the real acceptance controls without graphics; --oracle checks 144 pinned stage fixtures.
int main(int argc, char **argv) try {
	metricChecks();
	stageTableChecks();
	if (argc == 2 && std::string_view{argv[1]} == "--cpu") {
		const auto references = prepareReferences(false);
		Images none, msaa;
		for (unsigned i : std::views::iota(0U, unsigned(references.size()))) {
			const unsigned f = i / offsets.size();
			none[i] = stageRgba(stageInput(triangles(f), offsets[i % offsets.size()], f == residualFixture, 1));
			msaa[i] = stageRgba(stageInput(triangles(f), offsets[i % offsets.size()], f == residualFixture, 4));
		}
		for (bool fixed8 : {false, true}) {
			negativeControls(none, msaa, references, output / "current" / (fixed8 ? "cpu-fixed8" : "cpu-ideal"), fixed8);
		}
		const auto flat = stageRgba(std::vector<unsigned char>(side * side)); auto stale = flat; stale[0] = 1;
		require(stageWithin(flat, flat, flat) && !stageWithin(stale, flat, flat), "flat-output clearing control");
		std::println("CPU acceptance path PASS: both sampler models, provenance mismatches and all focused controls");
		return 0;
	}
	if (argc == 2 && std::string_view{argv[1]} == "--oracle") { return runStageOracle(); }
	require(argc == 1 || (argc == 2 && std::string_view{argv[1]} == "--freeze"), "usage: RenderSmaaQualityTests [--cpu|--freeze|--oracle]");
	return run(argc == 2);
} catch (const std::exception &error) {
	std::println("IQ-03a FAIL: {}", error.what());
	return 1;
}
