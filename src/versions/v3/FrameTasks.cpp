module VEEngine.V3;
import :Internal;
import std;

/**
 * @file
 * @brief Helper tasks that synchronize runtime frame state back into `World`.
 *
 * These helpers bridge the window/input subsystem state into the game-facing
 * world facade so user systems can observe current windows and input without
 * depending on lower-level runtime types.
 */
namespace vve::v3::detail {

#ifndef NDEBUG
   /// @brief Hotkey used to trigger debug graph dumps in debug builds.
   inline constexpr std::int32_t debugDumpGraphHotkey = 0x40000042u; // SDLK_F9
#endif

   /**
    * @brief Copies runtime window state into the world-facing window list.
    * @param window_frame Current frame snapshot produced by the window system.
    * @param windows Destination world-visible window cache.
    */
   void syncWorldWindows(const WindowFrameData &window_frame, std::vector<vve::WindowInfo> &windows) {
      // Rebuild the world-facing cache from the authoritative runtime snapshot
      windows.clear(); // so user systems always see a coherent per-frame view.
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

   /**
    * @brief Applies current frame input events to the world-facing input snapshot.
    * @param window_frame Current frame snapshot produced by the window system.
    * @param input Destination input snapshot exposed through `World`.
    */
   void syncWorldInput(const WindowFrameData &window_frame, vve::InputState &input) {
      // Begin by clearing per-frame transition state while preserving longer-
      vve::detail::beginInputFrame(input); // lived key-down and mouse-position data.

      for (const auto &event : window_frame.events) {
         switch (event.type) {
         case WindowEventType::key_down:
            // Key-down events update both the held set and the pressed-this-
            vve::detail::pressKey(input, event.b); // frame set.
            break;
         case WindowEventType::key_up:
            vve::detail::releaseKey(input, event.b); // Key-up events clear the held set and mark the release edge.
            break;
         case WindowEventType::mouse_move: {
            // Mouse move events derive frame-local delta from the previously
            // observed position before updating the authoritative position.
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
            // Wheel motion accumulates for the frame so callers can consume it
            vve::detail::addMouseWheelDelta( // once per update without losing multiple wheel ticks.
                input, event.window.value,
                vve::math::Vec2{static_cast<vve::math::Scalar>(event.a), static_cast<vve::math::Scalar>(event.b)});
            break;
         default:
            break;
         }
      }
   }

   /**
    * @brief Ensures the task graph contains the task that synchronizes world state.
    * @return Handle of the world-sync task, existing or newly created.
    */
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

             // Window and input synchronization is derived entirely from the
             // window-system frame snapshot handed to the task callback.
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
   /**
    * @brief Registers a debug-only task that dumps the combined frame graph.
    *
    * The task is gated by a hotkey so normal frame execution stays unaffected.
    */
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
             // The graph dump is opt-in and only fires on the debug hotkey.
             if (!execution_context.world->input().wasKeyPressed(debugDumpGraphHotkey)) {
                return {};
             }

             const auto output_path = std::filesystem::current_path() / "graph_dumps" / "combined_frame_graph.dot";
             return exportCombinedGraphDot(*task_graph, render_pipelines, output_path);
          });
   }
#endif

} // namespace vve::v3::detail
