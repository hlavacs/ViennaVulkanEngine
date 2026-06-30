/**
 * @file
 * @brief Guard and render-pass contract coverage for the simple GUI system.
 *
 * Functional objects:
 * - main: creates the simple GUI implementation without SDL or Vulkan backend setup,
 *   stores a frame callback, verifies guarded frame recording, and checks pass ordering data.
 */

#include <vulkan/vulkan_core.h>

import std;

import VEEngine.Simple;

int main() {
   vve::simple::GuiSystem gui_system{};
   if (gui_system.hasFrameCallback()) { return 1; }

   bool frame_callback_called{};
   gui_system.draw([&frame_callback_called] { frame_callback_called = true; });
   if (!gui_system.hasFrameCallback()) { return 2; }

   // The default system has no initialized backends, so recording must not enter Dear ImGui.
   gui_system.recordFrame(VK_NULL_HANDLE);
   if (frame_callback_called) { return 3; }

   constexpr std::string_view gui_overlay_pass{"gui.overlay_pass"};
   const auto passes = vve::simple::GuiSystem::passes();
   if (passes.size() != 3) { return 4; }
   if (passes[0].name != gui_overlay_pass || passes[0].milestone) { return 5; }
   if (passes[0].depends_on.size() != 1 || passes[0].depends_on[0] != vve::simple::RenderMilestone::scene_color()) {
      return 6;
   }
   if (passes[1].name != vve::simple::RenderMilestone::gui() || !passes[1].milestone) { return 7; }
   if (passes[1].depends_on.size() != 1 || passes[1].depends_on[0] != gui_overlay_pass) { return 8; }
   if (passes[2].name != vve::simple::RenderMilestone::frame_finished() || !passes[2].milestone) { return 9; }
   if (passes[2].depends_on.size() != 1 || passes[2].depends_on[0] != vve::simple::RenderMilestone::gui()) {
      return 10;
   }

   return 0;
}
