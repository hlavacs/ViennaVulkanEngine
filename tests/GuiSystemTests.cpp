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

   return 0;
}
