/**
 * @file
 * @brief Facade test for the world scene tree: one fixed root, keep_hierarchy models below it, nodes and hung objects.
 *
 * Functional objects:
 * - main: builds a small car rig twice, hangs one on a trailer node, hangs a cube on a mount, and checks every move,
 *   reparenting, removal and error case against fixed world values.
 */

import std;

import VVEngine;
import VVE.TestSupport;

namespace {

/// @brief Compares two facade vectors component-wise with a small tolerance.
[[nodiscard]] bool close(vve::Vec3 lhs, vve::Vec3 rhs) {
	constexpr auto epsilon = static_cast<vve::Scalar>(1.0e-4);
	return std::abs(lhs.x - rhs.x) <= epsilon && std::abs(lhs.y - rhs.y) <= epsilon && std::abs(lhs.z - rhs.z) <= epsilon;
}

/// @brief Builds a local transform from a translation with identity rotation and unit scale.
[[nodiscard]] vve::Transform at(vve::Vec3 translation) { return vve::Transform{.translation = {.value = translation}}; }

/// @brief Test context: asset and render systems plus the loaded scene.
struct Rig {
	vve::AssetSystem assets;		///< Asset queries for node names.
	vve::RenderSystem render;		///< System under test.
	vve::SceneHandle scene;			///< Loaded car rig.
};

/// @brief Finds an asset node by name.
[[nodiscard]] vve::NodeHandle assetNode(const Rig &rig, std::string_view name) {
	for (const auto handle : rig.assets.sceneNodes(rig.scene).value_or(vve::Vector<vve::NodeHandle>{})) {
		if (const auto node_name = rig.assets.nodeName(handle); node_name && node_name->value == name) { return handle; }
	}
	return {};
}

/// @brief Finds the render object created from a named asset node in one instance.
[[nodiscard]] vve::RenderObjectHandle object(const Rig &rig, vve::RenderSceneInstanceHandle instance, std::string_view name) {
	const auto wanted = assetNode(rig, name);
	for (const auto handle : rig.render.sceneInstanceObjects(instance).value_or(vve::Vector<vve::RenderObjectHandle>{})) {
		if (rig.render.objectSourceNode(handle) == wanted) { return handle; }
	}
	return {};
}

/// @brief Checks one object's world position and prints a diagnostic line on mismatch.
[[nodiscard]] bool expectAt(const vve::RenderSystem &render, vve::RenderObjectHandle handle, std::string_view step, vve::Vec3 expected) {
	const auto actual = render.objectTransform(handle);
	if (actual && close(actual->translation.value, expected)) { return true; }
	std::cerr << "[SceneTreeTests] step=" << step;
	if (actual) {
		const auto t = actual->translation.value;
		std::cerr << " actual=(" << t.x << ',' << t.y << ',' << t.z << ") expected=(" << expected.x << ',' << expected.y << ','
				  << expected.z << ")\n";
	} else {
		std::cerr << " error=" << vve::errorName(actual.error()) << '\n';
	}
	return false;
}

/// @brief True when a call failed with exactly the expected error.
template <typename T> [[nodiscard]] bool failsWith(const std::expected<T, vve::Error> &result, vve::Error error) {
	return !result && result.error() == error;
}

} // namespace

/// @brief Proves the world tree: fixed root, model copies below it, reparenting, hung objects and removal.
int main() {
	// Car (no mesh) -> Body (mesh) and Mount (no mesh) -> Wheel (mesh), as in SceneHierarchyTests.
	const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_tree_test.gltf", R"({
  "asset": {"version": "2.0", "generator": "vve scene tree test"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "Car", "children": [1, 2]},
    {"name": "Body", "translation": [0.0, 1.0, 0.0], "mesh": 0},
    {"name": "Mount", "translation": [2.0, 0.0, 0.0], "children": [3]},
    {"name": "Wheel", "translation": [0.0, 0.0, 1.0], "mesh": 0}
  ],
  "meshes": [
    {"name": "TriangleMesh", "primitives": [{"attributes": {"POSITION": 0}, "mode": 4}]}
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

	auto engine = vve::EngineBuilder<>{}.applicationName("scene-tree-tests").build();
	auto world = engine.world();
	const auto scene = world.get<vve::AssetSystem>().loadScene(path);
	if (!scene) { return 1; }
	const auto rig = Rig{.assets = world.get<vve::AssetSystem>(), .render = world.get<vve::RenderSystem>(), .scene = *scene};
	auto render = rig.render;

	// The world root exists from the start, has no parent and can neither move nor be removed.
	const auto root = render.sceneRoot();
	if (!root.valid() || render.nodeParent(root) != vve::SceneNodeHandle{}) { return 2; }
	if (!failsWith(render.setNodeTransform(root, at({1.0F, 0.0F, 0.0F})), vve::Error::invalid_argument) ||
		!failsWith(render.removeNode(root), vve::Error::invalid_argument)) { return 3; }

	// A model without keep_hierarchy stays loose objects; the tree calls report that.
	const auto flat = render.instantiateScene(*scene);
	if (!flat || !failsWith(render.sceneInstanceRoot(*flat), vve::Error::invalid_argument)) { return 4; }

	// Two copies with keep_hierarchy: each gets its own instance root under the world root.
	const auto car_a = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
	const auto car_b = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
	if (!car_a || !car_b) { return 5; }
	const auto root_a = render.sceneInstanceRoot(*car_a);
	const auto root_b = render.sceneInstanceRoot(*car_b);
	if (!root_a || !root_b || *root_a == *root_b || render.nodeParent(*root_a) != root ||
		render.nodeChildren(root) != vve::Vector<vve::SceneNodeHandle>{*root_a, *root_b}) { return 6; }
	const auto mount_a = render.sceneInstanceNode(*car_a, assetNode(rig, "Mount"));
	const auto mount_b = render.sceneInstanceNode(*car_b, assetNode(rig, "Mount"));
	if (!mount_a || !mount_b || *mount_a == *mount_b) { return 7; }
	if (!expectAt(render, object(rig, *car_a, "Wheel"), "initial", {2.0F, 0.0F, 1.0F})) { return 8; }

	// Moving one copy's instance root moves only that copy.
	if (!render.setNodeTransform(*root_b, at({0.0F, 0.0F, 10.0F}))) { return 9; }
	if (!expectAt(render, object(rig, *car_b, "Body"), "move copy b", {0.0F, 1.0F, 10.0F}) ||
		!expectAt(render, object(rig, *car_a, "Body"), "copy a unchanged", {0.0F, 1.0F, 0.0F})) { return 10; }

	// Car on a trailer: the whole copy follows the trailer node; its local transforms stay as they were.
	const auto trailer = render.createNode(root, at({100.0F, 0.0F, 0.0F}));
	if (!trailer || !render.setNodeParent(*root_a, *trailer)) { return 11; }
	if (!expectAt(render, object(rig, *car_a, "Body"), "on trailer", {100.0F, 1.0F, 0.0F})) { return 12; }
	if (!render.setNodeTransform(*trailer, at({200.0F, 0.0F, 0.0F}))) { return 13; }
	if (!expectAt(render, object(rig, *car_a, "Wheel"), "trailer moved", {202.0F, 0.0F, 1.0F})) { return 14; }
	if (render.nodeChildren(*trailer) != vve::Vector<vve::SceneNodeHandle>{*root_a}) { return 15; }

	// Loops and unknown nodes are rejected.
	const auto wheel_a = render.sceneInstanceNode(*car_a, assetNode(rig, "Wheel"));
	if (!wheel_a || !failsWith(render.setNodeParent(*trailer, *wheel_a), vve::Error::invalid_argument) ||
		!failsWith(render.setNodeTransform(vve::SceneNodeHandle{}, at({})), vve::Error::missing_object) ||
		!failsWith(render.createNode(vve::SceneNodeHandle{}), vve::Error::missing_object)) { return 16; }

	// A primitive hung on a mount: its own transform becomes its offset and it follows the mount.
	const auto cube = render.addCuboid(vve::Vec3{-0.1F, -0.1F, -0.1F}, vve::Vec3{0.1F, 0.1F, 0.1F},
		vve::LinearColor{.value = vve::Vec3{1.0F, 0.0F, 0.0F}}, at({0.0F, 1.0F, 0.0F}));
	if (!cube || !render.setObjectParent(*cube, *mount_a)) { return 17; }
	if (!expectAt(render, *cube, "cube hung", {202.0F, 1.0F, 0.0F})) { return 18; }
	if (!render.setNodeTransform(*mount_a, at({3.0F, 0.0F, 0.0F}))) { return 19; }
	if (!expectAt(render, *cube, "cube follows mount", {203.0F, 1.0F, 0.0F}) ||
		!expectAt(render, object(rig, *car_a, "Wheel"), "wheel follows mount", {203.0F, 0.0F, 1.0F})) { return 20; }

	// Taken off the tree, the cube stays where it is and no longer follows.
	if (!render.setObjectParent(*cube, vve::SceneNodeHandle{}) || !render.setNodeTransform(*mount_a, at({2.0F, 0.0F, 0.0F}))) { return 21; }
	if (!expectAt(render, *cube, "cube detached", {203.0F, 1.0F, 0.0F})) { return 22; }

	// The old instance calls still work on the tree.
	if (!render.setSceneInstanceNodeTransform(*car_a, assetNode(rig, "Car"), at({0.0F, 5.0F, 0.0F}))) { return 23; }
	if (!expectAt(render, object(rig, *car_a, "Body"), "old call", {200.0F, 6.0F, 0.0F})) { return 24; }

	// Removing a node removes everything below it, including its render objects.
	const auto wheel_object = object(rig, *car_a, "Wheel");
	if (!render.removeNode(*mount_a)) { return 25; }
	if (!failsWith(render.objectTransform(wheel_object), vve::Error::missing_object) ||
		!failsWith(render.sceneInstanceNode(*car_a, assetNode(rig, "Wheel")), vve::Error::missing_object) ||
		render.sceneInstanceObjects(*car_a).value_or(vve::Vector<vve::RenderObjectHandle>{}).size() != 1U) { return 26; }

	// Removing the instance removes its subtree wherever it hangs; the trailer stays.
	if (!render.removeSceneInstance(*car_a)) { return 27; }
	if (!failsWith(render.nodeParent(*root_a), vve::Error::missing_object) || render.nodeChildren(*trailer) != vve::Vector<vve::SceneNodeHandle>{}) {
		return 28;
	}

	// clearScene empties the tree but keeps the same world root.
	render.clearScene();
	if (render.sceneRoot() != root || render.nodeChildren(root) != vve::Vector<vve::SceneNodeHandle>{}) { return 29; }

	std::println("[SceneTreeTests] PASS");
	return 0;
}
