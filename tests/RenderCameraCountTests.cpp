/**
 * @file
 * @brief Facade test for public render-scene imported-camera count query.
 *
 * Functional objects:
 * - main: builds a facade engine and checks the fresh render-scene camera count.
 */

import VEEngine;

/// @brief Verifies imported cameras are absent on a fresh facade render system.
int main() {
   auto engine = vve::EngineBuilder<>{}.applicationName("render-camera-count-tests").build();
   auto &render = engine.world().get<vve::RenderSystem>();

   if (render.sceneCameraCount() != 0U) { return 1; }

   return 0;
}
