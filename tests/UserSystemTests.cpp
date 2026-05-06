#include <expected>

import VEEngine;

namespace {

struct CountingSystem {
   int *init_count{};
   int *update_count{};
   std::uint64_t *last_frame{};
   std::size_t *last_window_count{};

   template <typename TWorld> std::expected<void, vve::Error> init(TWorld &world) {
      if (init_count != nullptr) { ++*init_count; }
      return world.windowSystem().windowCount() == 0 ? std::unexpected(vve::Error::missing_object)
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

   auto engine = vve::makeEngine(
      vve::ApplicationName{"user-system-tests"},
      vve::MaxFrames{.value = vve::FrameCount{.value = 2}},
      vve::WindowSetups{vve::WindowSetup{}
                           .id("main")
                           .title("user-system-tests")
                           .extent(vve::PixelExtent{.width = 64, .height = 64})
                           .visible(false)},
      vve::makeUserSystems(CountingSystem{.init_count = &init_count,
                                           .update_count = &update_count,
                                           .last_frame = &last_frame,
                                           .last_window_count = &last_window_count}));

   if (!engine.init()) { return 1; }
   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != vve::FrameStatus::running || *second != vve::FrameStatus::stopped) {
      return 2;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || last_window_count != 1) { return 3; }

   return 0;
}
