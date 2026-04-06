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

/**
 * @file
 * @brief Concrete v3 engine implementation behind the public engine facade.
 */
namespace vve::v3 {

   namespace detail {

      /**
       * @brief Adapts the world scene-load callback to a concrete engine instance.
       * @tparam TEngine Engine implementation type receiving the callback.
       * @param context Opaque engine pointer stored in `WorldRuntimeAccess`.
       * @param path Scene path requested through the world facade.
       * @return Empty success result, or an error when the callback context is invalid.
       */
      template <typename TEngine>
      [[nodiscard]] std::expected<void, vve::Error> loadSceneThroughWorld(void *context,
                                                                          const std::filesystem::path &path) {
         // The world facade stores only opaque runtime glue, so validate the
         // callback context before recovering the concrete engine type.
         if (context == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         // Scene loading is ultimately owned by the engine because it touches
         // asset import, resource registration, and graph rebuild state.
         return static_cast<TEngine *>(context)->loadSceneFile(path);
      }

      /**
       * @brief Invokes a user system's optional `init(world)` hook when present.
       * @tparam TSystem User-system type.
       * @param system User-system instance.
       * @param world Game-facing world facade.
       * @return Empty success result, or an error returned by the user system.
       */
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

      /**
       * @brief Invokes a user system's optional update hook using the supported signatures.
       * @tparam TSystem User-system type.
       * @param system User-system instance.
       * @param world Game-facing world facade.
       * @param frame_context Timing data for the current frame.
       * @param window_frame Window snapshot for the current frame.
       * @return Empty success result, or an error returned by the user system.
       */
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

      /**
       * @brief Runs initialization hooks for all registered user systems.
       * @tparam TSystems User-system types stored in the tuple.
       * @param systems Tuple of user-system instances.
       * @param world Game-facing world facade.
       * @return Empty success result, or the first error returned by a user system.
       */
      template <typename... TSystems>
      [[nodiscard]] std::expected<void, vve::Error> initUserSystems(std::tuple<TSystems...> &systems,
                                                                    vve::World &world) {
         std::expected<void, vve::Error> result{};
         // Fold over the tuple so engine startup can stop on the first user
         // system failure without introducing a custom runtime registration API.
         std::apply(
             [&](auto &...system) { ((result = invokeUserSystemInit(system, world)), ...); },
             systems);
         return result;
      }

      /**
       * @brief Registers one task-graph task per user system update hook.
       * @tparam TSystems User-system types stored in the tuple.
       * @param systems Tuple of user-system instances.
       * @param world Game-facing world facade.
       * @param world_windows World-visible window cache.
       * @param input_state World-visible input snapshot.
       * @param world_runtime_access Runtime bridge shared with the world facade.
       * @param builder Task-graph builder receiving the user tasks.
       */
      template <typename... TSystems>
      void registerUserSystemTasks(std::tuple<TSystems...> &systems, vve::World &world,
                                   std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                   vve::detail::WorldRuntimeAccess &world_runtime_access, TaskGraphBuilder &builder) {
         // Skip all graph work entirely when the engine was instantiated
         // without compile-time user systems.
         if constexpr (sizeof...(TSystems) == 0) {
            return;
         }

         // User tasks run only after the world facade has been synchronized
         // with the current frame's window and input snapshots.
         const auto sync_world_state_task =
             ensureWorldSyncTask(world_windows, input_state, world_runtime_access, builder);

         std::size_t system_index = 0;
         std::apply(
             [&](auto &...system) {
                ([&] {
                   // Stable task names make user-system tasks debuggable and
                   // deterministic across task-graph rebuilds.
                   const auto task_name = std::format("task.user_system.{}.update", system_index++);
                   const auto update_task = builder.addTask(task_name, TaskKernelId::none, {}, {sync_world_state_task},
                                                            {}, std::string(system.name()), TaskPhase::user_update);
                   [[maybe_unused]] const auto callback_set = builder.setTaskCallback(
                       update_task,
                       [&world, &system](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                          // User hooks rely on frame timing, the synchronized
                          // world facade, and the current window snapshot.
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

   } // namespace detail

   /**
    * @brief Compiles and executes a task graph immediately.
    * @param task_graph Declarative task graph to execute.
    * @param execution_context Runtime data passed to task callbacks.
    * @return Empty success result, or an execution/validation error.
    */
   export inline [[nodiscard]] std::expected<void, vve::Error>
   executeTaskGraph(const TaskGraph &task_graph, const TaskExecutionContext &execution_context) {
      // Validate dependencies before callbacks execute.
      const auto compiled_task_graph = detail::compileTaskGraph(task_graph);
      if (!compiled_task_graph.valid) {
         return std::unexpected(compiled_task_graph.error);
      }

      return detail::executeCompiledTaskGraph(task_graph, compiled_task_graph, execution_context);
   }

   /**
    * @brief Concrete v3 engine implementation.
    * @tparam TUserSystems User-system types wired into the engine at compile time.
    */
   export template <typename... TUserSystems> class VVE_API BasicEngineImplementation {
   public:
      /**
       * @brief Creates the engine implementation from a public engine configuration.
       * @param config Type-indexed engine configuration options.
       */
      explicit BasicEngineImplementation(const vve::EngineConfig &config);

      /**
       * @brief Initializes runtime subsystems and prepares the engine for execution.
       * @return Empty success result, or an initialization error.
       */
      [[nodiscard]] std::expected<void, vve::Error> init();
      /**
       * @brief Runs the engine until shutdown is requested.
       * @return Empty success result, or the first runtime error.
       */
      [[nodiscard]] std::expected<void, vve::Error> run();
      /**
       * @brief Executes one engine frame.
       * @return Frame status on success, or an execution error.
       */
      [[nodiscard]] std::expected<vve::FrameStatus, vve::Error> step();
      /**
       * @brief Returns whether initialization has completed.
       * @return `true` when initialized, otherwise `false`.
       */
      [[nodiscard]] std::expected<bool, vve::Error> isInitialized() const noexcept;
      /**
       * @brief Returns the major engine version number.
       * @return Major version number on success.
       */
      [[nodiscard]] std::expected<int, vve::Error> getVersionMajor() const noexcept;
      /**
       * @brief Imports and instantiates a scene file.
       * @param file_path Scene file path to load.
       * @return Empty success result, or a loading/instantiation error.
       */
      [[nodiscard]] std::expected<void, vve::Error> loadSceneFile(const std::filesystem::path &file_path);

   private:
      /**
       * @brief Rebuilds the frame task graph from the current runtime state.
       * @return Empty success result, or a graph-construction error.
       */
      [[nodiscard]] std::expected<void, vve::Error> rebuildTaskGraph();

      std::string application_name_;             ///< Human-readable application name.
      bool validation_enabled_{false};           ///< Enables additional runtime validation where supported.
      bool initialized_{false};                  ///< Tracks whether initialization has completed.
      bool running_{false};                      ///< Tracks whether the main loop is currently running.
      std::uint64_t frame_index_{0};             ///< Monotonically increasing frame index.
      EngineRuntimeDesc runtime_desc_{};         ///< Runtime descriptor assembled from engine configuration.
      detail::Runtime runtime_{};                ///< Concrete runtime object holding subsystem facades.
      std::filesystem::path loaded_file_path_{}; ///< Last successfully loaded scene path.
      std::optional<SceneData> scene_{};         ///< Current instantiated scene data.
      std::optional<TaskGraph> task_graph_{};    ///< Current declarative task graph.
      /// @brief Cached compiled execution plan for the current task graph.
      std::optional<detail::CompiledTaskGraph> compiled_task_graph_{};
      bool task_graph_dirty_{true}; ///< Tracks whether the task graph must be rebuilt.
      /// @brief Timestamp used to compute per-frame delta time.
      std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> last_time_{};
      vve::ECS<> ecs_{};                                       ///< World ECS storage used by the engine.
      std::vector<vve::WindowInfo> world_windows_{};           ///< World-visible window cache synchronized each frame.
      vve::InputState input_state_{};                          ///< World-visible input snapshot synchronized each frame.
      vve::detail::WorldRuntimeAccess world_runtime_access_{}; ///< Runtime bridge shared with the world facade.
      vve::World world_;                                       ///< Game-facing world facade exposed to user systems.
      std::tuple<TUserSystems...> user_systems_{};             ///< Compile-time typed tuple of user-system instances.
   };

   export using EngineImplementation = BasicEngineImplementation<>;
   export using Engine = vve::Engine<EngineImplementation>;

   template <typename... TUserSystems>
   BasicEngineImplementation<TUserSystems...>::BasicEngineImplementation(const vve::EngineConfig &config)
       : application_name_("ViennaVulkanEngine"), validation_enabled_(false), world_(ecs_, world_runtime_access_) {
      // Apply optional public config values onto the runtime descriptor while
      // keeping sensible defaults for omitted settings.
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
         // User systems are wired at compile time but instantiated from config
         // so applications can still provide custom system state.
         if (const auto user_systems = config.tryGet<vve::UserSystems<TUserSystems...>>()) {
            user_systems_ = user_systems->value;
         }
      }
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::init() {
      if (*isInitialized()) { // Initialization is a one-way transition to a prepared runtime state.
         return std::unexpected(vve::Error::already_initialized);
      }

      auto runtime = detail::createRuntime(runtime_desc_); // Assemble the subsystem facades selected by config.
      if (!runtime) {
         return std::unexpected(runtime.error());
      }

      runtime_ = std::move(*runtime);
      // Initialize the backend before dependent systems such as GUI.
      if (auto backend_result = runtime_.graphics_backend.init(); !backend_result) {
         return backend_result;
      }

      if (runtime_.gui_system != nullptr) { // GUI support is optional and initialized only when present.
         if (auto gui_result = runtime_.gui_system->init(runtime_.graphics_backend); !gui_result) {
            return gui_result;
         }
      }

      loaded_file_path_.clear(); // Reset frame-owned state so a fresh runtime starts from a clean scene.
      scene_ = SceneData{};
      task_graph_.reset();
      compiled_task_graph_.reset();
      initialized_ = true;
      running_ = false;
      task_graph_dirty_ = true;
      last_time_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      // Wire the world facade to the runtime-owned window/input snapshots and
      // to the engine-owned scene-loading callback.
      world_runtime_access_.windows_begin = world_windows_.cbegin();
      world_runtime_access_.windows_end = world_windows_.cend();
      world_runtime_access_.input = &input_state_;
      world_runtime_access_.load_scene = &detail::loadSceneThroughWorld<BasicEngineImplementation<TUserSystems...>>;
      world_runtime_access_.load_scene_context = this;
      // Prime world-visible caches before user init hooks run.
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
      if (!*isInitialized()) { // `run` bootstraps initialization if the caller has not done it manually.
         if (auto init_result = init(); !init_result) {
            return init_result;
         }
      }

      running_ = true;
      while (running_) {
         // Stop immediately on execution errors so the caller sees the root cause.
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
      if (!*isInitialized()) { // Per-frame execution requires an initialized runtime.
         return std::unexpected(vve::Error::not_initialized);
      }

      if (!scene_) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto current_time = // Capture frame timing once and pass it into all scheduled work.
          std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      const double seconds_elapsed = std::chrono::duration<double>(current_time - last_time_).count();
      last_time_ = current_time;

      const FrameContext frame_context{.frame_index = frame_index_++, .delta_seconds = seconds_elapsed};

      // Rebuild the task graph only when scene/runtime state invalidates the
      // previously compiled execution plan.
      if (task_graph_dirty_ || !task_graph_) {
         if (auto task_graph_result = rebuildTaskGraph(); !task_graph_result) {
            return std::unexpected(task_graph_result.error());
         }
      } else if (!compiled_task_graph_.has_value()) {
          // Recompile lazily if only the cached plan is missing.
          compiled_task_graph_ = detail::compileTaskGraph(*task_graph_);
      }

      const TaskExecutionContext execution_context{ // Frame-local data shared by all task callbacks.
          .frame_context = &frame_context, .scene = &*scene_, .world = &world_, .window_frame = runtime_.window_frame};
      const auto execute_result = detail::executeCachedTaskGraph(*task_graph_, compiled_task_graph_, execution_context);
      if (!execute_result) {
         return std::unexpected(execute_result.error());
      }

      if (std::ranges::any_of(runtime_.window_frame->windows, // Treat close requests as a frame result, not an error.
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
      if (!*isInitialized()) { // Scene loading spans multiple subsystems and therefore requires a live runtime.
         return std::unexpected(vve::Error::not_initialized);
      }

      if (file_path.empty()) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      // Import converts source data into engine-owned intermediate scene data.
      const auto imported_scene = runtime_.asset_system.importScene(file_path);
      if (!imported_scene) {
         return std::unexpected(imported_scene.error());
      }

      // Resource registration assigns stable engine handles before instantiation.
      if (auto register_result = runtime_.resource_system.registerImportedScene(*imported_scene, file_path);
          !register_result) {
         return register_result;
      }

      // Scene instantiation produces the runtime scene representation consumed by systems.
      const auto scene = runtime_.scene_system.instantiate(*imported_scene);
      if (!scene) {
         return std::unexpected(scene.error());
      }

      loaded_file_path_ = file_path; // Changing the scene invalidates the previously built frame graph.
      scene_ = std::move(*scene);
      task_graph_dirty_ = true;
      return rebuildTaskGraph();
   }

   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::rebuildTaskGraph() {
      // Graph construction requires an instantiated scene because many tasks
      // depend on scene-owned nodes and resources.
      if (!scene_) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      // Flatten owning task-system pointers into a non-owning range expected by
      // the task-graph facade.
      Vector<ITaskSystem *> task_systems{};
      task_systems.reserve(runtime_.task_systems.size());
      for (const auto &task_system : runtime_.task_systems) {
         if (task_system != nullptr) { // Expose a compact non-owning range to the task-graph builder.
            task_systems.push_back(task_system.get());
         }
      }

      // Let the task-graph subsystem assemble the built-in frame tasks, then
      // inject engine-owned extensions such as user-system tasks and debug dumps.
      auto task_graph = runtime_.task_graph_system.build(
          *scene_, makeRange(task_systems), runtime_.window_system, runtime_.graphics_backend, runtime_.resource_system,
          runtime_.scene_system, *runtime_.render_system,
          [this](TaskGraphBuilder &builder, const SceneData &) {
             detail::registerUserSystemTasks(user_systems_, world_, world_windows_, input_state_,
                                             world_runtime_access_, builder);
#ifndef NDEBUG
             detail::registerDebugGraphDumpTask( // Debug builds expose an extra graph-dump task for inspection.
                 [this]() -> const TaskGraph * { return task_graph_ ? &*task_graph_ : nullptr; },
                 makeRange(runtime_.render_pipelines), world_windows_, input_state_, world_runtime_access_, builder);
#endif
          },
          makeRange(runtime_.render_pipelines));
      task_graph_ = std::move(task_graph); // Cache both the declarative graph and its compiled execution plan.
      compiled_task_graph_ = detail::compileTaskGraph(*task_graph_);
      task_graph_dirty_ = false;
      return {};
   }

} // namespace vve::v3
