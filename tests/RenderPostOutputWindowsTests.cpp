/// @file
/// @brief Independent output, resizing, suspended surfaces and shutdown with two ordinary window submission streams.
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_error.h>
#include <imgui.h>
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
using namespace vve::test::postoutput;
const std::filesystem::path root{VVE_POST_OUTPUT_WINDOWS_DIR}; ///< Evidence never enters the frozen baseline directories.
constexpr ToneMapping one{ToneMappingOperator::khronos_neutral, 1}, two{one.op, 2}; ///< Distinct next-frame requests.
constexpr PixelExtent resized{80, 56}; ///< Only the first window changes extent.

/// @brief Snapshot the actual optional image owners, without retaining their lifetime.
auto images(const simple::WindowTarget &target) {
	return std::array<VkImage, 4>{target.postImage.image, target.ldrImage.image, target.smaaImages[0].image, target.smaaImages[1].image};
}

/// @brief Query formats visible to this process; Profiles selections exercise real presentation without claiming unavailable formats.
void surface(const Renderer &renderer, bool unorm, bool noMutable) {
	presentation(renderer);
	require(renderer.validationActive(), "Khronos validation active for every surface case");
	require(std::getenv("VK_LAYER_VALIDATE_SYNC") && std::string_view{std::getenv("VK_LAYER_VALIDATE_SYNC")} == "1",
		"synchronization validation requested");
	for (const auto &target : renderer.targets) {
		std::uint32_t count{};
		require(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physicalDevice.physicalDevice, target.surface.surface, &count, nullptr) == VK_SUCCESS,
			"surface format count");
		std::vector<VkSurfaceFormatKHR> formats(count);
		require(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physicalDevice.physicalDevice, target.surface.surface, &count, formats.data()) == VK_SUCCESS,
			"surface format list");
		for (const auto &format : formats) {
			std::println("surface.window={} offered.format={} color_space={}", target.handle.value, int(format.format), int(format.colorSpace));
		}
		const auto format = target.swapchain.imageFormat;
		require(unorm == (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_R8G8B8A8_UNORM), "requested real swapchain format exercised");
		require(target.swapchain.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, "actual SRGB_NONLINEAR surface");
		if (noMutable) { require(!renderer.device.swapchainMutableFormat && target.imageViews.ownedGuiViews.empty(), "mutable views excluded"); }
		std::println("surface.window={} exercised.format={} gui_format={} mutable_device={} mutable_views={} offscreen_only=none",
			target.handle.value, int(format), int(target.swapchain.guiFormat), renderer.device.swapchainMutableFormat, !target.imageViews.ownedGuiViews.empty());
	}
}

/// @brief Check completed pixels against the shared independent oracle and assemble actual/oracle/difference rows for inspection.
void inspect(Renderer &renderer, WindowHandle chainWindow, bool smaa, bool gui, const std::filesystem::path &directory, bool fractional = false) {
	constexpr unsigned width{resized.width * 3}, rowHeight{resized.height};
	std::vector<unsigned char> sheet(width * rowHeight * renderer.outputDiagnostics.size() * 3);
	std::ofstream manifest{directory / "frames.tsv"};
	manifest << "index\twindow\tserial\textent\top\texposure\tencoded\talpha\tmax_byte_error\n";
	unsigned index{};
	// Geometry defines GUI masks: opaque widget/HUD in 8x16 and a separate translucent 8x8 rectangle.
	for (const auto &frame : renderer.outputDiagnostics) {
		const bool chain = frame.window == chainWindow;
		const bool srgbFormat = frame.format == VK_FORMAT_R8G8B8A8_SRGB || frame.format == VK_FORMAT_B8G8R8A8_SRGB;
		const bool optional = smaa || frame.settings.op != ToneMappingOperator::none;
		require(frame.serial && frame.serial <= renderer.completedSubmitSerial(), "old window receipt covered by observed fence");
		require(frame.encoded == (srgbFormat || optional) && frame.preserveAlpha == optional, "immutable encoding and alpha receipts");
		// Reuse the exact suffix checker with a non-owning diagnostic copy of its pass metadata before GUI.
		Renderer::OutputDiagnostic scene;
		scene.passes = frame.passes;
		const bool hasGui = !scene.passes.empty() && scene.passes.back() == Pass::gui;
		if (hasGui) { scene.passes.pop_back(); }
		require(hasGui == (gui && chain), "GUI belongs only to its owning window");
		checkPasses(scene, renderer.rendererBackend(), frame.settings, chain, smaa);
		const auto path = directory / std::format("frame-{:03}.png", index);
		require(bool(renderer.writeOutputDiagnostic(frame, path)), "fenced output readback");
		const auto pixels = test::imagePixels(path);
		require(pixels && pixels->size() == frame.extent.width * frame.extent.height * 4U, "recorded dimensions and RGBA channel count");
		auto expected = expectedBytes(frame.settings, chain, smaa, srgbFormat);
		if (fractional && optional) { expected[3] = 64; } // Independent native clear supplies exactly .25 alpha before processing.
		const auto &target = *std::ranges::find(renderer.targets, frame.window, &simple::WindowTarget::handle);
		const bool linearGui = target.swapchain.guiFormat == VK_FORMAT_B8G8R8A8_SRGB || target.swapchain.guiFormat == VK_FORMAT_R8G8B8A8_SRGB;
		int error{};
		// Every scene channel, authored widget byte and alpha participates in the one-byte bound.
		for (auto y : std::views::iota(0U, frame.extent.height)) {
			for (auto x : std::views::iota(0U, frame.extent.width)) {
				const bool widget = hasGui && x < 8 && y < 16;
				const std::array widgetBytes{128, 64, 200, 255};
				const auto pixel = (y * frame.extent.width + x) * 4;
				for (auto c : std::views::iota(0U, 4U)) {
					int oracle = widget ? widgetBytes[c] : expected[c];
					// ImGui blends in its attachment's numeric space: encoded UNORM or decoded linear sRGB.
					if (hasGui && x >= 8 && x < 16 && y < 8 && c < 3) {
						const auto decode = [](double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); };
						const double source = widgetBytes[c] / 255.0, destination = expected[c] / 255.0, alpha = 128.0 / 255;
						const double blended = linearGui ? srgb(alpha * decode(source) + (1 - alpha) * decode(destination)) : alpha * source + (1 - alpha) * destination;
						oracle = int(std::round(blended * 255));
					}
					error = std::max(error, std::abs(int((*pixels)[pixel + c]) - oracle));
					if (c == 3) { require((*pixels)[pixel + c] == oracle, "alpha exact on scene and GUI"); }
					else {
						const auto offset = ((index * rowHeight + y) * width + x) * 3 + c;
						sheet[offset] = (*pixels)[pixel + c];
						sheet[offset + resized.width * 3] = oracle;
						sheet[offset + resized.width * 6] = std::min(255, 32 * std::abs(int((*pixels)[pixel + c]) - oracle));
					}
				}
			}
		}
		require(error <= int(byteTolerance), "both window outputs match ToneMappingOracle/GUI bytes within 1/255");
		manifest << std::format("{}\t{}\t{}\t{}x{}\t{}\t{}\t{}\t{}\t{}\n", index, frame.window.value, frame.serial,
			frame.extent.width, frame.extent.height, int(frame.settings.op), frame.settings.exposure, frame.encoded, frame.preserveAlpha, error);
		std::println("receipt={} serial={} window={} extent={}x{} op={} exposure={} encoded={} max_byte_error={}", index++, frame.serial,
			frame.window.value, frame.extent.width, frame.extent.height, int(frame.settings.op), frame.settings.exposure, frame.encoded, error);
	}
	require(bool(manifest) && index >= 20, "complete window evidence");
	require(stbi_write_png((directory / "inspection.png").string().c_str(), width, rowHeight * index, 3, sheet.data(), width * 3), "inspection sheet");
}

/// @brief Resize only a while both windows have uncollected work; inspect actual retired owners before normal reuse collects them.
void resize(simple::Engine &engine, bool smaa) {
	auto &renderer = engine.renderSystem().forward();
	auto &a = renderer.targets.front();
	auto &b = renderer.targets.back();
	const auto old = images(a), other = images(b);
	const auto otherHdr = VkImage{b.hdrImage.image};
	const auto otherSwap = VkSwapchainKHR{b.swapchain.swapchain};
	const auto lastUse = *std::ranges::max_element(a.submitSerials);
	const auto idle = renderer.deviceWaitIdleCount();
	require(lastUse > renderer.completedSubmitSerial(), "resize starts before old output last-use fence collection");
	SDL_ClearError();
	const bool requested = SDL_SetWindowSize(a.window, resized.width, resized.height);
	std::println("resize.requested={} sdl_error={}", requested, SDL_GetError());
	require(requested, "real SDL resize accepted");
	SDL_ClearError();
	const bool synced = SDL_SyncWindow(a.window);
	const std::string syncError{SDL_GetError()};
	const auto started = std::chrono::steady_clock::now(), deadline = started + std::chrono::seconds{5};
	auto drawable = renderer.currentWindowPixelExtent(a);
	// Native event acknowledgements can outlast SDL's short sync timeout; polling leaves all GPU last-use receipts untouched.
	while (drawable.width != resized.width || drawable.height != resized.height) {
		require(bool(engine.windowSystem().poll()), "resize window events");
		drawable = renderer.currentWindowPixelExtent(a);
		if (std::chrono::steady_clock::now() >= deadline) { break; }
		std::this_thread::sleep_for(std::chrono::milliseconds{1});
	}
	std::println("resize.synced={} drawable={}x{} expected={}x{} elapsed_ms={} timeout={} sync_error={}", synced,
		drawable.width, drawable.height, resized.width, resized.height,
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(),
		drawable.width != resized.width || drawable.height != resized.height, syncError);
	require(drawable.width == resized.width && drawable.height == resized.height, "X11 applied requested independent extent");
	// Existing resize uses its existing device-idle boundary. No new renderer wait is introduced by this test.
	require(renderer.recreateSwapchain(a, drawable) == VK_SUCCESS, "swapchain and extent-dependent owners rebuilt");
	require(renderer.deviceWaitIdleCount() == idle + 1 && renderer.completedSubmitSerial() >= lastUse, "existing resize wait covers old owners");
	const auto retired = renderer.retiredOutputImages();
	for (const auto image : old) {
		if (image == VK_NULL_HANDLE) { continue; }
		require(std::ranges::any_of(retired, [&](const auto &bundle) {
			return bundle.first >= lastUse && bundle.first <= renderer.completedSubmitSerial() && std::ranges::find(bundle.second, image) != bundle.second.end();
		}), "old post/LDR/edge/weight owners remain retired until fence-covered collection");
	}
	require(bool(engine.renderFrame()), "first resized ordinary frame");
	require(a.postImage.extent.width == resized.width && a.postImage.extent.height == resized.height && a.postImage.image != old[0], "new postImage extent and owner");
	if (smaa) {
		const auto current = images(a);
		for (auto index : {1U, 2U, 3U}) {
			require(current[index] != VK_NULL_HANDLE && current[index] != old[index], "new SMAA attachment owner");
		}
		for (const auto *image : {&a.ldrImage, &a.smaaImages[0], &a.smaaImages[1]}) {
			require(image->extent.width == resized.width && image->extent.height == resized.height, "new LDR/edges/weights extent");
		}
	}
	require(images(b) == other && VkImage{b.hdrImage.image} == otherHdr && VkSwapchainKHR{b.swapchain.swapchain} == otherSwap,
		"untouched window owners and swapchain remain valid");
	std::println("resize.last_use={} observed={} retired_bundles={} existing_idle_delta=1 other_window_unchanged=1", lastUse, renderer.completedSubmitSerial(), retired.size());
}

/// @brief Repeat real desktop transitions while preserving both output owners and their ordinary submission receipts.
void nativeTransitions(simple::Engine &engine, unsigned cycles, bool forceRemap) {
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	auto &a = renderer.targets.front();
	auto &b = renderer.targets.back();
	auto *window = engine.windowSystem().findWindow("a");
	require(window != nullptr, "native window exists");
	const auto retained = images(a), other = images(b);
	const auto swap = VkSwapchainKHR{a.swapchain.swapchain}, otherSwap = VkSwapchainKHR{b.swapchain.swapchain};
	const auto hdr = VkImage{a.hdrImage.image}, otherHdr = VkImage{b.hdrImage.image};
	const auto native = a.window;
	const auto idle = renderer.deviceWaitIdleCount();
	unsigned totalRetries{}, totalRemaps{};
	// Record requests independently; SDL's short synchronization deadline is followed by actual-state checks.
	const auto nativeRequest = [&](std::string_view operation, bool (*request)(SDL_Window *)) {
		SDL_ClearError();
		const auto started = std::chrono::steady_clock::now();
		const bool result = request(a.window);
		std::println("native.operation={} result={} native_flags={} elapsed_ms={} sdl_error={}", operation, result,
			SDL_GetWindowFlags(a.window), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(), SDL_GetError());
		require(result || request == SDL_SyncWindow, operation);
	};
	const auto remapWindow = [&](bool forced) {
		std::println("native.restore_remap={} forced={} window={}", ++totalRemaps, forced, a.handle.value);
		// Withdraw the iconified client, clear SDL's pending minimized flag, then map the same native window normally.
		nativeRequest("withdraw iconified window", SDL_HideWindow);
		nativeRequest("restore withdrawn window", SDL_RestoreWindow);
		nativeRequest("remap restored window", SDL_ShowWindow);
	};
	const auto awaitNativeState = [&](bool minimized, bool hidden = false, bool restoring = false) {
		const auto started = std::chrono::steady_clock::now(), deadline = started + std::chrono::seconds{5};
		auto nextRestore = started + std::chrono::milliseconds{100};
		unsigned polls{}, restores{};
		bool nativeMinimized{}, nativeHidden{};
		// A window manager can reject activation; recovery must change the observed native state within this deadline.
		do {
			++polls;
			require(bool(engine.windowSystem().poll()), "native window events");
			nativeMinimized = (SDL_GetWindowFlags(a.window) & SDL_WINDOW_MINIMIZED) != 0;
			nativeHidden = (SDL_GetWindowFlags(a.window) & SDL_WINDOW_HIDDEN) != 0;
			if (nativeMinimized == minimized && nativeHidden == hidden && window->info().minimized == minimized) { break; }
			if (restoring && nativeMinimized && std::chrono::steady_clock::now() >= nextRestore) {
				std::println("native.restore_retry={} elapsed_ms={}", ++restores,
					std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
				++totalRetries;
				// X11 activation may keep using a timestamp older than real desktop input; remapping avoids focus policy.
				if (restores == 2) { remapWindow(false); }
				else {
					nativeRequest("restore retry", SDL_RestoreWindow);
					nativeRequest("raise retry", SDL_RaiseWindow);
				}
				nextRestore = std::chrono::steady_clock::now() + std::chrono::milliseconds{100};
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{1});
		} while (std::chrono::steady_clock::now() < deadline);
		std::println("native.minimized.requested={} native.minimized.actual={} native.hidden.requested={} native.hidden.actual={} snapshot.minimized={} native.state.polls={} native.restore_retries={} native.state.elapsed_ms={}",
			minimized, nativeMinimized, hidden, nativeHidden, window->info().minimized, polls, restores,
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
		require(nativeMinimized == minimized && nativeHidden == hidden && window->info().minimized == minimized,
			"X11 visibility and snapshot confirm requested native state");
	};
	// Every cycle requires actual iconification, restored two-window presentation and the original retained output owners.
	for (auto cycle : std::views::iota(0U, cycles)) {
		std::println("native.cycle={} phase=start submit={}", cycle, renderer.lastSubmitSerial());
		nativeRequest("show window", SDL_ShowWindow);
		nativeRequest("synchronize shown window", SDL_SyncWindow);
		awaitNativeState(false);
		nativeRequest("minimize window", SDL_MinimizeWindow);
		nativeRequest("synchronize minimized window", SDL_SyncWindow);
		awaitNativeState(true);
		const auto minimizedCount = renderer.outputDiagnostics.size();
		require(bool(engine.renderFrame()) && render.lastRenderedWindowCount() == 1 && renderer.outputDiagnostics.size() == minimizedCount + 1 &&
			renderer.outputDiagnostics.back().window == b.handle, "native minimized surface emits no frame while other window presents");
		if (forceRemap) {
			require((SDL_GetWindowFlags(a.window) & SDL_WINDOW_MINIMIZED) != 0 && window->info().minimized,
				"forced recovery starts from an actually iconified window");
			remapWindow(true);
		} else {
			nativeRequest("restore window", SDL_RestoreWindow);
			nativeRequest("raise restored window", SDL_RaiseWindow);
			nativeRequest("synchronize restored window", SDL_SyncWindow);
		}
		awaitNativeState(false, false, true);
		require(test::renderFrames(engine, 1, std::chrono::seconds{10}, 2), "first native resumed frame submitted");
		const auto &resumed = renderer.outputDiagnostics[renderer.outputDiagnostics.size() - 2];
		require(resumed.window == a.handle && same(resumed.settings, two), "native resume next-frame selection");
		nativeRequest("hide window", SDL_HideWindow);
		nativeRequest("synchronize hidden window", SDL_SyncWindow);
		awaitNativeState(false, true);
		require(images(a) == retained && images(b) == other && VkSwapchainKHR{a.swapchain.swapchain} == swap &&
			VkSwapchainKHR{b.swapchain.swapchain} == otherSwap && VkImage{a.hdrImage.image} == hdr && VkImage{b.hdrImage.image} == otherHdr &&
			a.window == native && window->native() == native && renderer.deviceWaitIdleCount() == idle,
			"native cycle preserves both window owners without an idle wait");
		std::println("native.cycle={} phase=pass submit={} idle_delta=0 owners_preserved=true", cycle, renderer.lastSubmitSerial());
	}
	std::println("native.cycles.completed={} restore_retries={} remaps={} forced={}", cycles, totalRetries, totalRemaps, forceRemap);
}

/// @brief Test distinct per-window requests through the implementation's existing per-target recorder, without a new facade API.
void windows(RendererBackend backend, Antialiasing aa, bool unorm, bool noMutable, unsigned nativeCycles = 1, bool forceRemap = false) {
	auto engine = simple::Engine{simple::Windows{.value = {
		WindowDesc{.id = "a", .extent = {48, 40}, .visible = false}, WindowDesc{.id = "b", .extent = {64, 48}, .visible = false}}}};
	auto &render = engine.renderSystem();
	auto &renderer = render.forward();
	require(bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(aa)), "startup backend and AA");
	render.setBackgroundColor({Vec3{background[0], background[1], background[2]}});
	render.setPostProcessSetup([](vvppl::PostProcessing &post) {
		auto &grade = post.addColorGrade();
		for (auto c : std::views::iota(0U, 3U)) { grade.gain[c] = float(gain[c]); }
	});
	require(bool(engine.init()) && bool(engine.renderFrame()), "two-window startup");
	// Establish one chain before the measured transitions. Rebuild b's source descriptors at the startup idle boundary.
	render.waitIdle();
	auto &a = renderer.targets.front();
	auto &b = renderer.targets.back();
	b.postProcess.reset();
	require(renderer.recreateSwapchain(b, b.swapchain.extent) == VK_SUCCESS && test::renderFrames(engine, 3), "startup chain restricted to a");
	require(a.postProcess && !b.postProcess && b.postImage.image == VK_NULL_HANDLE, "vvppl on only one measured window");
	surface(renderer, unorm, noMutable);
	const bool smaa = aa != Antialiasing::none;
	require(renderer.smaaEnabled() == smaa && renderer.sampleCount() == (aa == Antialiasing::msaa4_smaa ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT), "actual AA mode");
	const auto evidenceRoot = nativeCycles > 1 ? root / std::format("native-stress-{}{}", nativeCycles, forceRemap ? "-remap" : "") :
		(forceRemap ? root / "restore-recovery" : root);
	const auto directory = evidenceRoot / (unorm ? (noMutable ? "unorm-no-mutable" : "unorm") : (noMutable ? "srgb-no-mutable" : "srgb")) /
		std::format("backend-{}-aa-{}", int(backend), int(aa));
	std::filesystem::create_directories(directory);
	renderer.debugOutputReadback = true;
	const auto idle = renderer.deviceWaitIdleCount();
	// Each target records different settings while ordinary slot fences govern completion; captures cannot serialize transitions.
	for (const auto settings : {ToneMapping{}, one, two, ToneMapping{}, one}) {
		for (auto *target : {&a, &b}) {
			const auto selected = target == &a ? settings : (settings.op == ToneMappingOperator::none ? two : one);
			require(bool(render.setToneMapping(selected)) && bool(renderer.renderFrame(*target)), "independent window transition");
			const auto &frame = renderer.outputDiagnostics.back();
			require(frame.window == target->handle && same(frame.settings, selected), "next target frame retains its own request");
			const auto premature = renderer.writeOutputDiagnostic(frame, directory / "premature.png");
			require(!premature && premature.error() == Error::missing_object, "pending window readback rejected");
		}
	}
	require(renderer.deviceWaitIdleCount() == idle && renderer.lastSubmitSerial() > renderer.completedSubmitSerial(), "transitions keep ordinary frames uncollected without idle");
	require(bool(render.setToneMapping(two)), "resize during exposure transition");
	resize(engine, smaa);
	// Keep the unaffected window from satisfying the entire budget while the resized surface is unavailable.
	require(bool(render.setToneMapping(one)) && test::renderFrames(engine, 3, std::chrono::seconds{10}, 2), "ordinary frames collect old resized owners");
	require(renderer.retiredOutputImages().empty() && renderer.toneMapResourceCounts()[1] == 0, "resized optional owners retire after observed completion");
	// Match the engine's minimized snapshot contract, independent of whether an X11 window manager minimizes hidden windows.
	auto *window = engine.windowSystem().findWindow("a");
	require(window != nullptr, "minimized window exists");
	const auto retained = images(a);
	const auto swap = VkSwapchainKHR{a.swapchain.swapchain};
	const auto count = renderer.outputDiagnostics.size();
	window->info().minimized = true;
	require(bool(render.setToneMapping(two)) && bool(engine.renderFrame()) && render.lastRenderedWindowCount() == 1, "minimized window skips while b presents");
	require(renderer.outputDiagnostics.size() == count + 1 && renderer.outputDiagnostics.back().window == b.handle && images(a) == retained,
		"minimized optional owners survive without submission");
	window->info().minimized = false;
	require(bool(engine.renderFrame()) && render.lastRenderedWindowCount() == 2, "first resumed frame presents both windows");
	const auto &resumed = renderer.outputDiagnostics[renderer.outputDiagnostics.size() - 2];
	require(resumed.window == a.handle && same(resumed.settings, two) && resumed.encoded && images(a) == retained && VkSwapchainKHR{a.swapchain.swapchain} == swap,
		"first resumed frame retains correct selection/encoding and owners");
	nativeTransitions(engine, nativeCycles, forceRemap);
	renderer.debugOutputReadback = false;
	require(test::renderFrames(engine, 3), "ordinary drain before inspecting scene receipts");
	inspect(renderer, a.handle, smaa, false, directory);
	renderer.outputDiagnostics.clear();
	// Authored widget and HUD colours must survive output toggles on the resized surface without double encoding.
	const auto source = test::writeFixture(directory, "hud.ppm", "P6\n2 2\n255\n\x80\x40\xc8\x80\x40\xc8\x80\x40\xc8\x80\x40\xc8");
	const auto hud = engine.gui().loadImage(source);
	require(bool(hud), "HUD fixture loaded");
	engine.gui().draw([&] {
		const auto p = ImGui::GetMainViewport()->Pos;
		auto *draw = ImGui::GetBackgroundDrawList();
		draw->AddRectFilled(p, {p.x + 8, p.y + 8}, IM_COL32(128, 64, 200, 255));
		draw->AddRectFilled({p.x + 8, p.y}, {p.x + 16, p.y + 8}, IM_COL32(128, 64, 200, 128));
		if (const auto id = engine.gui().imageId(*hud)) { draw->AddImage(ImTextureID(id), {p.x, p.y + 8}, {p.x + 8, p.y + 16}); }
	});
	renderer.debugOutputReadback = true;
	for (auto repeat : std::views::iota(0U, 12U)) {
		const auto settings = smaa && repeat % 3 == 0 ? ToneMapping{} : (repeat % 2 ? one : two);
		require(bool(render.setToneMapping(settings)) && bool(engine.renderFrame()), "GUI after tone transition");
	}
	renderer.debugOutputReadback = false;
	require(test::renderFrames(engine, 3), "GUI readbacks complete through ordinary fences");
	std::filesystem::create_directories(directory / "gui");
	inspect(renderer, a.handle, smaa, true, directory / "gui");
	renderer.outputDiagnostics.clear();
	engine.gui().draw({});
	// Clear the final scene attachment in every backend to test alpha through MSAA resolve, Neutral and SMAA.
	require(bool(render.setNativeDraw([](const NativeDrawContext &context) -> std::expected<void, Error> {
		if (!context.prepare && context.pass == NativeDrawPass::forward_transparent) {
			const VkClearAttachment clear{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .colorAttachment = 0,
				.clearValue = {.color = {.float32 = {float(background[0]), float(background[1]), float(background[2]), .25F}}}};
			const VkClearRect rectangle{.rect = {.extent = {context.width, context.height}}, .layerCount = 1};
			vkCmdClearAttachments(reinterpret_cast<VkCommandBuffer>(context.commandBuffer), 1, &clear, 1, &rectangle);
		}
		return {};
	})), "fractional alpha scene fixture");
	renderer.debugOutputReadback = true;
	for (auto repeat : std::views::iota(0U, 12U)) {
		require(bool(render.setToneMapping(repeat % 3 ? two : ToneMapping{})) && bool(engine.renderFrame()), "alpha output transition");
	}
	renderer.debugOutputReadback = false;
	require(test::renderFrames(engine, 3), "alpha readback fences");
	std::filesystem::create_directories(directory / "alpha");
	inspect(renderer, a.handle, smaa, false, directory / "alpha", true);
	require(renderer.deviceWaitIdleCount() == idle + 1 && renderer.validationErrorCount() == 0, "only existing resize idle, zero validation errors");
	render.waitIdle();
	engine.gui().shutdownVulkan();
	render.shutdown();
	require(renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{0, 0} && renderer.cleanupLiveAllocations == 0 && renderer.validationErrorCount() == 0,
		"zero live/retired optional owners, allocations and validation errors after shutdown");
	std::println("windows.backend={} aa={} result=pass minimize=snapshot_and_native first_resume=oracle resize=80x56 validation_errors=0", int(backend), int(aa));
}

/// @brief Five complete creation/destruction cycles per backend prove enabled SMAA/tone owners cannot survive device shutdown.
void cycles(RendererBackend backend) {
	for (auto cycle : std::views::iota(0U, 5U)) {
		auto engine = test::hiddenEngine("output-shutdown-cycle", PixelExtent{32, 32});
		auto &render = engine.renderSystem();
		auto &renderer = render.forward();
		require(bool(render.setRendererBackend(backend)) && bool(render.setAntialiasing(Antialiasing::smaa)) && bool(render.setToneMapping(two)), "cycle options");
		require(bool(engine.init()) && test::renderFrames(engine, 3) && renderer.validationActive(), "cycle initialized with active validation");
		require(renderer.toneMapResourceCounts()[0] > 0, "cycle actually allocates enabled output owners");
		render.waitIdle();
		engine.gui().shutdownVulkan();
		render.shutdown();
		require(renderer.toneMapResourceCounts() == std::array<std::size_t, 2>{0, 0} && renderer.cleanupLiveAllocations == 0 && renderer.validationErrorCount() == 0,
			"cycle releases every optional object before allocator destruction");
		std::println("shutdown.backend={} cycle={} live=0 retired=0 allocations=0 validation_errors=0", int(backend), cycle);
	}
}
} // namespace

/// @brief Real sRGB/UNORM presentation variants use the existing Profiles layer format and mutable-extension selection.
int main(int argc, char **argv) try {
	bool unorm{}, noMutable{}, forceRemap{};
	unsigned nativeCycles{};
	// Diagnostic cycles retain one real rendering fixture and can combine with every existing surface variant.
	for (int index = 1; index < argc; ++index) {
		const std::string_view option{argv[index]};
		if (option == "--native-cycles") {
			require(index + 1 < argc, "native cycle count required");
			const std::string_view count{argv[++index]};
			const auto parsed = std::from_chars(count.data(), count.data() + count.size(), nativeCycles);
			require(parsed.ec == std::errc{} && parsed.ptr == count.data() + count.size() && nativeCycles > 0 && nativeCycles <= 1000,
				"native cycle count in [1,1000]");
		} else if (option == "--force-remap") { forceRemap = true; }
		else {
			require(option == "--unorm" || option == "--no-mutable" || option == "--unorm-no-mutable", "known surface or native-cycle option");
			unorm = unorm || option != "--no-mutable";
			noMutable = noMutable || option != "--unorm";
		}
	}
	fixedVectors();
	std::println("surface.format_legend=37:R8G8B8A8_UNORM,43:R8G8B8A8_SRGB,44:B8G8R8A8_UNORM,50:B8G8R8A8_SRGB offscreen_only=none");
	if (nativeCycles) {
		windows(RendererBackend::forward, Antialiasing::none, unorm, noMutable, nativeCycles, forceRemap);
		std::println("post_output_windows.native_stress=pass cycles={} forced_remap={} validation_errors=0", nativeCycles, forceRemap);
		return 0;
	}
	for (auto backend : {RendererBackend::forward, RendererBackend::deferred, RendererBackend::hybrid_gi}) {
		for (auto aa : {Antialiasing::none, Antialiasing::smaa, Antialiasing::msaa4_smaa}) { windows(backend, aa, unorm, noMutable, 1, forceRemap); }
		cycles(backend);
	}
	std::println("post_output_windows.result=pass cases=9 shutdown_cycles=15 forced_recovery={} validation_errors=0 evidence=actual_device_above real_formats=listed_per_surface offscreen_only=none", forceRemap);
	return 0;
} catch (const std::exception &error) {
	std::println(stderr, "post_output_windows.failure={}", error.what());
	return 1;
}
