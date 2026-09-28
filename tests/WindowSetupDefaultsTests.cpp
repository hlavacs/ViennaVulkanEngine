/**
 * @file
 * @brief Checks that adding a startup window does not retain an implicit main window.
 */
import std;

import VEEngine;
import VEEngine.Simple;

/// @brief Verifies empty startup collections and the facade builder path without rendering.
int main() {
   const auto simple_empty = vve::simple::Windows{}.value.empty();
   std::cout << "simple_windows_empty=" << simple_empty << '\n';

   // Adding a hidden tools window must be the only explicit startup entry.
   auto ws = vve::WindowSetups{};
   ws.add(vve::WindowSetup{}.id("tools").title("window-setup-defaults-tests")
             .extent(vve::PixelExtent{.width = 64, .height = 64}).visible(false));
   auto engine = vve::EngineBuilder<>{}.windows(ws).build();
   const auto initialized = engine.init();
   std::cout << "init_error=" << (initialized ? "none" : vve::errorName(initialized.error())) << '\n';
   if (!initialized) { return 1; }

   // Inspect the facade window registry; init() does not create a Vulkan device.
   auto &windows = engine.world().get<vve::WindowSystem>();
   const auto tools = windows.findWindow("tools").has_value();
   const auto main = windows.findWindow("main").has_value();
   std::cout << "window_count=" << windows.windowCount() << " tools=" << tools << " main=" << main << '\n';
   if (windows.windowCount() != 1U || !tools || main) { return 2; }
   if (!simple_empty) { return 3; }
   return 0;
}
