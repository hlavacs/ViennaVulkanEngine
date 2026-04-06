module VEEngine.V3;
import :Internal;
import std;

namespace vve::v3::detail {

#ifndef NDEBUG
   inline constexpr std::int32_t debugDumpGraphHotkey = 0x40000042u; // SDLK_F9
#endif

   void syncWorldWindows(const WindowFrameData &window_frame, std::vector<vve::WindowInfo> &windows) {
      windows.clear();
      windows.reserve(window_frame.windows.size());
      for (const auto &window : window_frame.windows) {
         windows.push_back(vve::WindowInfo{.handle = window.handle.value,
                                          .id = window.id,
                                          .title = window.title,
                                          .width = window.width,
                                          .height = window.height,
                                          .focused = window.focused,
                                          .minimized = window.minimized,
                                          .should_close = window.should_close});
      }
   }

   void syncWorldInput(const WindowFrameData &window_frame, vve::InputState &input) {
      vve::detail::beginInputFrame(input);

      for (const auto &event : window_frame.events) {
         switch (event.type) {
         case WindowEventType::key_down:
            vve::detail::pressKey(input, event.b);
            break;
         case WindowEventType::key_up:
            vve::detail::releaseKey(input, event.b);
            break;
         case WindowEventType::mouse_move: {
            const vve::math::Vec2 position{static_cast<vve::math::Scalar>(event.a),
                                           static_cast<vve::math::Scalar>(event.b)};
            const auto window_handle = event.window.value;
            const auto current_position = input.mousePosition(window_handle).value_or(position);
            vve::detail::addMouseDelta(
                input, window_handle, vve::math::Vec2{position.x - current_position.x, position.y - current_position.y});
            vve::detail::setMousePosition(input, window_handle, position);
            break;
         }
         case WindowEventType::mouse_wheel:
            vve::detail::addMouseWheelDelta(
                input, event.window.value,
                vve::math::Vec2{static_cast<vve::math::Scalar>(event.a), static_cast<vve::math::Scalar>(event.b)});
            break;
         default:
            break;
         }
      }
   }

   TaskNodeHandle ensureWorldSyncTask(std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                      vve::detail::WorldRuntimeAccess &world_runtime_access,
                                      TaskGraphBuilder &builder) {
      if (const auto existing_task = builder.findTask("task.sync_world_state")) {
         return *existing_task;
      }

      const auto sync_world_state_task =
          builder.addTask("task.sync_world_state", TaskKernelId::none, {},
                          {TaskGraphBuilder::taskHandleFor("task.poll_window_events")}, {}, "Sync World State",
                          TaskPhase::input);

      const auto callback_set = builder.setTaskCallback(
          sync_world_state_task,
          [&world_windows, &input_state,
           &world_runtime_access](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
             if (execution_context.window_frame == nullptr) {
                return std::unexpected(vve::Error::invalid_argument);
             }

             syncWorldWindows(*execution_context.window_frame, world_windows);
             world_runtime_access.windows_begin = world_windows.cbegin();
             world_runtime_access.windows_end = world_windows.cend();
             syncWorldInput(*execution_context.window_frame, input_state);
             return {};
          });
      if (!callback_set) {
         return {};
      }

      return sync_world_state_task;
   }

#ifndef NDEBUG
   void registerDebugGraphDumpTask(std::function<const TaskGraph *()> task_graph_accessor,
                                   VectorConstRange<WindowRenderPipeline> render_pipelines,
                                   std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                   vve::detail::WorldRuntimeAccess &world_runtime_access, TaskGraphBuilder &builder) {
      const auto sync_world_state_task =
          ensureWorldSyncTask(world_windows, input_state, world_runtime_access, builder);

      const auto dump_graph_task =
          builder.addTask("task.debug.dump_graphs", TaskKernelId::none, {}, {sync_world_state_task}, {},
                          "Dump Combined Graph DOT", TaskPhase::user_update);
      [[maybe_unused]] const auto callback_set = builder.setTaskCallback(
          dump_graph_task,
          [task_graph_accessor = std::move(task_graph_accessor),
           render_pipelines](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
             const auto *task_graph = task_graph_accessor ? task_graph_accessor() : nullptr;
             if (task_graph == nullptr || execution_context.world == nullptr) {
                return std::unexpected(vve::Error::invalid_argument);
             }
             if (!execution_context.world->input().wasKeyPressed(debugDumpGraphHotkey)) {
                return {};
             }

             const auto output_path = std::filesystem::current_path() / "graph_dumps" / "combined_frame_graph.dot";
             return exportCombinedGraphDot(*task_graph, render_pipelines, output_path);
          });
   }
#endif

} // namespace vve::v3::detail
