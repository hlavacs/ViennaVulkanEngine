#include <expected>
#include <filesystem>
#include <fstream>

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

[[nodiscard]] bool fileContains(const std::filesystem::path &path, std::string_view text) {
   std::ifstream input(path, std::ios::binary);
   const std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
   return content.contains(text);
}

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
   const auto dump_dir = std::filesystem::temp_directory_path() / "vve_user_system_debug_graphs";
   std::filesystem::remove_all(dump_dir);
   if (const auto dumped = engine.writeDebugGraphs(dump_dir); !dumped) { return 2; }
   const auto task_graph = dump_dir / "task_graph.json";
   if (!fileContains(task_graph, "task.update_system.") || !fileContains(task_graph, "CountingSystem")) {
      return 3;
   }
   std::filesystem::remove_all(dump_dir);

   const auto first = engine.step();
   const auto second = engine.step();
   if (!first || !second || *first != vve::FrameStatus::running || *second != vve::FrameStatus::stopped) {
      return 4;
   }
   if (init_count != 1 || update_count != 2 || last_frame != 1 || last_window_count != 1) { return 5; }

   return 0;
}
