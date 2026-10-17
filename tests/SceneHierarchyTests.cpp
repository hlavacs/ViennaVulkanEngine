/**
 * @file
 * @brief Facade test for scene instances that keep their node hierarchy (SceneInstantiationOptions::keep_hierarchy).
 *
 * Functional objects:
 * - main: instantiates a small car rig with and without the option and checks every node move against fixed world values.
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

/// @brief Compares rotations by absolute dot product so opposite quaternion signs count as equal.
[[nodiscard]] bool close(vve::Quat lhs, vve::Quat rhs) {
	const auto dot = lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	return std::abs(dot) > vve::one() - static_cast<vve::Scalar>(1.0e-4);
}

/// @brief Quarter and half turns about +Y; the Quat constructor order is w, x, y, z.
const auto quarterTurn = vve::Quat{0.7071068F, 0.0F, 0.7071068F, 0.0F};
const auto halfTurn = vve::Quat{0.0F, 0.0F, 1.0F, 0.0F};

/// @brief Builds a local transform from translation and rotation with unit scale.
[[nodiscard]] vve::Transform local(vve::Vec3 translation, vve::Quat rotation = vve::identityQuat()) {
	return vve::Transform{.translation = {.value = translation}, .rotation = {.value = rotation}};
}

/// @brief Test context: asset and render systems plus the loaded scene.
struct Rig {
	vve::AssetSystem assets;		///< Asset queries for node names.
	vve::RenderSystem render;		///< System under test.
	vve::SceneHandle scene;			///< Loaded car rig.
};

/// @brief Finds an asset node by name.
[[nodiscard]] vve::NodeHandle node(const Rig &rig, std::string_view name) {
	for (const auto handle : rig.assets.sceneNodes(rig.scene).value_or(vve::Vector<vve::NodeHandle>{})) {
		if (const auto node_name = rig.assets.nodeName(handle); node_name && node_name->value == name) { return handle; }
	}
	return {};
}

/// @brief Finds the render object created from a named node in one instance.
[[nodiscard]] vve::RenderObjectHandle object(const Rig &rig, vve::RenderSceneInstanceHandle instance, std::string_view name) {
	const auto wanted = node(rig, name);
	for (const auto handle : rig.render.sceneInstanceObjects(instance).value_or(vve::Vector<vve::RenderObjectHandle>{})) {
		if (rig.render.objectSourceNode(handle) == wanted) { return handle; }
	}
	return {};
}

/// @brief Checks one object's world translation and rotation and prints a diagnostic line on mismatch.
[[nodiscard]] bool expectWorld(const Rig &rig, vve::RenderSceneInstanceHandle instance, std::string_view name,
							   std::string_view step, vve::Vec3 translation, vve::Quat rotation = vve::identityQuat()) {
	const auto actual = rig.render.objectTransform(object(rig, instance, name));
	if (actual && close(actual->translation.value, translation) && close(actual->rotation.value, rotation)) { return true; }
	std::cerr << "[SceneHierarchyTests] step=" << step << " node=" << name;
	if (actual) {
		const auto t = actual->translation.value;
		std::cerr << " actual=(" << t.x << ',' << t.y << ',' << t.z << ") expected=(" << translation.x << ',' << translation.y
				  << ',' << translation.z << ")\n";
	} else {
		std::cerr << " error=" << vve::errorName(actual.error()) << '\n';
	}
	return false;
}

} // namespace

/// @brief Proves that moving a node moves every object below it, only in instances created with keep_hierarchy.
int main() {
	// Car (no mesh) -> Body (mesh) and Mount (no mesh) -> Wheel (mesh); every node has a simple known offset.
	const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_scene_hierarchy_test.gltf", R"({
  "asset": {"version": "2.0", "generator": "vve scene hierarchy test"},
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

	auto engine = vve::EngineBuilder<>{}.applicationName("scene-hierarchy-tests").build();
	auto world = engine.world();
	const auto scene = world.get<vve::AssetSystem>().loadScene(path);
	if (!scene) { return 1; }
	const auto rig = Rig{.assets = world.get<vve::AssetSystem>(), .render = world.get<vve::RenderSystem>(), .scene = *scene};
	auto render = rig.render;
	const auto car = node(rig, "Car");
	const auto mount = node(rig, "Mount");
	if (!car.valid() || !mount.valid()) { return 2; }

	// The default stays flat: no node transforms can be set, and its objects never move with another instance.
	const auto flat = render.instantiateScene(*scene);
	const auto rigged = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.keep_hierarchy = true});
	if (!flat || !rigged) { return 3; }
	if (const auto result = render.setSceneInstanceNodeTransform(*flat, car, local({1.0F, 0.0F, 0.0F}));
		result || result.error() != vve::Error::invalid_argument) { return 4; }

	// Instantiation composes the same world transforms with and without the option.
	if (!expectWorld(rig, *rigged, "Body", "initial", {0.0F, 1.0F, 0.0F}) ||
		!expectWorld(rig, *rigged, "Wheel", "initial", {2.0F, 0.0F, 1.0F})) { return 5; }

	// Moving the mesh-free root moves every part of this instance.
	if (!render.setSceneInstanceNodeTransform(*rigged, car, local({10.0F, 0.0F, 0.0F}))) { return 6; }
	if (!expectWorld(rig, *rigged, "Body", "move root", {10.0F, 1.0F, 0.0F}) ||
		!expectWorld(rig, *rigged, "Wheel", "move root", {12.0F, 0.0F, 1.0F})) { return 7; }

	// Turning the mesh-free mount swings the wheel around it and leaves the body alone.
	if (!render.setSceneInstanceNodeTransform(*rigged, mount, local({2.0F, 0.0F, 0.0F}, quarterTurn))) { return 8; }
	if (!expectWorld(rig, *rigged, "Wheel", "turn mount", {13.0F, 0.0F, 0.0F}, quarterTurn) ||
		!expectWorld(rig, *rigged, "Body", "turn mount", {10.0F, 1.0F, 0.0F})) { return 9; }

	// Turning the root afterwards keeps the mount's own turn: the wheel ends at a half turn.
	if (!render.setSceneInstanceNodeTransform(*rigged, car, local({10.0F, 0.0F, 0.0F}, quarterTurn))) { return 10; }
	if (!expectWorld(rig, *rigged, "Body", "turn root", {10.0F, 1.0F, 0.0F}, quarterTurn) ||
		!expectWorld(rig, *rigged, "Wheel", "turn root", {10.0F, 0.0F, -3.0F}, halfTurn)) { return 11; }

	// The flat instance kept its imported placement throughout.
	if (!expectWorld(rig, *flat, "Body", "flat unchanged", {0.0F, 1.0F, 0.0F}) ||
		!expectWorld(rig, *flat, "Wheel", "flat unchanged", {2.0F, 0.0F, 1.0F})) { return 12; }

	// Local transforms read back as set, or as imported for untouched nodes.
	const auto mount_local = render.sceneInstanceNodeTransform(*rigged, mount);
	const auto wheel_local = render.sceneInstanceNodeTransform(*rigged, node(rig, "Wheel"));
	if (!mount_local || !close(mount_local->rotation.value, quarterTurn) ||
		!wheel_local || !close(wheel_local->translation.value, {0.0F, 0.0F, 1.0F})) { return 13; }

	// A direct world placement holds until its node or an ancestor moves again.
	const auto body = object(rig, *rigged, "Body");
	if (!render.setObjectTransform(body, local({50.0F, 50.0F, 50.0F}))) { return 14; }
	if (!expectWorld(rig, *rigged, "Body", "direct placement", {50.0F, 50.0F, 50.0F})) { return 15; }
	if (!render.setSceneInstanceNodeTransform(*rigged, car, local({0.0F, 0.0F, 0.0F}))) { return 16; }
	if (!expectWorld(rig, *rigged, "Body", "root after direct placement", {0.0F, 1.0F, 0.0F})) { return 17; }

	// Unknown instances and nodes outside the instance's scene are rejected.
	if (const auto result = render.setSceneInstanceNodeTransform(vve::RenderSceneInstanceHandle{}, car, local({}));
		result || result.error() != vve::Error::missing_object) { return 18; }
	if (const auto result = render.sceneInstanceNodeTransform(*rigged, vve::NodeHandle{});
		result || result.error() != vve::Error::missing_object) { return 19; }

	// Removing a part leaves the rest of the hierarchy working.
	if (!render.removeObject(object(rig, *rigged, "Wheel"))) { return 20; }
	if (!render.setSceneInstanceNodeTransform(*rigged, mount, local({3.0F, 0.0F, 0.0F}))) { return 21; }
	if (!render.setSceneInstanceNodeTransform(*rigged, car, local({1.0F, 0.0F, 0.0F}))) { return 22; }
	if (!expectWorld(rig, *rigged, "Body", "after part removal", {1.0F, 1.0F, 0.0F})) { return 23; }

	// An interior subtree moved outside its instance root remains owned by that instance.
	const auto mount_copy = render.sceneInstanceNode(*rigged, mount);
	const auto wheel_copy = render.sceneInstanceNode(*rigged, node(rig, "Wheel"));
	const auto instance_root = render.sceneInstanceRoot(*rigged);
	if (!mount_copy || !wheel_copy || !instance_root || !render.setNodeParent(*mount_copy, render.sceneRoot())) { return 24; }
	if (render.nodeParent(*wheel_copy) != *mount_copy) { return 25; }
	// Removing the instance removes both the original root and the moved interior subtree.
	if (!render.removeSceneInstance(*rigged)) { return 26; }
	if (const auto result = render.setSceneInstanceNodeTransform(*rigged, car, local({}));
		result || result.error() != vve::Error::missing_object) { return 27; }
	for (const auto removed : {*instance_root, *mount_copy, *wheel_copy}) {
		const auto result = render.nodeTransform(removed);
		if (result || result.error() != vve::Error::missing_object) { return 28; }
	}
	std::println("[SceneHierarchyTests] moved_subtree_removed=1 owned_nodes_checked=3");

	std::println("[SceneHierarchyTests] PASS");
	return 0;
}
