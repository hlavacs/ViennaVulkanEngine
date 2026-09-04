import std;

import VEEngine;

namespace {

struct SharedSystem {
   int value{0};
};

struct CountingSystem {
   int *init_count{};
   int *update_count{};
   std::uint64_t *last_frame{};
   std::size_t *last_window_count{};
   int *shared_value{};

   template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
      if (init_count != nullptr) { ++*init_count; }
      if (shared_value != nullptr) {
         auto &shared = world.template get<SharedSystem>();
         *shared_value = shared.value;
         shared.value = 77;
      }
      auto &render_system = world.template get<vve::RenderSystem>();
      render_system.clearScene();
      if (const auto result = render_system.addPlane(vve::Vec2{1.0F, 1.0F}, vve::LinearColor{}); !result) {
         return std::unexpected(result.error());
      }
      render_system.setCamera(vve::Camera{}, vve::PixelExtent{.width = 64, .height = 64});
      render_system.setDirectionalLight(vve::Direction{}, vve::LinearColor{}, vve::LightIntensity{},
                                        vve::LinearColor{});
      return world.template get<vve::WindowSystem>().windowCount() == 0
                ? std::unexpected(vve::Error::missing_object)
                : std::expected<void, vve::Error>{};
   }

   template <typename TWorld, typename TWindowFrame>
   std::expected<void, vve::Error> update(TWorld &, const vve::FrameContext &frame,
                                          const TWindowFrame &window_frame) {
      if (update_count != nullptr) { ++*update_count; }
      if (last_frame != nullptr) { *last_frame = frame.frame_index.value; }
      if (last_window_count != nullptr) { *last_window_count = window_frame.windows.size(); }
      return window_frame.windows.empty() ? std::unexpected(vve::Error::missing_object)
                                          : std::expected<void, vve::Error>{};
   }
};

} // namespace

int main() {
   int init_count = 0;
   int update_count = 0;
   std::uint64_t last_frame = 99;
   std::size_t last_window_count = 0;
   int shared_value = 0;

   auto engine = vve::EngineBuilder<SharedSystem, CountingSystem>{}
                    .applicationName("user-system-tests")
                    .maxFrames(vve::MaxFrames{.value = vve::FrameCount{.value = 2}})
                    .addWindow(vve::WindowSetup{}
                                  .id("main")
                                  .title("user-system-tests")
                                  .extent(vve::PixelExtent{.width = 64, .height = 64})
                                  .visible(false))
                    .userSystems(vve::makeUserSystems(SharedSystem{.value = 42},
                                                       CountingSystem{.init_count = &init_count,
                                                                      .update_count = &update_count,
                                                                      .last_frame = &last_frame,
                                                                      .last_window_count = &last_window_count,
                                                                      .shared_value = &shared_value}))
                    .build();

   if (!engine.init()) { return 1; }
   auto world = engine.world();
   auto &render_system = world.get<vve::RenderSystem>();
   if (render_system.sceneMeshCount() != 1 || render_system.sceneMaterialCount() != 1 ||
       render_system.sceneInstanceCount() != 1) {
      return 8;
   }
   if (!render_system.hasSceneCamera() || !render_system.hasSceneDirectionalLight()) { return 9; }

   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != vve::FrameStatus::running || *second != vve::FrameStatus::stopped) {
      return 4;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || last_window_count != 1) { return 5; }
   if (shared_value != 42) { return 6; }
   if (engine.world().get<SharedSystem>().value != 77) { return 7; }
   if (render_system.renderedFrameCount() != 2 || render_system.lastRenderedWindowCount() != 1) { return 10; }

   return 0;
}
