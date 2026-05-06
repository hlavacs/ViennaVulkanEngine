import VEEngine;

int main() {
   auto engine = vve::makeEngine(vve::ApplicationName{"window-input-tests"});
   auto input = engine.world().input();
   const auto window = vve::makeHandleForTest<vve::WindowHandle>(77);

   input.pressKey('W');
   if (!input.isKeyDown('w') || !input.wasKeyPressed('w')) { return 1; }
   input.beginFrame();
   if (!input.isKeyDown('W') || input.wasKeyPressed('W')) { return 2; }
   input.holdKey('W');
   if (!input.isKeyDown('w') || input.wasKeyPressed('w')) { return 3; }
   input.releaseKey('w');
   if (input.isKeyDown('W') || !input.wasKeyReleased('W')) { return 4; }

   input.setMousePosition(window, vve::Vec2{10.0F, 20.0F});
   input.addMouseDelta(window, vve::Vec2{1.0F, 2.0F});
   input.addMouseDelta(window, vve::Vec2{3.0F, 4.0F});
   input.addMouseWheelDelta(window, vve::Vec2{0.0F, -1.0F});

   const auto position = input.mousePosition(window);
   if (!position || position->x != 10.0F || position->y != 20.0F) { return 5; }
   if (input.mouseDelta(window).x != 4.0F || input.mouseDelta(window).y != 6.0F) { return 6; }
   if (input.mouseWheelDelta(window).y != -1.0F) { return 7; }
   input.beginFrame();
   if (input.mouseDelta(window).x != 0.0F || input.mouseWheelDelta(window).y != 0.0F) { return 8; }

   return 0;
}
