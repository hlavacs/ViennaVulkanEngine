/**
 * @file
 * @brief Guard and render-pass contract coverage for the simple GUI system.
 *
 * Functional objects:
 * - main: creates the simple GUI implementation without SDL or Vulkan backend setup,
 *   checks guarded rendering, font setup before upload, context reuse and disabled layout persistence.
 */

#include <imgui.h>
#include <vulkan/vulkan_core.h>

import std;

import VVEngine.Simple;

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

   // Registration may be replaced before initialization; only the latest setup belongs to the context.
   int displaced_setup_calls{}, font_setup_calls{}, late_setup_calls{};
   ImGuiContext *font_setup_context{};
   ImFont *configured_font{};
   bool setup_before_upload{};
   const auto displaced_setup = gui_system.configureFonts([&displaced_setup_calls] { ++displaced_setup_calls; });
   const auto font_setup = gui_system.configureFonts([&] {
      ++font_setup_calls;
      font_setup_context = ImGui::GetCurrentContext();
      if (!font_setup_context) { return; }
      auto &atlas = *ImGui::GetIO().Fonts;
      setup_before_upload = !atlas.IsBuilt() && !atlas.Locked && atlas.TexID == ImTextureID{};
      ImFontConfig configuration{};
      configuration.SizePixels = 24.0F; // Real rasterization must retain the requested readable font size.
      configured_font = atlas.AddFontDefault(&configuration);
   });
   if (!displaced_setup || !font_setup) { return 12; }

   // Contexts must not persist GUI layouts in the application's working directory.
   gui_system.initContext();
   auto *initial_context = ImGui::GetCurrentContext();
   gui_system.initContext();
   const bool setup_once = font_setup_calls == 1 && displaced_setup_calls == 0 &&
                           font_setup_context == initial_context && ImGui::GetCurrentContext() == initial_context;
   if (!setup_once || !setup_before_upload || !configured_font) { return 13; }
   auto &atlas = *ImGui::GetIO().Fonts;
   if (atlas.Fonts.Size != 1 || atlas.Fonts[0] != configured_font || !atlas.Build() ||
       std::abs(configured_font->FontSize - 24.0F) > 1e-6F) { return 14; }

   // Changing fonts after creation would invalidate the uploaded atlas, so reject both replacement and clearing.
   const auto late_setup = gui_system.configureFonts([&late_setup_calls] { ++late_setup_calls; });
   const auto late_clear = gui_system.configureFonts({});
   if (late_setup || late_setup.error() != vve::Error::already_initialized ||
       late_clear || late_clear.error() != vve::Error::already_initialized) { return 15; }
   gui_system.initContext();
   if (font_setup_calls != 1 || late_setup_calls != 0 || ImGui::GetIO().Fonts->Fonts.Size != 1) { return 16; }
   std::println("[GuiSystemTests] font_setup_calls={} displaced_calls={} setup_before_upload={} font_pixels={} late_error={}",
                font_setup_calls, displaced_setup_calls, setup_before_upload, configured_font->FontSize,
                vve::errorName(late_setup.error()));
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
   // A fresh context invokes the retained setup again, proving rejected late registrations left it unchanged.
   if (font_setup_calls != 2 || displaced_setup_calls != 0 || late_setup_calls != 0 ||
       !setup_before_upload || !configured_font || ImGui::GetIO().Fonts->Fonts.Size != 1) { return 17; }
   const auto unorm_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   gui_system.shutdownContext();
   if (unorm_color.x != authored_color.x || unorm_color.w != authored_color.w) { return 10; }

   // Without mutable swapchains, convert the sRGB theme exactly once; never convert alpha.
   gui_system.initContext(VK_FORMAT_B8G8R8A8_SRGB);
   const auto fallback_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   gui_system.initContext(VK_FORMAT_B8G8R8A8_SRGB);
   const auto repeated_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
   if (font_setup_calls != 3 || late_setup_calls != 0) { return 18; }
   gui_system.shutdownContext();
   const float expected = std::pow((authored_color.x + 0.055F) / 1.055F, 2.4F);
   std::println("[GuiSystemTests] authored={} fallback={} repeated={} alpha={}",
                authored_color.x, fallback_color.x, repeated_color.x, fallback_color.w);
   if (std::abs(fallback_color.x - expected) > 1e-6F || repeated_color.x != fallback_color.x ||
       fallback_color.w != authored_color.w) { return 11; }

   // An empty setup clears the retained hook before a new context is created and keeps initialization optional.
   const auto empty_setup = gui_system.configureFonts({});
   if (!empty_setup) { return 19; }
   gui_system.initContext();
   const bool empty_setup_valid = ImGui::GetCurrentContext() != nullptr && font_setup_calls == 3;
   gui_system.shutdownContext();
   std::println("[GuiSystemTests] recreated_setups={} rejected_calls={} empty_setup_valid={}",
                font_setup_calls, late_setup_calls, empty_setup_valid);
   if (!empty_setup_valid) { return 20; }

   return 0;
}
