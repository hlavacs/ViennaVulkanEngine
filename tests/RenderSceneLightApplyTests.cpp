/**
 * @file
 * @brief Facade test for opt-in imported light application during scene instantiation.
 *
 * Functional objects:
 * - main: loads a glTF punctual light, instantiates it with lights disabled and enabled, and checks render counts.
 */

import std;

import VEEngine;
import VVE.TestSupport;

/// @brief Verifies imported lights affect render counts only when requested.
int main() {
   const auto path = vve::test::writeFixture(VVE_TEST_TMP_DIR, "vve_render_scene_light_apply_test.gltf", R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "point", "color": [0.8, 0.7, 0.6], "intensity": 4.0, "range": 9.0}
  ]}},
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "nodes": [{"name": "ImportedPointLight", "translation": [1.0, 2.0, 3.0],
             "extensions": {"KHR_lights_punctual": {"light": 0}}}]
})");

   auto engine = vve::EngineBuilder<>{}.applicationName("render-scene-light-apply-tests").build();
   auto world = engine.world();
   auto assets = world.get<vve::AssetSystem>();
   auto &render = world.get<vve::RenderSystem>();

   const auto scene = assets.loadScene(path);
   if (!scene || !scene->valid()) { return 1; }

   const auto lights = assets.sceneLights(*scene);
   if (!lights || lights->size() != 1U) { return 2; }
   const auto data = assets.lightData(lights->front());
   if (!data || data->kind != vve::LightKind::point) { return 3; }

   const auto disabled = render.instantiateScene(*scene);
   if (!disabled || render.sceneDirectionalLightCount() != 0U) { return 4; }
   if (render.scenePointLightCount() != 0U) { return 5; }
   if (render.sceneSpotLightCount() != 0U) { return 6; }

   const auto enabled = render.instantiateScene(*scene, vve::SceneInstantiationOptions{.apply_lights = true});
   if (!enabled || render.sceneDirectionalLightCount() != 0U) { return 7; }
   if (render.scenePointLightCount() != 1U) { return 8; }
   if (render.sceneSpotLightCount() != 0U) { return 9; }

   return 0;
}
