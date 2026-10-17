import std;
import VVEngine;
import VVEngine.Simple;
import VVE.TestSupport;

/// @file
/// @brief Checks source-scene unloading, render-cache coordination and bounded catalog lifetimes without a GPU.

namespace {

/// @brief One imported scene populates every asset table: scene, nodes, mesh, material, light and camera.
constexpr std::string_view sceneFixture = R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [{"type": "point", "intensity": 2.0}]}},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "Root", "children": [1, 2, 3]},
    {"name": "Triangle", "mesh": 0},
    {"name": "Light", "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "Camera", "camera": 0, "translation": [0, 0, 3]}
  ],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": 0, "mode": 4}]}],
  "materials": [{"name": "Red", "pbrMetallicRoughness": {"baseColorFactor": [1, 0, 0, 1]}}],
  "cameras": [{"type": "perspective", "perspective": {"yfov": 0.7, "znear": 0.1, "zfar": 50.0}}],
  "buffers": [{"uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA", "byteLength": 36}],
  "bufferViews": [{"buffer": 0, "byteLength": 36, "target": 34962}],
  "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
                 "min": [0, 0, 0], "max": [1, 1, 0]}]
})";

/// @brief Include lights, cameras and copied nodes so removal exercises their instance ownership too.
constexpr vve::SceneInstantiationOptions importOptions{.apply_cameras = true, .apply_lights = true, .keep_hierarchy = true};

/// @brief Requires the documented error instead of accepting any failure.
template <typename T> bool failsWith(const std::expected<T, vve::Error> &result, vve::Error error) {
	return !result && result.error() == error;
}

/// @brief Checks every saved handle through its public query against the required resolution state.
template <typename THandles, typename TQuery> bool handlesResolve(const THandles &handles, TQuery query, bool expected) {
	// Missing descriptors must report missing_object; surviving scenes must still resolve every handle.
	return !handles.empty() && std::ranges::all_of(handles, [&](auto handle) {
		const auto result = query(handle);
		return expected ? result.has_value() : failsWith(result, vve::Error::missing_object);
	});
}

/// @brief Exercises public unload semantics with surviving objects and an unrelated live scene.
bool facadeUnload(const std::filesystem::path &path) {
	auto engine = vve::EngineBuilder<>{}.applicationName("asset-scene-unload-tests").build();
	auto world = engine.world();
	auto assets = world.get<vve::AssetSystem>();
	auto render = world.get<vve::RenderSystem>();
	const auto survivor_scene = assets.loadScene(path);
	const auto scene = assets.loadScene(path);
	const auto primitive = render.addPlane(vve::Vec2{1, 1}, vve::LinearColor{});
	if (!survivor_scene || !scene || !primitive || *scene == *survivor_scene) { return false; }
	const auto survivor = render.instantiateScene(*survivor_scene, importOptions);
	if (!survivor) { return false; }
	const auto survivor_objects = render.sceneInstanceObjects(*survivor);
	const auto survivor_nodes = assets.sceneNodes(*survivor_scene).value_or(vve::Vector<vve::NodeHandle>{});
	const auto survivor_meshes = assets.sceneMeshes(*survivor_scene).value_or(vve::Vector<vve::MeshHandle>{});
	const auto survivor_materials = assets.sceneMaterials(*survivor_scene).value_or(vve::Vector<vve::MaterialHandle>{});
	const auto survivor_lights = assets.sceneLights(*survivor_scene).value_or(vve::Vector<vve::LightHandle>{});
	const auto survivor_cameras = assets.sceneCameras(*survivor_scene).value_or(vve::Vector<vve::CameraHandle>{});
	const auto mesh_baseline = render.sceneMeshCount();
	const auto material_baseline = render.sceneMaterialCount();
	const auto texture_baseline = render.sceneTextureCount();
	const auto object_baseline = render.sceneInstanceCount();
	const auto first = render.instantiateScene(*scene, importOptions);
	const auto second = render.instantiateScene(*scene, importOptions);
	const auto nodes = assets.sceneNodes(*scene);
	const auto meshes = assets.sceneMeshes(*scene);
	const auto materials = assets.sceneMaterials(*scene);
	const auto lights = assets.sceneLights(*scene);
	const auto cameras = assets.sceneCameras(*scene);
	if (!first || !second || !nodes || nodes->empty() || !meshes || meshes->empty() || !materials || materials->empty() ||
		!lights || lights->size() != 1 || !cameras || cameras->size() != 1 || !survivor_objects || survivor_objects->empty()) {
		return false;
	}

	// Every live instance blocks unloading, and a failed request must leave all source and render data intact.
	if (!failsWith(assets.unloadScene(*scene), vve::Error::invalid_argument) || !assets.containsScene(*scene) ||
		!render.sceneInstanceObjects(*first) || !render.sceneInstanceObjects(*second) ||
		!assets.meshPositions(meshes->front()) || !render.removeSceneInstance(*first) ||
		!failsWith(assets.unloadScene(*scene), vve::Error::invalid_argument) || !render.removeSceneInstance(*second)) {
		return false;
	}
	const bool cached_before = render.sceneMeshCount() > mesh_baseline;
	if (!cached_before || !assets.unloadScene(*scene)) { return false; }

	// Source handles of every category become stale, while other scenes and live resources remain usable.
	const bool stale = !assets.containsScene(*scene) && failsWith(assets.sceneName(*scene), vve::Error::missing_object) &&
		handlesResolve(*nodes, [&](auto handle) { return assets.nodeName(handle); }, false) &&
		handlesResolve(*meshes, [&](auto handle) { return assets.meshPositions(handle); }, false) &&
		handlesResolve(*materials, [&](auto handle) { return assets.materialName(handle); }, false) &&
		handlesResolve(*lights, [&](auto handle) { return assets.lightData(handle); }, false) &&
		handlesResolve(*cameras, [&](auto handle) { return assets.cameraData(handle); }, false);
	const bool preserved = assets.containsScene(*survivor_scene) && assets.sceneNodes(*survivor_scene) &&
		handlesResolve(survivor_nodes, [&](auto handle) { return assets.nodeName(handle); }, true) &&
		handlesResolve(survivor_meshes, [&](auto handle) { return assets.meshPositions(handle); }, true) &&
		handlesResolve(survivor_materials, [&](auto handle) { return assets.materialName(handle); }, true) &&
		handlesResolve(survivor_lights, [&](auto handle) { return assets.lightData(handle); }, true) &&
		handlesResolve(survivor_cameras, [&](auto handle) { return assets.cameraData(handle); }, true) &&
		render.sceneInstanceObjects(*survivor) == survivor_objects && render.objectTransform(*primitive) &&
		render.sceneMeshCount() == mesh_baseline && render.sceneMaterialCount() == material_baseline &&
		render.sceneTextureCount() == texture_baseline && render.sceneInstanceCount() == object_baseline &&
		render.scenePointLightCount() == 1 && render.sceneCameraCount() == 1;
	if (!stale || !preserved || !failsWith(assets.unloadScene(*scene), vve::Error::missing_object) ||
		!failsWith(assets.unloadScene(vve::SceneHandle{}), vve::Error::missing_object) ||
		!failsWith(render.instantiateScene(*scene), vve::Error::missing_object)) { return false; }

	// Explicit render-cache removal keeps source assets available until the separate unload operation.
	if (!render.removeSceneInstance(*survivor) || !render.removeScene(*survivor_scene) ||
		!assets.containsScene(*survivor_scene) || !assets.sceneMeshes(*survivor_scene) ||
		!assets.unloadScene(*survivor_scene) || !render.objectTransform(*primitive) ||
		render.sceneMeshCount() != 1 || render.sceneMaterialCount() != 1) { return false; }
	std::println("[asset_unload_facade] live_rejections=2 all_handles_stale={} unrelated_preserved={} render_caches_released={}",
		stale, preserved, cached_before);
	return true;
}

/// @brief Repeated level changes return all six catalog tables and render caches to their surviving-scene baseline.
bool repeatedCycles(const std::filesystem::path &path) {
	vve::simple::Engine engine{};
	auto &assets = engine.assets();
	auto &render = engine.renderSystem();
	const auto survivor_scene = assets.loadScene(path);
	const auto primitive = render.addPlane(vve::Vec2{1, 1}, vve::LinearColor{});
	if (!survivor_scene || !primitive) { return false; }
	const auto survivor = render.instantiateScene(*survivor_scene, importOptions);
	if (!survivor) { return false; }
	const auto baseline = assets.catalogEntryCount();
	const auto mesh_baseline = render.sceneMeshCount();
	const auto material_baseline = render.sceneMaterialCount();
	auto stale_scenes = vve::Vector<vve::SceneHandle>{};
	// Distinct level paths and fresh handles must not accumulate retained source descriptors or unused render caches.
	for (const auto cycle : std::views::iota(0, 8)) {
		const auto level_path = vve::test::writeFixture(VVE_TEST_TMP_DIR, std::format("level_{}.gltf", cycle), sceneFixture);
		const auto scene = assets.loadScene(level_path);
		if (!scene || assets.catalogEntryCount() <= baseline) { return false; }
		const auto entries = 1U + assets.sceneNodeCount(*scene).value_or(0) + assets.sceneMeshCount(*scene).value_or(0) +
			assets.sceneMaterialCount(*scene).value_or(0) + assets.sceneLightCount(*scene).value_or(0) +
			assets.sceneCameraCount(*scene).value_or(0);
		if (assets.catalogEntryCount() != baseline + entries) { return false; }
		const auto instance = render.instantiateScene(*scene, importOptions);
		if (!instance || !failsWith(assets.unloadScene(*scene), vve::Error::invalid_argument) ||
			!render.removeSceneInstance(*instance) || !assets.unloadScene(*scene)) { return false; }
		stale_scenes.push_back(*scene);
		const bool bounded = assets.catalogEntryCount() == baseline && render.sceneMeshCount() == mesh_baseline &&
			render.sceneMaterialCount() == material_baseline && render.scenePointLightCount() == 1 &&
			render.sceneCameraCount() == 1 && render.objectTransform(*primitive) && render.sceneInstanceObjects(*survivor);
		std::println("[asset_unload_cycle] cycle={} imported_entries={} baseline={} remaining={} bounded={}",
			cycle, entries, baseline, assets.catalogEntryCount(), bounded);
		if (!bounded || !std::ranges::all_of(stale_scenes, [&](auto handle) { return !assets.containsScene(handle); })) {
			return false;
		}
	}
	if (!render.removeSceneInstance(*survivor) || !assets.unloadScene(*survivor_scene) ||
		!render.removeObject(*primitive)) { return false; }
	// Empty user-created scenes own no nodes or resources and follow the same unload/stale-handle contract.
	const auto empty = assets.addScene(vve::ObjectName{.value = "empty"});
	if (!empty || assets.sceneNodeCount(*empty) != 0U || assets.catalogEntryCount() != 1U ||
		!assets.unloadScene(*empty) || !failsWith(assets.unloadScene(*empty), vve::Error::missing_object)) { return false; }
	std::println("[asset_unload_final] catalog_entries={} render_meshes={} render_materials={}",
		assets.catalogEntryCount(), render.sceneMeshCount(), render.sceneMaterialCount());
	return assets.catalogEntryCount() == 0 && render.sceneMeshCount() == 0 && render.sceneMaterialCount() == 0;
}

} // namespace

/// @brief Runs the facade lifetime contract and internal catalog-growth regression with deterministic fixtures.
int main() {
	const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "scene.gltf", sceneFixture);
	if (!facadeUnload(path)) { std::println("[asset_unload] failed=facade"); return 1; }
	if (!repeatedCycles(path)) { std::println("[asset_unload] failed=cycles"); return 2; }
	return 0;
}
