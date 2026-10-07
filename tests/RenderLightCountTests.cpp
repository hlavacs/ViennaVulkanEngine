/**
 * @file
 * @brief Facade tests for light counts and clearing lights without losing scene ownership.
 *
 * Functional objects:
 * - main: checks authored lights, clearLights and subsequent scene-instance removal without a GPU.
 */

import std;
import VVEngine;
import VVE.TestSupport;

/// @brief Verifies light counts, preserved scene data and owner-safe removal after repeated clearing.
int main() {
   auto engine = vve::EngineBuilder<>{}.applicationName("render-light-count-tests").build();
   auto &render = engine.world().get<vve::RenderSystem>();

   if (render.sceneDirectionalLightCount() != 0U) { return 1; }
   if (render.scenePointLightCount() != 0U) { return 2; }
   if (render.sceneSpotLightCount() != 0U) { return 3; }

   render.addDirectionalLight(vve::Direction{.value = vve::Vec3{-0.5F, -1.0F, 0.25F}},
                              vve::LinearColor{.value = vve::Vec3{1.0F, 0.95F, 0.8F}},
                              vve::LightIntensity{.value = 1.25F},
                              vve::LinearColor{.value = vve::Vec3{0.02F, 0.02F, 0.02F}});
   if (render.sceneDirectionalLightCount() != 1U) { return 4; }

   render.addPointLight(vve::Position{.value = vve::Vec3{1.0F, 2.0F, -1.0F}},
                        vve::LinearColor{.value = vve::Vec3{0.8F, 0.9F, 1.0F}},
                        vve::LightIntensity{.value = 2.0F}, vve::LightRange{.value = 6.0F});
   if (render.scenePointLightCount() != 1U) { return 5; }

   render.addSpotLight(vve::Position{.value = vve::Vec3{-1.0F, 3.0F, 1.0F}},
                       vve::Direction{.value = vve::Vec3{0.0F, -1.0F, 0.0F}},
                       vve::LinearColor{.value = vve::Vec3{1.0F, 0.75F, 0.55F}},
                       vve::LightIntensity{.value = 1.5F}, vve::LightRange{.value = 5.0F},
                       vve::SpotConeAngle{.radians = 0.6F});
   if (render.sceneSpotLightCount() != 1U) { return 6; }

   // Clearing lights preserves the plane and its resources, even on a repeated call.
   const auto plane = render.addPlane(vve::Vec2{2.0F, 2.0F}, vve::LinearColor{});
   if (!plane || render.sceneInstanceCount() != 1U) { return 7; }
   const auto mesh_count = render.sceneMeshCount();
   const auto material_count = render.sceneMaterialCount();
   render.clearLights();
   std::println("E2 clearLights directional={} point={} spot={} instances={}",
      render.sceneDirectionalLightCount(), render.scenePointLightCount(), render.sceneSpotLightCount(),
      render.sceneInstanceCount());
   if (render.sceneDirectionalLightCount() != 0U || render.scenePointLightCount() != 0U ||
       render.sceneSpotLightCount() != 0U || render.sceneInstanceCount() != 1U) { return 8; }
   render.clearLights();
   if (render.hasSceneDirectionalLight() || render.hasScenePointLight() || render.hasSceneSpotLight() ||
       render.sceneMeshCount() != mesh_count || render.sceneMaterialCount() != material_count ||
       render.objectVisible(*plane) != true) { return 9; }

   // Load and instantiate after clearing, then clear imported lights while retaining their instance and camera.
   const auto scene = engine.world().get<vve::AssetSystem>().loadScene(vve::test::writeFixture(VVE_TEST_TMP_DIR, "lights.gltf", R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {"KHR_lights_punctual": {"lights": [
    {"type": "directional", "color": [1, 1, 1], "intensity": 1.0},
    {"type": "point", "color": [1, 1, 1], "intensity": 2.0, "range": 10.0},
    {"type": "spot", "color": [1, 1, 1], "intensity": 2.0, "range": 10.0,
     "spot": {"innerConeAngle": 0.2, "outerConeAngle": 0.6}}
  ]}},
  "scenes": [{"nodes": [0, 1, 2, 3]}], "scene": 0,
  "nodes": [
    {"name": "Directional", "extensions": {"KHR_lights_punctual": {"light": 0}}},
    {"name": "Point", "extensions": {"KHR_lights_punctual": {"light": 1}}},
    {"name": "Spot", "extensions": {"KHR_lights_punctual": {"light": 2}}},
    {"name": "Camera", "camera": 0}
  ],
  "cameras": [{"type": "perspective", "perspective": {
    "yfov": 0.7, "aspectRatio": 1.5, "znear": 0.1, "zfar": 50.0
  }}]
})"));
   if (!scene) { std::println("E2 scene_load_error={}", vve::errorName(scene.error())); return 10; }
   const auto options = vve::SceneInstantiationOptions{.apply_cameras = true, .apply_lights = true};
   const auto old_instance = render.instantiateScene(*scene, options);
   if (!old_instance || render.sceneDirectionalLightCount() != 1U || render.scenePointLightCount() != 1U ||
       render.sceneSpotLightCount() != 1U || render.sceneCameraCount() != 1U) { return 11; }
   render.clearLights();
   if (render.sceneDirectionalLightCount() != 0U || render.scenePointLightCount() != 0U ||
       render.sceneSpotLightCount() != 0U || render.sceneCameraCount() != 1U ||
       !render.sceneInstanceObjects(*old_instance)) { return 12; }

   // Removing the old instance must preserve newly imported lights and the authored point light.
   render.addPointLight(vve::PointLight{});
   const auto new_instance = render.instantiateScene(*scene, options);
   if (!new_instance || render.sceneCameraCount() != 2U) { return 13; }
   if (!render.removeSceneInstance(*old_instance) || render.sceneDirectionalLightCount() != 1U ||
       render.scenePointLightCount() != 2U || render.sceneSpotLightCount() != 1U ||
       render.sceneCameraCount() != 1U || !render.sceneInstanceObjects(*new_instance)) { return 14; }
   if (!render.removeSceneInstance(*new_instance) || render.sceneDirectionalLightCount() != 0U ||
       render.scenePointLightCount() != 1U || render.sceneSpotLightCount() != 0U ||
       render.sceneCameraCount() != 0U || !render.removeScene(*scene)) { return 15; }
   if (render.sceneInstanceCount() != 1U || render.sceneMeshCount() != mesh_count ||
       render.sceneMaterialCount() != material_count || render.objectVisible(*plane) != true) { return 16; }
   std::println("E2 imported_removal=ok surviving_point={} instances={}",
      render.scenePointLightCount(), render.sceneInstanceCount());

   return 0;
}
