/**
 * @file
 * @brief SDL event-queue tests for key taps, GUI input exclusivity and held-key release.
 * A hidden WindowSystem and the ImGui SDL backend exercise input without initializing a renderer.
 */
#include <SDL3/SDL.h>
#include <imgui.h>

import std;
import VVEngine.Simple;

namespace {

/// @brief Builds a non-repeated W key event associated with this test's hidden window.
[[nodiscard]] SDL_Event keyEvent(SDL_WindowID window, std::uint32_t type) {
   SDL_Event event{};
   event.type = type;
   event.key.windowID = window;
   event.key.scancode = SDL_SCANCODE_W;
   event.key.key = SDLK_W;
   event.key.down = type == SDL_EVENT_KEY_DOWN;
   return event;
}

/// @brief Prints all three keyboard flags so failures identify the lost or incorrectly delivered edge.
void printKeys(std::string_view stage, const vve::simple::InputState &input) {
   std::println("[WindowEventTests] {} pressed={} released={} down={}", stage,
                input.wasKeyPressed(SDLK_W), input.wasKeyReleased(SDLK_W), input.isKeyDown(SDLK_W));
}

} // namespace

/// @brief Pushes deterministic events through poll and separately verifies Dear ImGui's capture policy.
int main() {
   vve::simple::WindowSystem windows{};
   const auto initialized = windows.init(vve::simple::Windows{.value = {
      vve::WindowDesc{.id = "input", .extent = {64, 64}, .renderer_id = {"none"}, .visible = false}}});
   std::println("[WindowEventTests] init={}", initialized ? "ok" : vve::errorName(initialized.error()));
   if (!initialized) { return 1; }
   const auto *window = windows.findWindow("input");
   if (!window || !windows.poll()) { return 2; } // Drain startup events before the synthetic input.
   auto down = keyEvent(SDL_GetWindowID(window->native()), SDL_EVENT_KEY_DOWN);
   auto up = keyEvent(down.key.windowID, SDL_EVENT_KEY_UP);
   const auto &input = windows.input();

   // A tap inside one poll retains both edges even though the key is no longer held.
   if (!SDL_PushEvent(&down) || !SDL_PushEvent(&up) || !windows.poll()) { return 3; }
   printKeys("tap", input);
   if (!input.wasKeyPressed(SDLK_W) || !input.wasKeyReleased(SDLK_W) || input.isKeyDown(SDLK_W)) { return 4; }
   if (!windows.poll()) { return 5; }
   if (input.wasKeyPressed(SDLK_W) || input.wasKeyReleased(SDLK_W) || input.isKeyDown(SDLK_W)) { return 6; }

   // A GUI-claimed press never reaches gameplay; its later release must not manufacture a release edge.
   windows.setGuiEventSink([](const SDL_Event &event) { return event.type == SDL_EVENT_KEY_DOWN; });
   if (!SDL_PushEvent(&down) || !windows.poll()) { return 7; }
   printKeys("claimed_down", input);
   if (input.wasKeyPressed(SDLK_W) || input.wasKeyReleased(SDLK_W) || input.isKeyDown(SDLK_W)) { return 8; }
   if (!SDL_PushEvent(&up) || !windows.poll()) { return 9; }
   printKeys("unheld_up", input);
   if (input.wasKeyPressed(SDLK_W) || input.wasKeyReleased(SDLK_W) || input.isKeyDown(SDLK_W)) { return 10; }

   // Taking GUI focus while a key is held must still let gameplay observe its release.
   windows.setGuiEventSink({});
   if (!SDL_PushEvent(&down) || !windows.poll()) { return 11; }
   if (!input.wasKeyPressed(SDLK_W) || !input.isKeyDown(SDLK_W)) { return 12; }
   windows.setGuiEventSink([](const SDL_Event &) { return true; });
   if (!SDL_PushEvent(&up) || !windows.poll()) { return 13; }
   printKeys("held_up_claimed", input);
   if (input.wasKeyPressed(SDLK_W) || !input.wasKeyReleased(SDLK_W) || input.isKeyDown(SDLK_W)) { return 14; }

   // Establish a real delta first, then prove that claimed motion contributes nothing on the next poll.
   SDL_Event motion{};
   motion.type = SDL_EVENT_MOUSE_MOTION;
   motion.motion.windowID = down.key.windowID;
   motion.motion.x = 32.0F;
   motion.motion.y = 16.0F;
   motion.motion.xrel = 8.0F;
   motion.motion.yrel = -4.0F;
   windows.setGuiEventSink({});
   if (!SDL_PushEvent(&motion) || !windows.poll()) { return 15; }
   const auto delta = input.mouseDelta(window->info().handle);
   if (delta.x == 0.0F || delta.y == 0.0F) { return 16; }
   windows.setGuiEventSink([](const SDL_Event &event) { return event.type == SDL_EVENT_MOUSE_MOTION; });
   if (!SDL_PushEvent(&motion) || !windows.poll()) { return 17; }
   const auto claimed_delta = input.mouseDelta(window->info().handle);
   std::println("[WindowEventTests] motion_delta={},{} claimed_delta={},{}", delta.x, delta.y, claimed_delta.x, claimed_delta.y);
   if (claimed_delta.x != 0.0F || claimed_delta.y != 0.0F) { return 18; }
   windows.setGuiEventSink({});

   // The real GUI backend claims key-down exactly when requested, but never key-up.
   vve::simple::GuiSystem gui{};
   const auto gui_initialized = gui.initSDL(window->native());
   if (!gui_initialized) { gui.shutdownContext(); return 19; }
   ImGui::GetIO().WantCaptureKeyboard = true;
   const bool captured_down = gui.processEvent(down);
   const bool captured_up = gui.processEvent(up);
   ImGui::GetIO().WantCaptureKeyboard = false;
   const bool uncaptured_down = gui.processEvent(down);
   const bool uncaptured_up = gui.processEvent(up);
   gui.shutdownSDL();
   gui.shutdownContext();
   std::println("[WindowEventTests] gui_capture down={} up={} gui_no_capture down={} up={}",
                captured_down, captured_up, uncaptured_down, uncaptured_up);
   if (!captured_down || captured_up || uncaptured_down || uncaptured_up) { return 20; }
   return 0;
}
