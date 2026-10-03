/**
 * @file
 * @brief GUI callback exceptions recover unfinished ImGui windows and report a one-shot step error.
 */

#include <imgui.h>
#include <vulkan/vulkan_core.h>
#include <VVPPL.h>

import std;
import VVE.TestSupport;

import VVEngine.Simple;
import VVEngine.Simple.Renderer;

/// @brief Keeps rendering usable after a callback throws between ImGui::Begin and ImGui::End.
int main() {
	auto engine = vve::test::hiddenEngine("gui-callback-error-tests", vve::PixelExtent{.width = 64, .height = 64});
	if (!engine.init()) { return 1; }
	engine.renderSystem().setPostProcessSetup([](vvppl::PostProcessing &chain) { chain.addInvert(); });
	if (!engine.renderFrame() || !engine.gui().ready()) { return 2; }
	// The executable must see the same ImGui runtime initialized by the engine.
	const bool context_visible = ImGui::GetCurrentContext() != nullptr;
	std::println("[GuiCallbackErrorTests] engine_gui_ready=true imgui_context_visible={}", context_visible);
	if (!context_visible) { return 12; }
	const auto &renderer = engine.renderSystem().forward();
	using RecordedPass = vve::simple::ForwardRenderer::RecordedPass; ///< Optional passes around GUI preparation.
	// Post-processing without GUI draw data presents directly from the transfer layout.
	if (std::ranges::count(renderer.lastRecordedPassOrder(), RecordedPass::post_process) != 1 ||
		std::ranges::count(renderer.lastRecordedPassOrder(), RecordedPass::gui) != 0) { return 9; }
	const bool recovery_assert = ImGui::GetIO().ConfigErrorRecoveryEnableAssert;
	bool callback_called{};
	engine.gui().draw([&callback_called] {
		callback_called = true;
		ImGui::SetNextWindowPos(ImVec2{0, 0});
		ImGui::SetNextWindowSize(ImVec2{64, 64});
		ImGui::Begin("Throwing callback");
		throw std::runtime_error{"GUI callback test failure"};
	});

	// Preparation recovers unfinished GUI stacks before recording; the next input step reports the error.
	try {
		if (!engine.renderFrame() || !callback_called) { return 3; }
		const auto failed = engine.step();
		std::println("[GuiCallbackErrorTests] callback_called={} step_error={}", callback_called,
			failed ? "none" : vve::errorName(failed.error()));
		if (failed || failed.error() != vve::Error::platform_error) { return 4; }
		if (ImGui::GetIO().ConfigErrorRecoveryEnableAssert != recovery_assert) { return 5; }
		if (std::ranges::count(renderer.lastRecordedPassOrder(), RecordedPass::gui) != 1) { return 10; }

		// Consuming the error allows a balanced replacement callback and later steps to succeed.
		callback_called = false;
		engine.gui().draw([&callback_called] {
			callback_called = true;
			ImGui::SetNextWindowPos(ImVec2{0, 0});
			ImGui::SetNextWindowSize(ImVec2{64, 64});
			ImGui::Begin("Recovered callback");
			ImGui::TextUnformatted("Rendering continues");
			ImGui::End();
		});
		if (!engine.step() || !engine.renderFrame() || !callback_called || !engine.step()) { return 6; }
		const auto passes = renderer.lastRecordedPassOrder();
		if (std::ranges::count(passes, RecordedPass::post_process) != 1 ||
			std::ranges::count(passes, RecordedPass::gui) != 1 || passes.back() != RecordedPass::gui ||
			passes[passes.size() - 2U] != RecordedPass::post_process) { return 10; }
		std::println("[GuiCallbackErrorTests] post_process_passes=1 gui_passes=1 recovered=true");
		// An empty callback drops the GUI pass while retaining the post-processing chain.
		engine.gui().draw([]{});
		if (!engine.renderFrame() || std::ranges::count(renderer.lastRecordedPassOrder(), RecordedPass::gui) != 0 ||
			std::ranges::count(renderer.lastRecordedPassOrder(), RecordedPass::post_process) != 1) { return 11; }
	} catch (const std::exception &error) {
		std::println("[GuiCallbackErrorTests] escaped_exception={}", error.what());
		return 7;
	}

	// Observe completed GPU work so validation also covers the recovered frame.
	engine.renderSystem().waitIdle();
	std::println("[GuiCallbackErrorTests] recovered={} validationActive={} validationErrorCount={}",
		callback_called, renderer.validationActive(), renderer.validationErrorCount());
	if (renderer.validationActive() && renderer.validationErrorCount() != 0U) { return 8; }
	return 0;
}
