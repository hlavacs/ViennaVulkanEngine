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
export import :Types;
export import :Systems;
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
       * @brief Adapts the world camera callback to a concrete engine instance.
       * @tparam TEngine Engine implementation type receiving the callback.
       * @param context Opaque engine pointer stored in `WorldRuntimeAccess`.
       * @param camera Public camera description supplied by game code.
       * @return Empty success result, or an error when the callback context is invalid.
       */
      template <typename TEngine>
      [[nodiscard]] std::expected<void, vve::Error> setCameraThroughWorld(void *context,
                                                                          const vve::Camera &camera) {
         if (context == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return static_cast<TEngine *>(context)->setActiveCamera(camera);
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
                   [[maybe_unused]] const auto update_task = builder.addTask(
                       task_name, TaskKernelId::none,
                       requireFrameWindowFrameWorld([&world, &system](const FrameContext &frame_context,
                                                                      const WindowFrameData &window_frame,
                                                                      vve::World &callback_world)
                                                        -> std::expected<void, vve::Error> {
                          // User hooks rely on the engine-owned world facade rather than any unrelated world instance.
                          if (&callback_world != &world) {
                             return std::unexpected(vve::Error::invalid_argument);
                          }

                          return invokeUserSystemUpdate(system, callback_world, frame_context, window_frame);
                       }),
                       {sync_world_state_task}, {}, std::string(system.name()), TaskPhase::user_update);
                }(),
                 ...);
             },
             systems);
      }

      /// @brief Engine-owned lifecycle flags and user-facing configuration values.
      struct EngineLifecycleState {
         std::string application_name{"ViennaVulkanEngine"}; ///< Human-readable application name.
         bool validation_enabled{false};                     ///< Enables additional runtime validation where supported.
         bool initialized{false};                            ///< Tracks whether initialization has completed.
         bool running{false};                                ///< Tracks whether the main loop is currently running.
      };

      /// @brief Active-scene state owned by the engine.
      struct SceneRuntimeState {
         std::filesystem::path loaded_file_path{}; ///< Last successfully loaded scene path.
         std::optional<SceneData> scene{};         ///< Current instantiated scene data.
      };

      /// @brief Frame-execution caches and timing state owned by the engine.
      struct FrameExecutionState {
         std::uint64_t frame_index{0};                               ///< Monotonically increasing frame index.
         std::optional<TaskGraph> task_graph{};                      ///< Current declarative task graph.
         std::optional<detail::CompiledTaskGraph> compiled_task_graph{}; ///< Cached compiled execution plan.
         bool task_graph_dirty{true};                               ///< Tracks whether the task graph must be rebuilt.
         std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> last_time{}; ///< Timestamp used to compute per-frame delta time.
      };

      /// @brief World-facing caches and runtime bridge owned by the engine.
      struct WorldBridgeState {
         vve::ECS<> ecs{};                                  ///< World ECS storage used by the engine.
         std::vector<vve::WindowInfo> windows{};            ///< World-visible window cache synchronized each frame.
         vve::InputState input{};                           ///< World-visible input snapshot synchronized each frame.
         vve::detail::WorldRuntimeAccess runtime_access{}; ///< Runtime bridge shared with the world facade.
         std::unique_ptr<vve::World> world{};               ///< Game-facing world facade exposed to user systems.
      };

   } // namespace detail

   /**
    * @brief Compiles and executes a task graph immediately.
    * @param task_graph Declarative task graph to execute.
    * @param execution_context Runtime data passed to task callbacks.
    * @return Empty success result, or an execution/validation error.
    */
   export [[nodiscard]] inline std::expected<void, vve::Error>
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
      explicit BasicEngineImplementation(const vve::EngineConfig &config);

      [[nodiscard]] std::expected<void, vve::Error> init();
      [[nodiscard]] std::expected<void, vve::Error> run();
      [[nodiscard]] std::expected<vve::FrameStatus, vve::Error> step();
      [[nodiscard]] std::expected<bool, vve::Error> isInitialized() const noexcept;
      [[nodiscard]] std::expected<int, vve::Error> getVersionMajor() const noexcept;
      [[nodiscard]] std::expected<void, vve::Error> loadSceneFile(const std::filesystem::path &file_path);
      [[nodiscard]] std::expected<void, vve::Error> setActiveCamera(const vve::Camera &camera);

   private:
      [[nodiscard]] std::expected<void, vve::Error> rebuildTaskGraph();

      EngineRuntimeDesc runtime_desc_{};         ///< Runtime descriptor assembled from engine configuration.
      detail::Runtime runtime_{};                ///< Concrete runtime object holding subsystem facades.
      detail::EngineLifecycleState lifecycle_{};      ///< Engine lifecycle flags and config-derived values.
      detail::SceneRuntimeState scene_state_{};       ///< Active-scene state owned by the engine.
      detail::FrameExecutionState execution_state_{}; ///< Frame-execution caches and timing state.
      detail::WorldBridgeState world_bridge_{};       ///< World-facing caches and runtime bridge.
      std::tuple<TUserSystems...> user_systems_{};    ///< Compile-time typed tuple of user-system instances.
   };

   export using Engine = vve::Engine<BasicEngineImplementation<>>;

   /**
    * @brief Creates the engine implementation from a public engine configuration.
    * @param config Type-indexed engine configuration options.
    */
   template <typename... TUserSystems>
   BasicEngineImplementation<TUserSystems...>::BasicEngineImplementation(const vve::EngineConfig &config) {
      // Apply optional public config values onto the runtime descriptor while
      // keeping sensible defaults for omitted settings.
      world_bridge_.world = std::make_unique<vve::World>(world_bridge_.ecs, world_bridge_.runtime_access);

      if (const auto application_name = config.tryGet<vve::ApplicationName>()) {
         lifecycle_.application_name = application_name->value;
      }

      if (const auto enable_validation = config.tryGet<vve::EnableValidation>()) {
         lifecycle_.validation_enabled = enable_validation->value;
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

      const auto default_renderer_id = std::string(vve::rendererKindName(runtime_desc_.renderer));
      for (auto &window : runtime_desc_.windows) {
         if (window.renderer_id.empty()) {
            window.renderer_id = default_renderer_id;
         }
      }
   }

   /**
    * @brief Initializes runtime subsystems and prepares the engine for execution.
    * @return Empty success result, or an initialization error.
    */
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
      detail::rebindRuntimeReferences(runtime_);
      const auto instance_extensions = runtime_.window_system.vulkanInstanceExtensions();
      if (!instance_extensions) {
         return std::unexpected(instance_extensions.error());
      }

      // Initialize the backend with the platform presentation extensions before dependent systems such as GUI.
      if (auto backend_result = runtime_.graphics_backend.init(*instance_extensions); !backend_result) {
         return backend_result;
      }

      if (auto pipeline_resources = detail::createRuntimePipelineResources(runtime_); !pipeline_resources) {
         return pipeline_resources;
      }
      if (auto swapchains = detail::createRuntimeWindowSwapchains(runtime_); !swapchains) {
         return swapchains;
      }
      if (auto renderer_bindings = detail::bindRuntimeRendererPipelines(runtime_); !renderer_bindings) {
         return renderer_bindings;
      }
      if (auto graphics_pipelines = detail::createRuntimeGraphicsPipelines(runtime_); !graphics_pipelines) {
         return graphics_pipelines;
      }

      if (runtime_.gui_system != nullptr) { // GUI support is optional and initialized only when present.
         if (auto gui_result = runtime_.gui_system->init(runtime_.graphics_backend); !gui_result) {
            return gui_result;
         }
      }

      scene_state_.loaded_file_path.clear(); // Reset frame-owned state so a fresh runtime starts from a clean scene.
      scene_state_.scene = SceneData{};
      execution_state_.task_graph.reset();
      execution_state_.compiled_task_graph.reset();
      lifecycle_.initialized = true;
      lifecycle_.running = false;
      execution_state_.task_graph_dirty = true;
      execution_state_.last_time =
          std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      // Wire the world facade to the runtime-owned window/input snapshots and
      // to the engine-owned scene-loading callback.
      world_bridge_.runtime_access.windows_begin = world_bridge_.windows.cbegin();
      world_bridge_.runtime_access.windows_end = world_bridge_.windows.cend();
      world_bridge_.runtime_access.input = &world_bridge_.input;
      world_bridge_.runtime_access.load_scene = &detail::loadSceneThroughWorld<BasicEngineImplementation<TUserSystems...>>;
      world_bridge_.runtime_access.load_scene_context = this;
      world_bridge_.runtime_access.set_camera =
          &detail::setCameraThroughWorld<BasicEngineImplementation<TUserSystems...>>;
      world_bridge_.runtime_access.set_camera_context = this;
      // Prime world-visible caches before user init hooks run.
      detail::syncWorldWindows(*runtime_.window_frame, world_bridge_.windows);
      world_bridge_.runtime_access.windows_begin = world_bridge_.windows.cbegin();
      world_bridge_.runtime_access.windows_end = world_bridge_.windows.cend();
      detail::syncWorldInput(*runtime_.window_frame, world_bridge_.input);

      if (world_bridge_.world == nullptr) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      if (auto user_system_result = detail::initUserSystems(user_systems_, *world_bridge_.world); !user_system_result) {
         return user_system_result;
      }

      return {};
   }

   /**
    * @brief Runs the engine until shutdown is requested.
    * @return Empty success result, or the first runtime error.
    */
   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::run() {
      if (!*isInitialized()) { // `run` bootstraps initialization if the caller has not done it manually.
         if (auto init_result = init(); !init_result) {
            return init_result;
         }
      }

      lifecycle_.running = true;
      while (lifecycle_.running) {
         // Stop immediately on execution errors so the caller sees the root cause.
         if (auto step_result = step(); !step_result) {
            lifecycle_.running = false;
            return std::unexpected(step_result.error());
         } else if (*step_result == vve::FrameStatus::should_close) {
            lifecycle_.running = false;
         }
      }

      return {};
   }

   /**
    * @brief Executes one engine frame.
    * @return Frame status on success, or an execution error.
    */
   template <typename... TUserSystems>
   std::expected<vve::FrameStatus, vve::Error> BasicEngineImplementation<TUserSystems...>::step() {
      if (!*isInitialized()) { // Per-frame execution requires an initialized runtime.
         return std::unexpected(vve::Error::not_initialized);
      }

      if (!scene_state_.scene) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto current_time = // Capture frame timing once and pass it into all scheduled work.
          std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now());
      const double seconds_elapsed = std::chrono::duration<double>(current_time - execution_state_.last_time).count();
      execution_state_.last_time = current_time;

      const FrameContext frame_context{.frame_index = execution_state_.frame_index++, .delta_seconds = seconds_elapsed};

      // Rebuild the task graph only when scene/runtime state invalidates the
      // previously compiled execution plan.
      if (execution_state_.task_graph_dirty || !execution_state_.task_graph) {
         if (auto task_graph_result = rebuildTaskGraph(); !task_graph_result) {
            return std::unexpected(task_graph_result.error());
         }
      } else if (!execution_state_.compiled_task_graph.has_value()) {
          // Recompile lazily if only the cached plan is missing.
          execution_state_.compiled_task_graph = detail::compileTaskGraph(*execution_state_.task_graph);
      }

      const TaskExecutionContext execution_context{ // Frame-local data shared by all task callbacks.
          .frame_context = &frame_context,
          .scene = &*scene_state_.scene,
          .world = world_bridge_.world.get(),
          .window_frame = runtime_.window_frame};
      const auto execute_result = detail::executeCachedTaskGraph(*execution_state_.task_graph,
                                                                 execution_state_.compiled_task_graph,
                                                                 execution_context);
      if (!execute_result) {
         return std::unexpected(execute_result.error());
      }
      if (auto swapchain_update = detail::updateRuntimeWindowSwapchainsAfterFrame(runtime_); !swapchain_update) {
         return std::unexpected(swapchain_update.error());
      }

      if (std::ranges::any_of(runtime_.window_frame->windows, // Treat close requests as a frame result, not an error.
                              [](const WindowState &window) { return window.should_close; })) {
         lifecycle_.running = false;
         return vve::FrameStatus::should_close;
      }

      return vve::FrameStatus::continue_running;
   }

   /**
    * @brief Returns whether initialization has completed.
    * @return `true` when initialized, otherwise `false`.
    */
   template <typename... TUserSystems>
   std::expected<bool, vve::Error> BasicEngineImplementation<TUserSystems...>::isInitialized() const noexcept {
      return lifecycle_.initialized;
   }

   /**
    * @brief Returns the major engine version number.
    * @return Major version number on success.
    */
   template <typename... TUserSystems>
   std::expected<int, vve::Error> BasicEngineImplementation<TUserSystems...>::getVersionMajor() const noexcept {
      return 3;
   }

   /**
    * @brief Imports and instantiates a scene file.
    * @param file_path Scene file path to load.
    * @return Empty success result, or a loading/instantiation error.
    */
   template <typename... TUserSystems>
   std::expected<void, vve::Error>
   BasicEngineImplementation<TUserSystems...>::loadSceneFile(const std::filesystem::path &file_path) {
      if (!*isInitialized()) { // Scene loading spans multiple subsystems and therefore requires a live runtime.
         return std::unexpected(vve::Error::not_initialized);
      }

      if (file_path.empty()) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      if (runtime_.scene_loader == nullptr) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      // Scene loading now lives behind a dedicated orchestration subsystem so
      // the engine keeps only active-scene ownership and graph invalidation.
      const auto scene = runtime_.scene_loader->loadScene(file_path);
      if (!scene) {
         return std::unexpected(scene.error());
      }

      scene_state_.loaded_file_path = file_path; // Changing the scene invalidates the previously built frame graph.
      scene_state_.scene = std::move(*scene);
      execution_state_.task_graph_dirty = true;
      return rebuildTaskGraph();
   }

   /**
    * @brief Updates the active scene camera from game-facing world code.
    * @param camera Public camera description supplied through `World`.
    * @return Empty success result, or an error when no scene is active.
    */
   template <typename... TUserSystems>
   std::expected<void, vve::Error>
   BasicEngineImplementation<TUserSystems...>::setActiveCamera(const vve::Camera &camera) {
      if (!*isInitialized() || !scene_state_.scene) {
         return std::unexpected(vve::Error::not_initialized);
      }

      scene_state_.scene->active_camera = CameraFrameData{.position = camera.position,
                                                          .view_transform = camera.view_transform,
                                                          .vertical_fov_radians = camera.vertical_fov_radians,
                                                          .near_plane = camera.near_plane,
                                                          .far_plane = camera.far_plane};
      return {};
   }

   /**
    * @brief Rebuilds the frame task graph from the current runtime state.
    * @return Empty success result, or a graph-construction error.
    */
   template <typename... TUserSystems>
   std::expected<void, vve::Error> BasicEngineImplementation<TUserSystems...>::rebuildTaskGraph() {
      // Graph construction requires an instantiated scene because many tasks
      // depend on scene-owned nodes and resources.
      if (!scene_state_.scene) {
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
          *scene_state_.scene, makeRange(task_systems), runtime_.window_system, runtime_.graphics_backend, runtime_.resource_system,
          runtime_.scene_system, *runtime_.render_system,
          [this](TaskGraphBuilder &builder, const SceneData &) {
             detail::registerUserSystemTasks(user_systems_, *world_bridge_.world, world_bridge_.windows,
                                             world_bridge_.input, world_bridge_.runtime_access, builder);
#ifndef NDEBUG
             detail::registerDebugGraphDumpTask( // Debug builds expose an extra graph-dump task for inspection.
                 [this]() -> const TaskGraph * {
                    return execution_state_.task_graph ? &*execution_state_.task_graph : nullptr;
                 },
                 makeRange(runtime_.render_pipelines), world_bridge_.windows, world_bridge_.input,
                 world_bridge_.runtime_access, builder);
#endif
          },
          makeRange(runtime_.render_pipelines));
      execution_state_.task_graph = std::move(task_graph); // Cache both the declarative graph and its compiled execution plan.
      execution_state_.compiled_task_graph = detail::compileTaskGraph(*execution_state_.task_graph);
      execution_state_.task_graph_dirty = false;
      return {};
   }

} // namespace vve::v3
