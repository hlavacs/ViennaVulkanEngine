/** @file @brief Debug builds name the G-buffer, shadow maps and attachments and label every pass for RenderDoc; Release records nothing. */
#include <vulkan/vulkan_core.h>

import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVEngine.Simple.Vulkan;
import VVE.TestSupport;

namespace {

/// @brief Reports one failed expectation and returns false.
[[nodiscard]] bool fail(std::string_view what) {
	std::println("[RenderDebugLabelsTests] failed: {}", what);
	return false;
}

/// @brief True when some recorded label contains the text (labels carry two spaces of indentation per nesting level).
[[nodiscard]] bool hasLabel(const std::vector<std::string> &labels, std::string_view text) {
	return std::ranges::any_of(labels, [text](const std::string &label) { return label.find(text) != std::string::npos; });
}

/// @brief Renders a deferred frame with one light of each type and checks the names and labels of that frame.
[[nodiscard]] bool checkDeferred() {
	using namespace vve;
	auto engine = test::hiddenEngine("debug-labels", PixelExtent{96, 64});
	auto &render = engine.renderSystem();
	if (!render.setRendererBackend(RendererBackend::deferred) || !engine.init()) { return fail("deferred init"); }
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 5, 6}}, Position{}));
	const auto floor = render.addPlane(Vec2{4, 4}, LinearColor{.value = Vec3{0.5F}});
	const auto cube = render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{0.7F, 0.3F, 0.1F}},
		Transform{.translation = Position{.value = Vec3{0, 1, 0}}});
	if (!floor || !cube) { return fail("scene"); }
	const auto dark = LinearColor{.value = Vec3{0}};
	render.setDirectionalLight(Direction{.value = Vec3{0.3F, -1, 0.2F}}, LinearColor{}, LightIntensity{1}, dark);
	render.setSpotLight(Position{.value = Vec3{0, 5, 0}}, Direction{.value = Vec3{0, -1, 0}}, LinearColor{}, LightIntensity{4}, LightRange{10},
		SpotConeAngle{0.5F}, dark);
	render.setPointLight(Position{.value = Vec3{2, 3, 0}}, LinearColor{}, LightIntensity{4}, LightRange{10}, dark);
	if (!engine.renderFrame() || !engine.renderFrame()) { return fail("render"); }

	const auto &renderer = render.forward();
	const auto &labels = renderer.debugLabels;
	const auto &target = renderer.targets.front();
	std::println("[RenderDebugLabelsTests] compiled={} debug_utils_active={} labels={}", simple::VulkanDebugLabels::compiled,
		labels.active(), labels.frameLabels().size());
	if constexpr (!simple::VulkanDebugLabels::compiled) {
		// Release: no names, no labels, no extension use.
		return labels.frameLabels().empty() && labels.nameOf(target.gbufferImages[0].image).empty() && !labels.active()
			? true : fail("release build recorded debug names");
	} else {
		const std::array<std::string_view, 3> contents{"G-buffer 0 base colour", "G-buffer 1 world normal", "G-buffer 2 emission"};
		for (std::size_t index{}; index < contents.size(); ++index) {
			const auto name = labels.nameOf(target.gbufferImages[index].image);
			std::println("[RenderDebugLabelsTests] gbuffer{}=\"{}\"", index, name);
			if (name.find(contents[index]) == std::string::npos || name.find("debug-labels") == std::string::npos) { return fail(contents[index]); }
			if (labels.nameOf(target.gbufferImages[index].imageView).empty()) { return fail("G-buffer view name"); }
		}
		if (labels.nameOf(target.depthImage.image).find("depth") == std::string::npos ||
			labels.nameOf(target.hdrImage.image).find("HDR colour") == std::string::npos ||
			labels.nameOf(target.swapchain.images.front()).find("swapchain image 0") == std::string::npos) { return fail("attachment names"); }
		const auto directional = labels.nameOf(renderer.dirShadowArray.image), spot = labels.nameOf(renderer.spotShadowArray.image),
			point = labels.nameOf(renderer.pointShadowArray.image);
		std::println("[RenderDebugLabelsTests] shadow_dir=\"{}\" shadow_spot=\"{}\" shadow_point=\"{}\"", directional, spot, point);
		if (directional.find("Shadow map directional") == std::string::npos || spot.find("Shadow map spot") == std::string::npos ||
			point.find("Shadow map point") == std::string::npos) { return fail("shadow map names"); }
		if (renderer.dirShadowArray.layerViews.size() < 4U ||
			labels.nameOf(renderer.dirShadowArray.layerViews[3]) != "Shadow map directional: light 0 cascade 3" ||
			labels.nameOf(renderer.pointShadowArray.layerViews[5]) != "Shadow map point: light 0 face -Z") { return fail("shadow layer names"); }
		if (labels.nameOf(VkPipeline{renderer.gbufferPipeline.pipeline}) != "G-buffer pipeline" ||
			labels.nameOf(VkPipeline{renderer.lightingPipeline.pipeline}) != "Deferred lighting pipeline") { return fail("pipeline names"); }
		for (const auto &label : labels.frameLabels()) { std::println("[RenderDebugLabelsTests] label {}", label); }
		for (const std::string_view expected : {"Frame: debug-labels", "Shadow maps", "Shadow: directional light 0 cascade 0",
			"Shadow: directional light 0 cascade 3", "Shadow: spot light 0", "Shadow: point light 0 face -Z", "G-buffer:", "Deferred lighting",
			"Sky and transparency"}) {
			if (!hasLabel(labels.frameLabels(), expected)) { return fail(expected); }
		}
		// Every pass sits inside the frame region, shadow layers inside "Shadow maps".
		if (labels.frameLabels().front().starts_with(" ") || !hasLabel(labels.frameLabels(), "    Shadow: spot light 0") ||
			!hasLabel(labels.frameLabels(), "  Deferred lighting")) { return fail("label nesting"); }
		return renderer.validationErrorCount() == 0U ? true : fail("validation errors");
	}
}

/// @brief The forward backend labels its single colour pass instead of the deferred ones.
[[nodiscard]] bool checkForward() {
	using namespace vve;
	auto engine = test::hiddenEngine("debug-labels-forward", PixelExtent{64, 64});
	if (!engine.init()) { return fail("forward init"); }
	auto &render = engine.renderSystem();
	render.setCamera(Camera::lookAt(Position{.value = Vec3{0, 2, 3}}, Position{}));
	if (!render.addCuboid(Vec3{-0.5F}, Vec3{0.5F}, LinearColor{.value = Vec3{0.2F, 0.6F, 0.9F}}) || !engine.renderFrame()) { return fail("forward render"); }
	const auto &labels = render.forward().debugLabels;
	if constexpr (!simple::VulkanDebugLabels::compiled) { return labels.frameLabels().empty() ? true : fail("release forward labels"); }
	else {
		return hasLabel(labels.frameLabels(), "Forward colour") && !hasLabel(labels.frameLabels(), "G-buffer") &&
			labels.nameOf(VkPipeline{render.forward().graphicsPipeline.pipeline}) == "Forward opaque pipeline" ? true : fail("forward labels");
	}
}

} // namespace

/// @brief Exit code 0 when names and labels match the build type.
int main() {
	const bool ok = checkDeferred() && checkForward();
	std::println("[RenderDebugLabelsTests] result={}", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
