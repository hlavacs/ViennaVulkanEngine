import std;

import VEEngine;

namespace {

struct WindowCameraProbe {
   int *updates{};
   std::size_t *window_count{};
   std::optional<vve::Entity> *main_camera{};
   std::optional<vve::Entity> *tools_camera{};

   template <typename TWorld, typename TWindowFrame>
   std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &, const TWindowFrame &window_frame) {
      if (updates != nullptr) { ++*updates; }
      if (window_count != nullptr) { *window_count = window_frame.windows.size(); }

      for (const auto &window : window_frame.windows) {
         if (window.id == "main" && main_camera != nullptr) { *main_camera = window.camera; }
         if (window.id == "tools" && tools_camera != nullptr) { *tools_camera = window.camera; }
      }

      return {};
   }
};

[[nodiscard]] bool sameCamera(std::optional<vve::Entity> camera, vve::Entity expected) {
   return camera.has_value() && *camera == expected;
}

} // namespace

int main() {
   int updates = 0;
   std::size_t window_count = 0;
   std::optional<vve::Entity> frame_main_camera{};
   std::optional<vve::Entity> frame_tools_camera{};

   auto engine = vve::EngineBuilder<WindowCameraProbe>{}
                    .applicationName("window-ownership-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 1}})
                    .windows(vve::WindowSetups{vve::WindowSetup{}
                                                  .id("main")
                                                  .title("window-ownership-main")
                                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                                  .visible(false),
                                               vve::WindowSetup{}
                                                  .id("tools")
                                                  .title("window-ownership-tools")
                                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                                  .visible(false)})
                    .userSystems(vve::makeUserSystems(WindowCameraProbe{.updates = &updates,
                                                                        .window_count = &window_count,
                                                                        .main_camera = &frame_main_camera,
                                                                        .tools_camera = &frame_tools_camera}))
                    .build();

   if (!engine.init()) { return 1; }

   auto world = engine.world();
   auto &ecs = world.get<vve::ECS>();
   auto window_system = world.get<vve::WindowSystem>();
   const auto camera = ecs.create();
   if (const auto result = ecs.add(camera, vve::Camera{}); !result) { return 2; }

   if (!window_system.setWindowCamera("main", camera)) { return 3; }
   const auto main_window_after_direct_set = window_system.findWindow("main");
   const auto tools_window_after_direct_set = window_system.findWindow("tools");
   if (!main_window_after_direct_set || !tools_window_after_direct_set) { return 4; }
   if (!sameCamera(main_window_after_direct_set->camera(), camera)) { return 5; }
   if (tools_window_after_direct_set->camera().has_value()) { return 6; }

   if (!window_system.setActiveCamera(camera)) { return 7; }
   const auto main_window_after_active_set = window_system.findWindow("main");
   const auto tools_window_after_active_set = window_system.findWindow("tools");
   if (!main_window_after_active_set || !tools_window_after_active_set) { return 8; }
   if (!sameCamera(main_window_after_active_set->camera(), camera)) { return 9; }
   if (!sameCamera(tools_window_after_active_set->camera(), camera)) { return 10; }
   if (!sameCamera(window_system.windowCamera("main"), camera)) { return 19; }
   if (!sameCamera(window_system.activeCamera(), camera)) { return 20; }

   const auto status = engine.step();
   if (!status || *status != vve::FrameStatus::stopped) { return 11; }
   if (updates != 1 || window_count != 2) { return 12; }
   if (!sameCamera(frame_main_camera, camera)) { return 13; }
   if (!sameCamera(frame_tools_camera, camera)) { return 14; }

   if (!window_system.clearWindowCamera("main")) { return 15; }
   if (window_system.windowCamera("main").has_value()) { return 16; }

   const auto tools_window = window_system.findWindow("tools");
   if (!tools_window || !window_system.clearWindowCamera(tools_window->handle())) { return 17; }
   if (window_system.activeCamera().has_value()) { return 18; }

   return 0;
}
