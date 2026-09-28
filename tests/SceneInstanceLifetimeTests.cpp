/**
 * @file
 * @brief Facade-only lifetime checks for partial object removal, imported lights/cameras and clearScene.
 * Fixtures contain two OBJ objects and a glTF point light/camera; no engine initialization or GPU is needed.
 */
import std;
import VEEngine;

namespace {

/// @brief Writes deterministic, inspectable assets in this test's build-tree directory.
[[nodiscard]] bool writeFixtures(const std::filesystem::path &directory) {
   std::filesystem::create_directories(directory);
   std::ofstream objects{directory / "objects.obj"};
   objects << "o A\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
           << "o B\nv 2 0 0\nv 3 0 0\nv 2 1 0\nf 4 5 6\n";
   std::ofstream rig{directory / "rig.gltf"};
   rig << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "point", "color": [0.8, 0.7, 0.6], "intensity": 4.0, "range": 9.0}
  ]}},
  "scenes": [{"nodes": [0, 1]}], "scene": 0,
  "nodes": [
    {"name": "Light", "translation": [1, 2, 3], "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "Camera", "camera": 0, "translation": [0, 2, 5]}
  ],
  "cameras": [{"type": "perspective", "perspective": {
    "yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 50.0
  }}]
})";
   return objects.good() && rig.good();
}

/// @brief Requires the precise public error for an invalidated object or scene-instance handle.
template <typename T>
[[nodiscard]] bool missing(const std::expected<T, vve::Error> &result) {
   return !result && result.error() == vve::Error::missing_object;
}

} // namespace

/// @brief Exercises each removal path using only facade calls and reports counts before assertions.
int main() {
   const auto directory = std::filesystem::path{VVE_TEST_TMP_DIR};
   if (!writeFixtures(directory)) { return 1; }
   auto engine = vve::EngineBuilder<>{}.applicationName("scene-instance-lifetime-tests").build();
   auto world = engine.world();
   auto &assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();
   const auto objects_scene = assets.loadScene(directory / "objects.obj");
   const auto rig_scene = assets.loadScene(directory / "rig.gltf");
   std::println("[SceneInstanceLifetimeTests] obj_loaded={} gltf_loaded={}", bool(objects_scene), bool(rig_scene));
   if (!objects_scene || !rig_scene) { return 2; }

   // (a) Removing one object updates the instance's membership; removing the instance still removes its sibling.
   const auto instance = render.instantiateScene(*objects_scene);
   if (!instance) { return 3; }
   const auto objects = render.sceneInstanceObjects(*instance);
   if (!objects || objects->size() != 2U) { return 4; }
   if (!render.removeObject((*objects)[0])) { return 5; }
   const auto remaining = render.sceneInstanceObjects(*instance);
   if (!remaining || remaining->size() != 1U || remaining->front() != (*objects)[1]) { return 6; }
   if (!render.removeSceneInstance(*instance)) { return 7; }
   const bool objects_missing = missing(render.objectVisible((*objects)[0])) && missing(render.objectVisible((*objects)[1]));
   const bool instance_missing = missing(render.sceneInstanceObjects(*instance)) && missing(render.removeSceneInstance(*instance));
   std::println("[SceneInstanceLifetimeTests] partial_removal objects_missing={} instance_missing={}", objects_missing, instance_missing);
   if (!objects_missing) { return 8; }
   if (!instance_missing) { return 9; }

   // (b) Check exact imported counts 2/2 -> 1/1 -> 0/0, then repeat with a pre-existing manual point light.
   const vve::SceneInstantiationOptions options{.apply_cameras = true, .apply_lights = true};
   for (const auto manual_lights : {0U, 1U}) {
      if (manual_lights != 0U) { render.addPointLight(vve::PointLight{}); }
      const auto first = render.instantiateScene(*rig_scene, options);
      const auto second = render.instantiateScene(*rig_scene, options);
      if (!first || !second) { return 10; }
      std::println("[SceneInstanceLifetimeTests] manual={} points={} cameras={} expected_imported=2/2",
                   manual_lights, render.scenePointLightCount(), render.sceneCameraCount());
      if (render.scenePointLightCount() != manual_lights + 2U || render.sceneCameraCount() != 2U) { return 11; }
      if (!render.removeSceneInstance(*first)) { return 12; }
      std::println("[SceneInstanceLifetimeTests] manual={} points={} cameras={} expected_imported=1/1",
                   manual_lights, render.scenePointLightCount(), render.sceneCameraCount());
      if (render.scenePointLightCount() != manual_lights + 1U || render.sceneCameraCount() != 1U) { return 13; }
      if (!render.sceneInstanceObjects(*second) || !render.removeSceneInstance(*second)) { return 14; }
      std::println("[SceneInstanceLifetimeTests] manual={} points={} cameras={} expected_imported=0/0",
                   manual_lights, render.scenePointLightCount(), render.sceneCameraCount());
      if (render.scenePointLightCount() != manual_lights || render.sceneCameraCount() != 0U) { return 15; }
   }

   // (c) Clearing a live geometry instance and a light/camera instance invalidates handles and all render resources.
   const auto geometry = render.instantiateScene(*objects_scene);
   const auto rig = render.instantiateScene(*rig_scene, options);
   if (!geometry || !rig) { return 16; }
   const auto live_objects = render.sceneInstanceObjects(*geometry);
   const auto first_mesh_count = render.sceneMeshCount();
   if (!live_objects || live_objects->size() != 2U || first_mesh_count != 2U || render.sceneMaterialCount() == 0U ||
       render.scenePointLightCount() != 2U || render.sceneCameraCount() != 1U) { return 17; }
   render.clearScene();
   if (!missing(render.removeSceneInstance(*geometry)) || !missing(render.removeSceneInstance(*rig)) ||
       !missing(render.sceneInstanceObjects(*geometry)) || !missing(render.sceneInstanceObjects(*rig))) { return 18; }
   // Every object handle saved before clearScene must now fail with missing_object.
   for (const auto object : *live_objects) {
      if (!missing(render.objectVisible(object))) { return 19; }
   }
   std::println("[SceneInstanceLifetimeTests] cleared meshes={} materials={} lights={}/{}/{} cameras={} objects={}",
                render.sceneMeshCount(), render.sceneMaterialCount(), render.sceneDirectionalLightCount(),
                render.scenePointLightCount(), render.sceneSpotLightCount(), render.sceneCameraCount(), render.sceneInstanceCount());
   if (render.sceneMeshCount() != 0U || render.sceneMaterialCount() != 0U || render.sceneDirectionalLightCount() != 0U ||
       render.scenePointLightCount() != 0U || render.sceneSpotLightCount() != 0U || render.sceneCameraCount() != 0U ||
       render.sceneInstanceCount() != 0U) { return 20; }
   // Asset ownership survives clearScene, so the same source scene can recreate its original mesh count.
   const auto reinstantiated = render.instantiateScene(*objects_scene);
   std::println("[SceneInstanceLifetimeTests] reinstantiated={} meshes={} expected={}",
                bool(reinstantiated), render.sceneMeshCount(), first_mesh_count);
   if (!reinstantiated || render.sceneMeshCount() != first_mesh_count) { return 21; }
   const auto recreated = render.sceneInstanceObjects(*reinstantiated);
   if (!recreated || recreated->size() != 2U) { return 22; }
   return 0;
}
