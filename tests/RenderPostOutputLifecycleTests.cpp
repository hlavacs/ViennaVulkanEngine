/// @file
/// @brief Single-window output transitions with ordinary submissions, independent colour oracles and fenced ownership checks.
#include <SDL3/SDL_video.h>
#include <stb_image_write.h>
#include <vulkan/vulkan.h>
#include <VVPPL.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;
#include "support/ToneMappingOracle.hpp"
#include "support/PostOutputOracle.hpp"

namespace {
using namespace vve;
using namespace vve::test::postoutput;
using Schedule = std::vector<std::pair<ToneMapping, std::string>>; ///< Independently requested settings and reason for each submission.
constexpr PixelExtent extent{48, 40}; ///< Empty scene, no GUI: every pixel belongs to the analytic swatch.
constexpr unsigned toggleCount{24}; ///< More than twenty runtime changes exercise bounded retirement.
constexpr std::array modes{Antialiasing::none, Antialiasing::msaa4, Antialiasing::smaa, Antialiasing::msaa4_smaa}; ///< Startup matrix.
const std::filesystem::path root{VVE_POST_OUTPUT_LIFECYCLE_DIR}; ///< Candidate evidence stays outside immutable references.

/// @brief Account for actual owned objects: five output objects, optional postImage and two fallback GUI objects.
std::size_t outputBundle(const simple::WindowTarget &target, bool chain) {
	const bool srgbGui = target.swapchain.guiFormat == VK_FORMAT_R8G8B8A8_SRGB ||
		target.swapchain.guiFormat == VK_FORMAT_B8G8R8A8_SRGB;
	return 5 + unsigned(chain) + (srgbGui ? 2 : 0);
}

/// @brief Convert old receipts only after ordinary slot reuse observes their fences, using the original settings and encoding.
void inspect(Renderer &renderer, const Schedule &schedule, bool chain, bool smaa, const std::filesystem::path &directory) {
	require(renderer.outputDiagnostics.size() == schedule.size(), "every planned ordinary frame has a receipt");
	constexpr unsigned columns{8}, cellWidth{extent.width * 3};
	const unsigned width = columns * cellWidth, height = ((schedule.size() + columns - 1) / columns) * extent.height;
	std::vector<unsigned char> sheet(width * height * 3);
	std::ofstream manifest{directory / "frames.tsv"};
	manifest << "index\tserial\trequest\top\texposure\tencoded\tpreserve_alpha\tpasses\tmax_byte_error\n";
	std::vector<unsigned char> validPixels;
	// Every cell shows actual output, the independent CPU swatch and a 32x amplified absolute difference.
	for (auto index : std::views::iota(std::size_t{}, schedule.size())) {
		const auto &[settings, reason] = schedule[index];
		const auto &frame = renderer.outputDiagnostics[index];
		require(frame.serial && frame.serial <= renderer.completedSubmitSerial(), "receipt covered by an observed submission fence");
		require(same(frame.settings, settings), "old receipt retains requested settings after later setters");
		require(frame.window == renderer.targets.front().handle && frame.extent.width == extent.width && frame.extent.height == extent.height,
			"recorded single-window identity and extent");
		const bool srgbFormat = frame.format == VK_FORMAT_R8G8B8A8_SRGB || frame.format == VK_FORMAT_B8G8R8A8_SRGB;
		const bool optional = smaa || settings.op != ToneMappingOperator::none;
		require(frame.encoded == (srgbFormat || optional) && frame.preserveAlpha == optional, "immutable recorded output encoding and alpha policy");
		checkPasses(frame, renderer.rendererBackend(), settings, chain, smaa);
		const auto path = directory / std::format("frame-{:03}.png", index);
		require(bool(renderer.writeOutputDiagnostic(frame, path)), "fenced diagnostic PNG conversion");
		const auto pixels = test::imagePixels(path);
		require(pixels && pixels->size() == extent.width * extent.height * 4, "decoded RGBA dimensions");
		const auto expected = expectedBytes(settings, chain, smaa, srgbFormat);
		int maximumError{};
		for (auto pixel : std::views::iota(0U, extent.width * extent.height)) {
			for (auto c : std::views::iota(0U, 4U)) {
				maximumError = std::max(maximumError, std::abs(int((*pixels)[pixel * 4 + c]) - expected[c]));
			}
			require((*pixels)[pixel * 4 + 3] == 255, "opaque alpha exact");
			const auto destination = ((index / columns * extent.height + pixel / extent.width) * width +
				index % columns * cellWidth + pixel % extent.width) * 3;
			for (auto c : std::views::iota(0U, 3U)) {
				sheet[destination + c] = (*pixels)[pixel * 4 + c];
				sheet[destination + extent.width * 3 + c] = expected[c];
				sheet[destination + extent.width * 6 + c] = std::min(255, 32 * std::abs(int((*pixels)[pixel * 4 + c]) - expected[c]));
			}
		}
		std::println("frame={} serial={} reason={} expected={} maximum_byte_error={} encoded={} preserve_alpha={} passes={}",
			index, frame.serial, reason, expected, maximumError, frame.encoded, frame.preserveAlpha, frame.passes | std::views::transform([](Pass p) { return int(p); }));
		require(maximumError <= int(byteTolerance), "every scene pixel matches ToneMappingOracle within 1/255");
		// Supplemental API equality only: each invalid frame has already passed the independent colour oracle above.
		if (reason.starts_with("valid-")) { validPixels = *pixels; }
		if (reason.starts_with("invalid-")) { require(*pixels == validPixels, "invalid setter preserves output exactly"); }
		manifest << std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\n", index, frame.serial, reason, int(settings.op),
			settings.exposure, frame.encoded, frame.preserveAlpha,
			frame.passes | std::views::transform([](Pass p) { return int(p); }), maximumError);
	}
	require(bool(manifest), "per-frame evidence write");
	require(stbi_write_png((directory / "inspection.png").string().c_str(), width, height, 3, sheet.data(), width * 3), "inspection sheet write");
}

/// @brief Exercise one startup selection without resize, extra windows, format overrides or capture-helper idle waits.
void lifecycle(RendererBackend backend, Antialiasing aa, bool chain) {
	auto engine = test::hiddenEngine("post-output-lifecycle", extent);
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	require(bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(aa)), "startup backend and AA");
	render.setBackgroundColor({Vec3{background[0], background[1], background[2]}});
	if (chain) {
		render.setPostProcessSetup([](vvppl::PostProcessing &post) {
			auto &grade = post.addColorGrade();
			for (auto c : std::views::iota(0U, 3U)) { grade.gain[c] = float(gain[c]); }
		});
	}
	require(bool(engine.init()) && test::renderFrames(engine, 3), "single-window startup frames");
	presentation(renderer);
#ifndef NDEBUG
	require(renderer.validationActive(), "Khronos validation must be active");
	require(std::getenv("VK_LAYER_VALIDATE_SYNC") && std::string_view{std::getenv("VK_LAYER_VALIDATE_SYNC")} == "1", "synchronization validation requested");
#endif
	require(renderer.targets.size() == 1 && render.lastRenderedWindowCount() == 1, "one real rendered window");
	const auto &target = renderer.targets.front();
	const bool smaa = aa == Antialiasing::smaa || aa == Antialiasing::msaa4_smaa;
	const bool four = aa == Antialiasing::msaa4 || aa == Antialiasing::msaa4_smaa;
	require(renderer.smaaEnabled() == smaa && renderer.sampleCount() == (four ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT), "actual startup AA");
	const auto bundle = outputBundle(target, chain);
	// SMAA retains eleven window objects plus two tables and two samplers until shutdown; AA is startup-only.
	const std::size_t resident = smaa ? 15 + bundle : 0;
	require(renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{resident, 0}, "initial optional-resource inventory");
	const auto smaaOwners = std::tuple{VkImage{target.ldrImage.image}, VkImage{target.smaaImages[0].image}, VkImage{target.smaaImages[1].image},
		target.smaaSet, VkPipeline{target.toneMapPipeline.pipeline}, VkImage{target.postImage.image}};
	const auto idle = renderer.deviceWaitIdleCount();
	const auto directory = root / std::format("backend-{}-aa-{}-gain-{}", int(backend), int(aa), chain);
	std::filesystem::create_directories(directory);
	Schedule schedule;
	std::optional<std::pair<std::uint64_t, std::size_t>> retired;
	std::array<std::size_t, 2> peak{};
	renderer.debugOutputReadback = true;

	/// @brief Queue without host readback/waits; ordinary reuse may observe only the oldest slot's fence.
	const auto queue = [&](ToneMapping settings, std::string reason) {
		const auto serial = renderer.lastSubmitSerial();
		const auto receipts = renderer.outputDiagnostics.size();
		require(serial - renderer.completedSubmitSerial() >= 2, "two uncollected submissions before the next ordinary frame");
		require(bool(engine.renderFrame()) && render.lastRenderedWindowCount() == 1, "ordinary frame presented");
		require(renderer.lastSubmitSerial() == serial + 1 && renderer.outputDiagnostics.size() == receipts + 1, "next frame has exactly one submission and receipt");
		const auto &frame = renderer.outputDiagnostics.back();
		require(frame.serial == serial + 1 && same(frame.settings, settings), "next recorded frame uses requested settings");
		const auto pending = renderer.lastSubmitSerial() - renderer.completedSubmitSerial();
		require(pending == 2, "both frame slots remain submitted and uncollected");
		const auto premature = renderer.writeOutputDiagnostic(frame, directory / "premature.png");
		require(!premature && premature.error() == Error::missing_object, "readback rejects an unobserved fence");
		const auto counts = renderer.toneMapResourceCounts();
		const auto live = smaa ? resident : settings.op == ToneMappingOperator::none ? 0 : bundle;
		require(counts[0] == live && counts[1] <= bundle, "optional objects stay within one live and one retired bundle");
		if (retired) {
			const auto [lastUse, objects] = *retired;
			require(counts[1] == (renderer.completedSubmitSerial() < lastUse ? objects : 0), "release follows last-use fence, never an earlier slot");
			if (renderer.completedSubmitSerial() >= lastUse) { retired.reset(); }
		}
		if (smaa) {
			require(smaaOwners == std::tuple{VkImage{target.ldrImage.image}, VkImage{target.smaaImages[0].image}, VkImage{target.smaaImages[1].image},
				target.smaaSet, VkPipeline{target.toneMapPipeline.pipeline}, VkImage{target.postImage.image}}, "startup SMAA and its source owners survive tone changes");
		}
		require(renderer.deviceWaitIdleCount() == idle && renderer.validationErrorCount() == 0, "no idle wait or validation error during transitions");
		for (auto i : {0U, 1U}) { peak[i] = std::max(peak[i], counts[i]); }
		std::println("request={} submit={} completed={} uncollected={} live={} retired={} last_use={}",
			reason, frame.serial, renderer.completedSubmitSerial(), pending, counts[0], counts[1], retired ? retired->first : 0);
		schedule.emplace_back(settings, std::move(reason));
	};
	/// @brief Disable must immediately move the complete bundle into retirement while both older slots remain uncollected.
	const auto select = [&](ToneMapping settings) {
		const auto before = renderer.toneMapResourceCounts();
		require(renderer.lastSubmitSerial() - renderer.completedSubmitSerial() == 2, "transition begins with two submissions uncollected");
		require(bool(render.setToneMapping(settings)) && same(render.toneMapping(), settings), "valid runtime selection");
		if (!smaa && settings.op == ToneMappingOperator::none && before[0]) {
			retired = std::pair{renderer.lastSubmitSerial(), before[0]};
			require(renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{0, before[1] + before[0]}, "complete output/postImage owner bundle retained on disable");
		}
	};

	const ToneMapping one{ToneMappingOperator::khronos_neutral, 1}, two{one.op, 2};
	const auto nan = std::numeric_limits<Scalar>::quiet_NaN(), infinity = std::numeric_limits<Scalar>::infinity();
	const std::array invalid{ToneMapping{static_cast<ToneMappingOperator>(999), 1}, ToneMapping{one.op, 0},
		ToneMapping{one.op, -1}, ToneMapping{one.op, nan}, ToneMapping{one.op, infinity}};
	unsigned stage{};
	// Valid and invalid calls each have their own next-frame receipt; no diagnostic is consumed until the entire sequence ends.
	for (const auto settings : {ToneMapping{}, one, two, ToneMapping{}, one}) {
		select(settings);
		queue(settings, std::format("valid-{}", stage));
		for (auto index : std::views::iota(std::size_t{}, invalid.size())) {
			const auto before = renderer.toneMapResourceCounts();
			const auto failed = render.setToneMapping(invalid[index]);
			require(!failed && failed.error() == Error::invalid_argument && same(render.toneMapping(), settings), "invalid operator/exposure preserves settings");
			require(renderer.toneMapResourceCounts() == before, "invalid setter preserves live and retired owners");
			queue(settings, std::format("invalid-{}-{}", stage, index));
		}
		for (auto late : modes) {
			const auto failed = render.setAntialiasing(late);
			require(!failed && failed.error() == Error::already_initialized && render.antialiasing() == aa, "all valid late AA values retain startup selection");
		}
		const auto unknown = render.setAntialiasing(static_cast<Antialiasing>(999));
		require(!unknown && unknown.error() == Error::invalid_argument && render.antialiasing() == aa, "unknown late AA retains invalid_argument precedence");
		++stage;
	}
	for (auto toggle : std::views::iota(0U, toggleCount)) {
		const auto settings = toggle % 2 ? (toggle % 4 == 1 ? two : one) : ToneMapping{};
		select(settings);
		queue(settings, std::format("toggle-{}", toggle));
	}

	// Drain by normal slot reuse, leaving current settings different from enabled receipts; no capture helper serializes transitions.
	select({});
	renderer.debugOutputReadback = false;
	require(test::renderFrames(engine, 3), "ordinary frames observe all readback and retirement fences");
	require(renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{resident, 0} && renderer.retiredResourceCount() == 0, "optional counts return to startup level after fences retire");
	require(renderer.deviceWaitIdleCount() == idle, "no device-wide wait through final drain");
	inspect(renderer, schedule, chain, smaa, directory);
	std::println("case.backend={} aa={} gain={} frames={} toggles={} peak={} drained={} validation_active={} format={} color_space={} idle_delta=0",
		int(backend), int(aa), chain, schedule.size(), toggleCount, peak, resident, renderer.validationActive(), int(target.swapchain.imageFormat), int(target.swapchain.colorSpace));
	// Teardown is the sole explicit idle boundary, after every lifecycle assertion and old-frame readback.
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	require(renderer.validationErrorCount() == 0 && renderer.cleanupLiveAllocations == 0 &&
		renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{0, 0}, "zero validation errors and no optional owners through shutdown");
}
} // namespace

/// @brief Run the complete single-window matrix; unsupported presentation/AA/validation fails without skipping a case.
int main() try {
	fixedVectors();
	std::println("lifecycle.scope=single-window tolerance_bytes={} frames_in_flight=2 evidence_device=reported_per_case", byteTolerance);
	for (auto backend : {RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		for (auto aa : modes) {
			for (bool chain : {false, true}) { lifecycle(backend, aa, chain); }
		}
	}
	std::println("post_output_lifecycle.result=pass cases=24 frames=1296 toggles_per_case={} validation_errors=0 skipped=0", toggleCount);
	return 0;
} catch (const std::exception &error) {
	std::println(stderr, "post_output_lifecycle.failure={}", error.what());
	return 1;
}
