/// @file
/// @brief Shared independent colour and pass-order checks for ordinary output lifecycle submissions.
#pragma once
namespace vve::test::postoutput {
using namespace vve;
using namespace vve::test::tone;
using Renderer = simple::ForwardRenderer; ///< Implementation diagnostics stay outside the facade.
using Pass = Renderer::RecordedPass; ///< Command-recording tags for the independent expected suffix.
constexpr RGB background{.125, .25, 2}, gain{2, .5, .25}; ///< Exact binary16 scene input and analytic gain.
constexpr unsigned byteTolerance{1}; ///< Existing display quantization bound, in bytes.
/// @brief Compare stored settings with the independent request, including the exposure retained by none.
bool same(ToneMapping actual, ToneMapping expected) {
	return actual.op == expected.op && actual.exposure == expected.exposure;
}
/// @brief Independently compute decoded PNG bytes; the legacy UNORM path quantizes linear colour before CPU encoding.
auto expectedBytes(ToneMapping settings, bool chain, bool smaa, bool srgbFormat) {
	auto color = background;
	// The pinned colour-grade defaults are identities; the selected gain is exact in binary16.
	for (auto c : std::views::iota(0U, 3U)) {
		if (chain) { color[c] *= gain[c]; }
	}
	if (settings.op != ToneMappingOperator::none) { color = neutral(color, settings.exposure); }
	std::array<int, 4> result{0, 0, 0, 255};
	for (auto c : std::views::iota(0U, 3U)) {
		double value = std::clamp(color[c], 0.0, 1.0);
		if (!srgbFormat && !smaa && settings.op == ToneMappingOperator::none) {
			value = std::round(value * 255) / 255;
		}
		result[c] = int(std::round(srgb(value) * 255));
	}
	return result;
}
/// @brief Exact output suffix and backend scene ordering catch bypassed, duplicated or incorrectly ordered passes.
void checkPasses(const Renderer::OutputDiagnostic &frame, RendererBackend backend, ToneMapping settings, bool chain, bool smaa) {
	std::vector<Pass> expected;
	if (chain) { expected.push_back(Pass::post_process); }
	if (settings.op != ToneMappingOperator::none) { expected.push_back(Pass::tone_mapping); }
	else if (smaa) { expected.push_back(Pass::output_identity); }
	if (smaa) { expected.insert(expected.end(), {Pass::smaa_edges, Pass::smaa_weights, Pass::smaa_blend}); }
	const auto first = std::ranges::find_if(frame.passes, [](Pass pass) { return pass >= Pass::post_process; });
	require(std::ranges::equal(std::ranges::subrange(first, frame.passes.end()), expected), "exact recorded output suffix");
	// Scene tags precede the entire suffix; no GUI, shadows or material masks can dilute this swatch.
	const auto scene = std::ranges::find(frame.passes, backend == RendererBackend::forward ? Pass::forward_color : Pass::forward_transparency);
	require(scene != frame.passes.end() && scene < first, "scene precedes output");
	if (backend != RendererBackend::forward) {
		const auto geometry = std::ranges::find(frame.passes, Pass::gbuffer);
		const auto lighting = std::ranges::find(frame.passes, Pass::deferred_lighting);
		require(geometry < lighting && lighting < scene, "G-buffer then lighting then composition");
	}
}
} // namespace vve::test::postoutput
