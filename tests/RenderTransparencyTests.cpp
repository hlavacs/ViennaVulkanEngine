/**
 * @file
 * @brief CPU checks for object opacity and imported glTF alpha modes, without a window or GPU.
 *
 * Functional objects:
 * - hasOpacityRoundTrip: default, set/get, range and handle errors of setObjectOpacity / objectOpacity.
 * - main: also imports a glTF file with one OPAQUE, one MASK and one BLEND material and checks the render materials.
 */

import std;

import VVEngine.Simple;
import VVE.TestSupport;

namespace {

/// @brief Checks the opacity calls on one cuboid and on an unknown handle.
[[nodiscard]] bool hasOpacityRoundTrip(vve::simple::RenderSystem &render) {
	using namespace vve;
	const auto object = render.addCuboid(Vec3{-1, -1, -1}, Vec3{1, 1, 1}, LinearColor{.value = Vec3{1, 0, 0}});
	if (!object) { return false; }
	const auto initial = render.objectOpacity(*object);
	const bool set = render.setObjectOpacity(*object, 0.25F).has_value();
	const auto stored = render.objectOpacity(*object);
	// Out-of-range and NaN values leave the stored opacity unchanged.
	const auto too_high = render.setObjectOpacity(*object, 1.5F);
	const auto negative = render.setObjectOpacity(*object, -0.1F);
	const auto not_a_number = render.setObjectOpacity(*object, std::numeric_limits<Scalar>::quiet_NaN());
	const auto unchanged = render.objectOpacity(*object);
	const auto missing_set = render.setObjectOpacity(RenderObjectHandle{}, 0.5F);
	const auto missing_get = render.objectOpacity(RenderObjectHandle{});
	std::println("[RenderTransparencyTests] initial={} set={} stored={} unchanged={}", initial.value_or(-1), set,
		stored.value_or(-1), unchanged.value_or(-1));
	return initial == 1.0F && set && stored == 0.25F && unchanged == 0.25F &&
		!too_high && too_high.error() == Error::invalid_argument && !negative && negative.error() == Error::invalid_argument &&
		!not_a_number && not_a_number.error() == Error::invalid_argument &&
		!missing_set && missing_set.error() == Error::missing_object && !missing_get && missing_get.error() == Error::missing_object &&
		render.removeObject(*object).has_value();
}

} // namespace

/// @brief Runs the opacity checks, then imports the alpha-mode fixture and compares its render materials.
int main() {
	using namespace vve;
	// Three triangles share one position buffer; each uses one material.
	const auto path = test::writeFixture(VVE_TEST_TMP_DIR, "alpha_modes.gltf", R"({
  "asset": {"version": "2.0", "generator": "vve transparency test"},
  "scene": 0,
  "scenes": [{"nodes": [0, 1, 2]}],
  "nodes": [{"name": "Solid", "mesh": 0}, {"name": "Leaves", "mesh": 1}, {"name": "Glass", "mesh": 2}],
  "materials": [
    {"name": "Solid", "pbrMetallicRoughness": {"baseColorFactor": [1.0, 0.0, 0.0, 1.0]}},
    {"name": "Leaves", "alphaMode": "MASK", "pbrMetallicRoughness": {"baseColorFactor": [0.0, 1.0, 0.0, 1.0]}},
    {"name": "Glass", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [0.0, 0.0, 1.0, 0.5]}}
  ],
  "meshes": [
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 0, "mode": 4}]},
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 1, "mode": 4}]},
    {"primitives": [{"attributes": {"POSITION": 0}, "material": 2, "mode": 4}]}
  ],
  "buffers": [
    {"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA", "byteLength": 36}
  ],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962}],
  "accessors": [
    {"bufferView": 0, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0.0, 0.0, 0.0], "max": [1.0, 1.0, 0.0]}
  ]
})");
	auto engine = test::hiddenEngine("render-transparency-tests", PixelExtent{.width = 64, .height = 64});
	vve::test::configureRenderer(engine.renderSystem());
	auto &render = engine.renderSystem();
	if (!hasOpacityRoundTrip(render)) { return 1; }
	const auto scene = engine.assets().loadScene(path);
	if (!scene) { return 2; }
	if (!render.instantiateScene(*scene)) { return 3; }
	// Materials are keyed by base colour, which tells the three fixture materials apart.
	const auto expect = [&render](Vec3 color, Scalar opacity, simple::AlphaMode mode) {
		const auto &materials = render.renderMaterials();
		const auto found = std::ranges::find_if(materials, [color](const simple::RenderMaterial &material) {
			return material.base_color.value.x == color.x && material.base_color.value.y == color.y && material.base_color.value.z == color.z;
		});
		if (found == materials.end()) { std::println("[RenderTransparencyTests] material missing"); return false; }
		std::println("[RenderTransparencyTests] material rgb={},{},{} opacity={} alpha_mode={}", color.x, color.y, color.z,
			found->opacity, static_cast<int>(found->alpha_mode));
		return std::abs(found->opacity - opacity) < 1.0e-6F && found->alpha_mode == mode;
	};
	const bool opaque = expect(Vec3{1, 0, 0}, 1.0F, simple::AlphaMode::opaque);
	const bool mask = expect(Vec3{0, 1, 0}, 1.0F, simple::AlphaMode::mask);
	const bool blend = expect(Vec3{0, 0, 1}, 0.5F, simple::AlphaMode::blend);
	return opaque && mask && blend ? 0 : 4;
}
