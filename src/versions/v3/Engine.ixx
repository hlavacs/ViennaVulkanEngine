module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine.V3;
import VEEngine;
export import VEEngine.V3.Types;
export import VEEngine.V3.Systems;
import :Internal;
import std;

namespace vve::v3 {

   namespace detail {

#ifndef NDEBUG
      inline constexpr std::int32_t debugDumpGraphHotkey = 0x40000042u; // SDLK_F9
#endif

      inline void syncWorldWindows(const WindowFrameData &window_frame, std::vector<vve::WindowInfo> &windows) {
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

      inline void syncWorldInput(const WindowFrameData &window_frame, vve::InputState &input) {
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
                   input, window_handle,
                   vve::math::Vec2{position.x - current_position.x, position.y - current_position.y});
               vve::detail::setMousePosition(input, window_handle, position);
               break;
            }
            case WindowEventType::mouse_wheel:
               vve::detail::addMouseWheelDelta(input, event.window.value,
                   vve::math::Vec2{static_cast<vve::math::Scalar>(event.a), static_cast<vve::math::Scalar>(event.b)});
               break;
            default:
               break;
            }
         }
      }

      inline TaskNodeHandle ensureWorldSyncTask(std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                                vve::detail::WorldRuntimeAccess &world_runtime_access,
                                                TaskGraphBuilder &builder) {
         if (const auto existing_task = builder.findTask("task.sync_world_state")) {
            return *existing_task;
         }

         const auto sync_world_state_task =
             builder.addTask("task.sync_world_state", TaskKernelId::none, {},
                             {TaskGraphBuilder::taskHandleFor("task.poll_window_events")}, {}, "Sync World State",
                             TaskPhase::input);

         builder.setTaskCallback(
             sync_world_state_task,
             [&world_windows, &input_state,
              &world_runtime_access](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.window_frame == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                detail::syncWorldWindows(*execution_context.window_frame, world_windows);
                world_runtime_access.windows_begin = world_windows.cbegin();
                world_runtime_access.windows_end = world_windows.cend();
                detail::syncWorldInput(*execution_context.window_frame, input_state);
                return {};
             });

         return sync_world_state_task;
      }

      template <typename TEngine>
      [[nodiscard]] std::expected<void, vve::Error> loadSceneThroughWorld(void *context,
                                                                          const std::filesystem::path &path) {
         if (context == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return static_cast<TEngine *>(context)->loadSceneFile(path);
      }

      template <typename TSystem>
      [[nodiscard]] std::expected<void, vve::Error> invokeUserSystemInit(TSystem &system, vve::World &world) {
         // C++26 reflection is not available in the current toolchain, so
         // hook detection falls back to constrained lookup in this single
         // helper seam.
         if constexpr (requires { system.init(world); }) {
            return system.init(world);
         } else {
            return {};
         }
      }

      template <typename TSystem>
      [[nodiscard]] std::expected<void, vve::Error> invokeUserSystemUpdate(TSystem &system, vve::World &world,
                                                                           const FrameContext &frame_context,
                                                                           const WindowFrameData &window_frame) {
         if constexpr (requires { system.update(world, frame_context, window_frame); }) {
            return system.update(world, frame_context, window_frame);
         } else if constexpr (requires { system.update(world, frame_context); }) {
            return system.update(world, frame_context);
         } else {
            return {};
         }
      }

      template <typename... TSystems>
      [[nodiscard]] std::expected<void, vve::Error> initUserSystems(std::tuple<TSystems...> &systems,
                                                                    vve::World &world) {
         std::expected<void, vve::Error> result{};
         std::apply(
             [&](auto &...system) { ((result = invokeUserSystemInit(system, world)), ...); },
             systems);
         return result;
      }

      template <typename... TSystems>
      void registerUserSystemTasks(std::tuple<TSystems...> &systems, vve::World &world,
                                   std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                   vve::detail::WorldRuntimeAccess &world_runtime_access, TaskGraphBuilder &builder) {
         if constexpr (sizeof...(TSystems) == 0) {
            return;
         }

         const auto sync_world_state_task =
             ensureWorldSyncTask(world_windows, input_state, world_runtime_access, builder);

         std::size_t system_index = 0;
         std::apply(
             [&](auto &...system) {
                ([&] {
                   const auto task_name = std::format("task.user_system.{}.update", system_index++);
                   const auto update_task = builder.addTask(task_name, TaskKernelId::none, {}, {sync_world_state_task},
                                                            {}, std::string(system.name()), TaskPhase::user_update);
                   builder.setTaskCallback(
                       update_task,
                       [&world, &system](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                          if (execution_context.frame_context == nullptr || execution_context.window_frame == nullptr ||
                              execution_context.world == nullptr || execution_context.world != &world) {
                             return std::unexpected(vve::Error::invalid_argument);
                          }

                          return invokeUserSystemUpdate(system, *execution_context.world, *execution_context.frame_context,
                                                        *execution_context.window_frame);
                       });
                }(),
                 ...);
             },
              systems);
      }

#ifndef NDEBUG
      inline void registerDebugGraphDumpTask(std::function<const TaskGraph *()> task_graph_accessor,
                                             VectorConstRange<WindowRenderPipeline> render_pipelines,
                                             std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                             vve::detail::WorldRuntimeAccess &world_runtime_access,
                                             TaskGraphBuilder &builder) {
         const auto sync_world_state_task =
             ensureWorldSyncTask(world_windows, input_state, world_runtime_access, builder);

         const auto dump_graph_task =
             builder.addTask("task.debug.dump_graphs", TaskKernelId::none, {}, {sync_world_state_task}, {},
                             "Dump Combined Graph DOT", TaskPhase::user_update);
         builder.setTaskCallback(
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

   } // namespace detail

   export inline [[nodiscard]] std::expected<void, vve::Error>
   executeTaskGraph(const TaskGraph &task_graph, const TaskExecutionContext &execution_context) {
      const auto compiled_task_graph = detail::compileTaskGraph(task_graph);
      if (!compiled_task_graph.valid) {
         return std::unexpected(compiled_task_graph.error);
      }

      return detail::executeCompiledTaskGraph(task_graph, compiled_task_graph, execution_context);
   }

   export template <typename... TUserSystems> class VVE_API BasicEngineImplementation {
   public:
      explicit BasicEngineImplementation(const vve::EngineConfig &config);

      [[nodiscard]] std::expected<void, vve::Error> init();
      [[nodiscard]] std::expected<void, vve::Error> run();
      [[nodiscard]] std::expected<vve::FrameStatus, vve::Error> step();
      [[nodiscard]] std::expected<bool, vve::Error> isInitialized() const noexcept;
      [[nodiscard]] std::expected<int, vve::Error> getVersionMajor() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error> loadSceneFile(const std::filesystem::path &file_path);

   private:
      [[nodiscard]] std::expected<void, vve::Error> rebuildTaskGraph();

      std::string application_name_;
      bool validation_enabled_{false};
      bool initialized_{false};
      bool running_{false};
      std::uint64_t frame_index_{0};
      EngineRuntimeDesc runtime_desc_{};
      detail::Runtime runtime_{};
      std::filesystem::path loaded_file_path_{};
      std::optional<SceneData> scene_{};
      std::optional<TaskGraph> task_graph_{};
      std::optional<detail::CompiledTaskGraph> compiled_task_graph_{};
      bool task_graph_dirty_{true};
      std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> last_time_{};
      vve::ECS<> ecs_{};
      std::vector<vve::WindowInfo> world_windows_{};
      vve::InputState input_state_{};
      vve::detail::WorldRuntimeAccess world_runtime_access_{};
      vve::World world_;
      std::tuple<TUserSystems...> user_systems_{};
   };

   export using EngineImplementation = BasicEngineImplementation<>;
   export using Engine = vve::Engine<EngineImplementation>;

   template <typename... TUserSystems>
   BasicEngineImplementation<TUserSystems...>::BasicEngineImplementation(const vve::EngineConfig &config)
       : application_name_("ViennaVulkanEngine"), validation_enabled_(false), world_(ecs_, world_runtime_access_) {
      if (const auto application_name = config.tryGet<vve::ApplicationName>()) {
         application_name_ = application_name->value;
      }

      if (const auto enable_validation = config.tryGet<vve::EnableValidation>()) {
         validation_enabled_ = enable_validation->value;
      }

      if (const auto graphics_api = config.tryGet<vve::PreferredGraphicsApi>()) {
         runtime_desc_.graphics_api = graphics_api->value;
      }

      if (const auto renderer = config.tryGet<vve::PreferredRenderer>()) {
         runtime_desc_.renderer = renderer->value;
      }

      if (const auto shadow = config.tryGet<vve::PreferredShadow>()) {
         runtime_desc_.shadow = shadow->value;
      }

      if (const auto imgui = config.tryGet<vve::EnableImGui>()) {
         runtime_desc_.imgui_enabled = imgui->value;
      }

      if (const auto windows = config.tryGet<vve::Windows>()) {
         runtime_desc_.windows.clear();
         runtime_desc_.windows.appendRange(windows->value);
      }

      if (const auto task_systems = config.tryGet<vve::v3::TaskSystems>()) {
         runtime_desc_.task_systems = task_systems->value;
      }

      if constexpr (sizeof...(TUserSystems) > 0) {
         if (const auto user_systems = config.tryGet<vve::UserSystems<TUserSystems...>>()) {
            user_systems_ = user_systems->value;
         }
      }
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::init() {
      if (*isInitialized()) {
         return std::unexpected(vve::Error::already_initialized);
      }

      auto runtime = detail::createRuntime(runtime_desc_);
      if (!runtime) {
         return std::unexpected(runtime.error());
      }

      runtime_ = std::move(*runtime);
      if (auto backend_result = runtime_.graphics_backend.init(); !backend_result) {
         return backend_result;
      }

      if (runtime_.gui_system != nullptr) {
         if (auto gui_result = runtime_.gui_system->init(runtime_.graphics_backend); !gui_result) {
            return gui_result;
         }
      }

      loaded_file_path_.clear();
      scene_ = SceneData{};
      task_graph_.reset();
      compiled_task_graph_.reset();
      initialized_ = true;
      running_ = false;
      task_graph_dirty_ = true;
      last_time_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      world_runtime_access_.windows_begin = world_windows_.cbegin();
      world_runtime_access_.windows_end = world_windows_.cend();
      world_runtime_access_.input = &input_state_;
      world_runtime_access_.load_scene = &detail::loadSceneThroughWorld<BasicEngineImplementation<TUserSystems...>>;
      world_runtime_access_.load_scene_context = this;
      detail::syncWorldWindows(*runtime_.window_frame, world_windows_);
      world_runtime_access_.windows_begin = world_windows_.cbegin();
      world_runtime_access_.windows_end = world_windows_.cend();
      detail::syncWorldInput(*runtime_.window_frame, input_state_);

      if (auto user_system_result = detail::initUserSystems(user_systems_, world_); !user_system_result) {
         return user_system_result;
      }

      return {};
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::run() {
      if (!*isInitialized()) {
         if (auto init_result = init(); !init_result) {
            return init_result;
         }
      }

      running_ = true;
      while (running_) {
         if (auto step_result = step(); !step_result) {
            running_ = false;
            return std::unexpected(step_result.error());
         } else if (*step_result == vve::FrameStatus::should_close) {
            running_ = false;
         }
      }

      return {};
   }

   template <typename... TUserSystems>
   std::expected<vve::FrameStatus, vve::Error> BasicEngineImplementation<TUserSystems...>::step() {
      if (!*isInitialized()) {
         return std::unexpected(vve::Error::not_initialized);
      }

      if (!scene_) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto current_time =
          std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      const double seconds_elapsed = std::chrono::duration<double>(current_time - last_time_).count();
      last_time_ = current_time;

      const FrameContext frame_context{.frame_index = frame_index_++, .delta_seconds = seconds_elapsed};

      if (task_graph_dirty_ || !task_graph_) {
         if (auto task_graph_result = rebuildTaskGraph(); !task_graph_result) {
            return std::unexpected(task_graph_result.error());
         }
      } else if (!compiled_task_graph_.has_value()) {
         compiled_task_graph_ = detail::compileTaskGraph(*task_graph_);
      }

      const TaskExecutionContext execution_context{
          .frame_context = &frame_context, .scene = &*scene_, .world = &world_, .window_frame = runtime_.window_frame};
      const auto execute_result = detail::executeCachedTaskGraph(*task_graph_, compiled_task_graph_, execution_context);
      if (!execute_result) {
         return std::unexpected(execute_result.error());
      }

      if (std::ranges::any_of(runtime_.window_frame->windows,
                              [](const WindowState &window) { return window.should_close; })) {
         running_ = false;
         return vve::FrameStatus::should_close;
      }

      return vve::FrameStatus::continue_running;
   }

   template <typename... TUserSystems>
   std::expected<bool, vve::Error> BasicEngineImplementation<TUserSystems...>::isInitialized() const noexcept {
      return initialized_;
   }

   template <typename... TUserSystems>
   std::expected<int, vve::Error> BasicEngineImplementation<TUserSystems...>::getVersionMajor() const noexcept {
      return 3;
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error>
   BasicEngineImplementation<TUserSystems...>::loadSceneFile(const std::filesystem::path &file_path) {
      if (!*isInitialized()) {
         return std::unexpected(vve::Error::not_initialized);
      }

      if (file_path.empty()) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto imported_scene = runtime_.asset_system.importScene(file_path);
      if (!imported_scene) {
         return std::unexpected(imported_scene.error());
      }

      if (auto register_result = runtime_.resource_system.registerImportedScene(*imported_scene, file_path);
          !register_result) {
         return register_result;
      }

      const auto scene = runtime_.scene_system.instantiate(*imported_scene);
      if (!scene) {
         return std::unexpected(scene.error());
      }

      loaded_file_path_ = file_path;
      scene_ = std::move(*scene);
      task_graph_dirty_ = true;
      return rebuildTaskGraph();
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::rebuildTaskGraph() {
      if (!scene_) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      Vector<ITaskSystem *> task_systems{};
      task_systems.reserve(runtime_.task_systems.size());
      for (const auto &task_system : runtime_.task_systems) {
         if (task_system != nullptr) {
            task_systems.push_back(task_system.get());
         }
      }

        auto task_graph = runtime_.task_graph_system.build(
            *scene_, makeRange(task_systems), runtime_.window_system, runtime_.graphics_backend, runtime_.resource_system,
            runtime_.scene_system, *runtime_.render_system,
            [this](TaskGraphBuilder &builder, const SceneData &) {
               detail::registerUserSystemTasks(user_systems_, world_, world_windows_, input_state_,
                                               world_runtime_access_, builder);
#ifndef NDEBUG
               detail::registerDebugGraphDumpTask(
                   [this]() -> const TaskGraph * { return task_graph_ ? &*task_graph_ : nullptr; },
                   makeRange(runtime_.render_pipelines), world_windows_, input_state_, world_runtime_access_, builder);
#endif
            },
            makeRange(runtime_.render_pipelines));
      task_graph_ = std::move(task_graph);
      compiled_task_graph_ = detail::compileTaskGraph(*task_graph_);
      task_graph_dirty_ = false;
      return {};
   }

} // namespace vve::v3
