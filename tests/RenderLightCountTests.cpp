/**
 * @file
 * @brief Facade test for public render-scene light count queries.
 *
 * Functional objects:
 * - main: builds a facade engine, adds lights through public render calls, and checks visible counts.
 */

import VEEngine;

/// @brief Verifies public light adders are observable through facade-only count queries.
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

   return 0;
}
