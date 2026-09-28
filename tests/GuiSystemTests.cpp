/**
 * @file
 * @brief Guard and render-pass contract coverage for the simple GUI system.
 *
 * Functional objects:
 * - main: creates the simple GUI implementation without SDL or Vulkan backend setup,
 *   checks initialization errors and guarded preparation/recording, and verifies disabled layout persistence.
 */

#include <imgui.h>
#include <vulkan/vulkan_core.h>

import std;

import VEEngine.Simple;

int main() {
   vve::simple::GuiSystem gui_system{};
   if (gui_system.hasFrameCallback()) { return 1; }

   bool frame_callback_called{};
   gui_system.draw([&frame_callback_called] { frame_callback_called = true; });
   if (!gui_system.hasFrameCallback()) { return 2; }

   // Neither preparation nor recording may enter Dear ImGui without initialized backends.
   if (gui_system.prepareFrame()) { return 3; }
   gui_system.record(VK_NULL_HANDLE);
   if (frame_callback_called) { return 3; }

   // Invalid arguments and missing backend prerequisites must leave the GUI unavailable.
   const auto sdl_result = gui_system.initSDL(nullptr);
   const auto vulkan_result = gui_system.initVulkan(nullptr);
   const auto fonts_result = gui_system.buildFonts();
   if (sdl_result || sdl_result.error() != vve::Error::invalid_argument) { return 5; }
   if (vulkan_result || vulkan_result.error() != vve::Error::invalid_argument) { return 6; }
   if (fonts_result || fonts_result.error() != vve::Error::not_initialized) { return 7; }
   if (gui_system.ready()) { return 8; }

   // Contexts must not persist GUI layouts in the application's working directory.
   gui_system.initContext();
   const auto authored_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   const bool ini_disabled = ImGui::GetIO().IniFilename == nullptr;
   if (gui_system.prepareFrame()) { return 9; }
   gui_system.record(VK_NULL_HANDLE);
   const bool guarded = !gui_system.ready() && !frame_callback_called;
   gui_system.shutdownContext();
   std::println("[GuiSystemTests] ini_disabled={} guarded={} sdl_error={} vulkan_error={} fonts_error={}",
                ini_disabled, guarded, vve::errorName(sdl_result.error()),
                vve::errorName(vulkan_result.error()), vve::errorName(fonts_result.error()));
   if (!ini_disabled) { return 4; }
   if (!guarded || gui_system.ready()) { return 9; }

   // A UNORM GUI target preserves the authored theme, including alpha.
   gui_system.initContext(VK_FORMAT_B8G8R8A8_UNORM);
   const auto unorm_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   gui_system.shutdownContext();
   if (unorm_color.x != authored_color.x || unorm_color.w != authored_color.w) { return 10; }

   // Without mutable swapchains, convert the sRGB theme exactly once; never convert alpha.
   gui_system.initContext(VK_FORMAT_B8G8R8A8_SRGB);
   const auto fallback_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   gui_system.initContext(VK_FORMAT_B8G8R8A8_SRGB);
   const auto repeated_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   gui_system.shutdownContext();
   const float expected = std::pow((authored_color.x + 0.055F) / 1.055F, 2.4F);
   std::println("[GuiSystemTests] authored={} fallback={} repeated={} alpha={}",
                authored_color.x, fallback_color.x, repeated_color.x, fallback_color.w);
   if (std::abs(fallback_color.x - expected) > 1e-6F || repeated_color.x != fallback_color.x ||
       fallback_color.w != authored_color.w) { return 11; }

   return 0;
}
