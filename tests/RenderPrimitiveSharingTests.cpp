/// @file
/// @brief CPU regressions for shared primitive resources, object removal and imported default materials.
import std;
import VVEngine;
import VVEngine.Simple;

/// @brief Builds three imported meshes without materials, using deterministic asset callbacks only.
[[nodiscard]] auto materiallessAssets() -> vve::simple::ImportedAssetReadAccess {
	using namespace vve;
	// Shared vertices and static indices outlive every imported render mesh.
	static const auto vertices = std::make_shared<std::vector<vve::simple::RenderVertex>>(
		std::initializer_list<vve::simple::RenderVertex>{{.position = {0, 0, 0}}, {.position = {1, 0, 0}}, {.position = {0, 1, 0}}});
	static constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
	const auto node = makeCounterHandle<NodeHandle>();
	const auto meshes = Vector<MeshHandle>{makeCounterHandle<MeshHandle>(), makeCounterHandle<MeshHandle>(),
		makeCounterHandle<MeshHandle>()};
	return {
		.scene_nodes = [node](SceneHandle) { return Vector<NodeHandle>{node}; },
		.scene_root_node = [node](SceneHandle) { return node; },
		.scene_node_children = [](SceneHandle, NodeHandle) { return Vector<NodeHandle>{}; },
		.node_transform = [](NodeHandle) { return Transform{}; },
		.node_meshes = [meshes](NodeHandle) { return meshes; },
		.mesh_material = [](MeshHandle) { return MaterialHandle{}; },
		.mesh_geometry = [](MeshHandle) { return std::shared_ptr<const std::vector<vve::simple::RenderVertex>>{vertices}; },
		.mesh_indices = [](MeshHandle) { return std::span{indices}; }
	};
}

/// @brief Checks plane UVs, cache identity and facade resource lifetime without a GPU.
[[nodiscard]] bool hasTexturedPlaneCoverage(vve::RenderSystem &render, const std::filesystem::path &texture) {
	using namespace vve;
	auto scene = simple::RenderScene{};
	// Default and repeated planes retain four corners and the same two triangles.
	for (const auto uv_scale : {Vec2{1, 1}, Vec2{10, 10}}) {
		const auto handle = uv_scale.x == 1 ? scene.addPlaneMesh(Vec2{20, 20}) : scene.addPlaneMesh(Vec2{20, 20}, uv_scale);
		const auto *mesh = scene.findMesh(handle);
		if (!mesh || !mesh->vertices || mesh->vertices->size() != 4U || mesh->indices.size() != 6U) { return false; }
		const auto expected = std::array{Vec2{0, 0}, Vec2{uv_scale.x, 0}, uv_scale, Vec2{0, uv_scale.y}};
		for (std::size_t index{}; index < expected.size(); ++index) {
			const auto &vertex = (*mesh->vertices)[index];
			if (vertex.uv.x != expected[index].x || vertex.uv.y != expected[index].y ||
				std::abs(vertex.position.x) != 20 || vertex.position.y != 0 || std::abs(vertex.position.z) != 20 ||
				vertex.normal.x != 0 || vertex.normal.y != 1 || vertex.normal.z != 0) { return false; }
		}
		if (std::ranges::any_of(mesh->indices, [](auto index) { return index >= 4U; })) { return false; }
		std::println("textured_plane vertices=4 indices=6 uv={}x{}", uv_scale.x, uv_scale.y);
	}
	const auto transform = Transform{.translation = Position{.value = Vec3{1, 2, 3}}};
	const auto repeated = render.addTexturedPlane(Vec2{20, 20}, texture, Vec2{10, 10}, transform);
	const auto repeated_copy = render.addTexturedPlane(Vec2{20, 20}, texture, Vec2{10, 10});
	const auto different_u = render.addTexturedPlane(Vec2{20, 20}, texture, Vec2{9, 10});
	const auto different_v = render.addTexturedPlane(Vec2{20, 20}, texture, Vec2{10, 9});
	const auto unit = render.addTexturedPlane(Vec2{20, 20}, texture);
	const auto plain = render.addPlane(Vec2{20, 20}, LinearColor{.value = Vec3{1, 1, 1}});
	if (!repeated || !repeated_copy || !different_u || !different_v || !unit || !plain ||
		render.sceneMeshCount() != 4U || render.sceneMaterialCount() != 2U || render.sceneTextureCount() != 1U) { return false; }
	const auto stored = render.objectTransform(*repeated);
	if (!stored || stored->translation.value.x != 1 || stored->translation.value.y != 2 ||
		stored->translation.value.z != 3 || !render.removeObject(*plain)) { return false; }
	// Shared geometry survives its first removal; the final object releases texture and material.
	std::size_t index{};
	constexpr std::array<std::size_t, 5> meshes_after_removal{4, 3, 2, 1, 0};
	for (const auto object : {*repeated, *repeated_copy, *different_u, *different_v, *unit}) {
		const auto remaining = index + 1U == meshes_after_removal.size() ? 0U : 1U;
		if (!render.removeObject(object) || render.sceneMeshCount() != meshes_after_removal[index++] ||
			render.sceneMaterialCount() != remaining || render.sceneTextureCount() != remaining) { return false; }
	}
	// Nonfinite UVs must fail before acquiring textures or allocating scene resources.
	for (const auto uv_scale : {Vec2{std::numeric_limits<Scalar>::quiet_NaN(), 1},
		Vec2{1, std::numeric_limits<Scalar>::infinity()}}) {
		const auto invalid = render.addTexturedPlane(Vec2{20, 20}, texture, uv_scale);
		if (invalid || invalid.error() != Error::invalid_argument || render.sceneMeshCount() != 0U ||
			render.sceneMaterialCount() != 0U || render.sceneTextureCount() != 0U) { return false; }
	}
	std::println("textured_plane sharing=ok uv_axes=distinct removal=ok nonfinite=rejected");
	return true;
}

/// @brief Checks sharing and reclamation without initializing windows or the renderer.
int main() {
	using namespace vve;
	std::filesystem::create_directories(VVE_TEST_TMP_DIR);
	const auto texture = std::filesystem::path{VVE_TEST_TMP_DIR} / "white.ppm";
	// A single white texel keeps the texture fixture independent of repository assets.
	{
		std::ofstream file{texture, std::ios::binary};
		file << "P6\n1 1\n255\n" << "\xff\xff\xff";
		if (!file) { return 1; }
	}
	auto engine = Engine<>{};
	auto world = engine.world();
	auto &render = world.get<RenderSystem>();
	if (!hasTexturedPlaneCoverage(render, texture)) { return 20; }
	const Vec3 minimum{-0.5F, -0.5F, -0.5F}, maximum{0.5F, 0.5F, 0.5F};
	auto objects = Vector<RenderObjectHandle>{};
	// Placement belongs to instances; identical local geometry and materials are shared.
	for (const auto index : std::views::iota(0, 100)) {
		const auto transform = Transform{.translation = Position{.value = Vec3{static_cast<float>(index), 0, 0}}};
		const auto object = render.addTexturedCuboid(minimum, maximum, texture, transform);
		if (!object) { return 2; }
		objects.push_back(*object);
		const auto stored = render.objectTransform(*object);
		if (!stored || stored->translation.value.x != transform.translation.value.x) { return 3; }
	}
	std::println("shared objects={} meshes={} materials={} textures={}", objects.size(),
		render.sceneMeshCount(), render.sceneMaterialCount(), render.sceneTextureCount());
	if (render.sceneMeshCount() != 1U || render.sceneMaterialCount() != 1U || render.sceneTextureCount() != 1U) { return 4; }
	// Every intermediate removal must preserve resources for the remaining objects.
	for (const auto object : objects) {
		if (!render.removeObject(object)) { return 5; }
		const auto remaining = object == objects.back() ? 0U : 1U;
		if (render.sceneMeshCount() != remaining || render.sceneMaterialCount() != remaining ||
			render.sceneTextureCount() != remaining) { return 6; }
	}
	std::println("removed meshes={} materials={} textures={}", render.sceneMeshCount(),
		render.sceneMaterialCount(), render.sceneTextureCount());
	const auto recreated = render.addTexturedCuboid(minimum, maximum, texture);
	if (!recreated || render.sceneMeshCount() != 1U || render.sceneMaterialCount() != 1U ||
		render.sceneTextureCount() != 1U || !render.removeObject(*recreated)) { return 7; }

	// Shapes and extents distinguish meshes; all untextured shapes can share one colour material.
	const auto white = LinearColor{.value = Vec3{1, 1, 1}};
	const auto plane = render.addPlane(Vec2{1, 1}, white);
	const auto plane_copy = render.addPlane(Vec2{1, 1}, white);
	const auto wide_plane = render.addPlane(Vec2{2, 1}, white);
	const auto cuboid = render.addCuboid(minimum, maximum, white);
	const auto red_cuboid = render.addCuboid(minimum, maximum, LinearColor{.value = Vec3{1, 0, 0}});
	const auto textured = render.addTexturedCuboid(minimum, maximum, texture);
	const auto triangle = render.addTriangleMesh(Vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}, {0, 1, 2}, white);
	if (!plane || !plane_copy || !wide_plane || !cuboid || !red_cuboid || !textured || !triangle ||
		render.sceneMeshCount() != 4U || render.sceneMaterialCount() != 3U || render.sceneTextureCount() != 1U) { return 8; }
	if (!render.removeObject(*textured) || render.sceneTextureCount() != 0U || render.sceneMeshCount() != 4U ||
		render.sceneMaterialCount() != 2U || !render.objectVisible(*cuboid).value_or(false)) { return 9; }
	// Editing a shared mesh clones it; editing a sole cached primitive invalidates that cache entry.
	const auto deformed = Vector<Vec3>(24U, Vec3{2, 3, 4});
	if (!render.setObjectMeshPositions(*cuboid, deformed) || render.sceneMeshCount() != 5U ||
		!render.setObjectMeshPositions(*red_cuboid, deformed)) { return 10; }
	const auto fresh = render.addCuboid(minimum, maximum, white);
	if (!fresh || render.sceneMeshCount() != 6U) { return 11; }
	// Clear invalidates primitive caches as well as all their resources.
	render.clearScene();
	const auto after_clear = render.addCuboid(minimum, maximum, white);
	if (!after_clear || render.sceneMeshCount() != 1U || render.sceneMaterialCount() != 1U) { return 12; }

	auto imported = vve::simple::RenderSystem{materiallessAssets()};
	const auto scene = makeCounterHandle<SceneHandle>();
	const auto first = imported.instantiateScene(scene), second = imported.instantiateScene(scene);
	std::println("materialless meshes={} materials={} objects={}", imported.sceneMeshCount(),
		imported.sceneMaterialCount(), imported.sceneInstanceCount());
	if (!first || !second || imported.sceneInstanceCount() != 6U || imported.sceneMeshCount() != 3U ||
		imported.sceneMaterialCount() != 1U) { return 13; }
	// Cached imported resources survive removal and remain usable for another instantiation.
	if (!imported.removeSceneInstance(*first) || !imported.removeSceneInstance(*second) ||
		imported.sceneInstanceCount() != 0U || imported.sceneMeshCount() != 3U || imported.sceneMaterialCount() != 1U ||
		!imported.instantiateScene(scene) || imported.sceneMaterialCount() != 1U) { return 14; }
	imported.clearScene();
	const auto before_purge = imported.instantiateScene(scene);
	if (!before_purge || !imported.removeSceneInstance(*before_purge) || imported.purgeUnusedAssets() != 4U ||
		!imported.instantiateScene(scene) || imported.sceneMeshCount() != 3U || imported.sceneMaterialCount() != 1U) { return 15; }
	// Removing the middle primitive compacts all three indexed vectors and releases its texture.
	render.clearScene();
	const auto left = render.addPlane(Vec2{1, 1}, white);
	const auto middle = render.addTexturedCuboid(minimum, maximum, texture);
	const auto right = render.addCuboid(Vec3{-1, -1, -1}, Vec3{1, 1, 1}, LinearColor{.value = Vec3{1, 0, 0}});
	if (!left || !middle || !right || render.sceneMeshCount() != 3U || render.sceneMaterialCount() != 3U) { return 16; }
	if (!render.removeObject(*middle) || render.objectTransform(*middle) ||
		render.sceneMeshCount() != 2U || render.sceneMaterialCount() != 2U || render.sceneTextureCount() != 0U ||
		!render.setObjectVisible(*right, false) || render.objectVisible(*right).value_or(true) ||
		!render.setObjectMeshPositions(*right, deformed) || !render.objectTransform(*left)) { return 17; }
	const auto reused_middle = render.addTexturedCuboid(minimum, maximum, texture);
	if (!reused_middle || render.sceneMeshCount() != 3U || render.sceneMaterialCount() != 3U ||
		render.sceneTextureCount() != 1U || !render.removeObject(*left) ||
		!render.objectTransform(*right) || !render.objectTransform(*reused_middle)) { return 18; }
	render.clearScene();
	if (render.objectTransform(*right) || render.objectTransform(*reused_middle) ||
		!render.addTexturedCuboid(minimum, maximum, texture) || render.sceneMeshCount() != 1U ||
		render.sceneMaterialCount() != 1U || render.sceneTextureCount() != 1U) { return 19; }
	std::println("D14 primitive middle_erase=ok slot_reuse=ok clear=ok");
	std::println("RenderPrimitiveSharingTests passed");
	return 0;
}
