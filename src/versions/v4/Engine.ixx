module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine.V4;
import std;
export import VEEngine.V4.Math;
export import :Graph;
export import :ECS;
export import :Window;
export import :Assets;
export import :RenderSystem;
export import :Resources;
export import :Shaders;
export import VEEngine.V4.Handle;
export import :Gui;

/// @file
/// @brief Small v4 runtime facade: SDL windows, input, graphs, and stub subsystems.

export namespace vve::v4 {

   struct TaskHandleTag {}; ///< v4 CPU task graph node handle tag.

   using TaskHandle = TypedHandle<TaskHandleTag>; ///< v4 CPU task graph node handle.

   /// @brief User-system task names supplied by the facade for graph dumps.
   struct UserSystemTasks {
      Vector<ObjectName> value{}; ///< Task names already formatted for the task graph.
   };

} // namespace vve::v4

export namespace vve::v4 {

   using TaskGraph = Graph<TaskHandle>; ///< Generic DAG for CPU frame-task dependencies.

   namespace detail {

      inline constexpr std::int32_t debugDumpGraphHotkey{0x40000042}; ///< SDL keycode for F9.

   } // namespace detail

   /// @brief Educational v4 engine shell with SDL windows and lightweight subsystems.
   class Engine {
   public:
      Engine();
      Engine(const Engine &) = delete;
      Engine(Engine &&) = delete;
      Engine &operator=(const Engine &) = delete;
      Engine &operator=(Engine &&) = delete;
      explicit Engine(EngineConfig config);

      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit Engine(TOptions &&...options);

      [[nodiscard]] std::uint32_t versionMajor() const;
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept;
      [[nodiscard]] std::string_view versionName() const;
      [[nodiscard]] AssetSystem &assets();
      [[nodiscard]] RenderSystem &renderSystem();
      [[nodiscard]] const RenderSystem &renderSystem() const;
      [[nodiscard]] GuiSystem &gui();
      [[nodiscard]] ECS &ecs();
      [[nodiscard]] WindowSystem &windowSystem();
      [[nodiscard]] const WindowSystem &windowSystem() const;
      [[nodiscard]] std::expected<void, Error> init();
      [[nodiscard]] std::expected<void, Error> run();
      [[nodiscard]] std::expected<FrameStatus, Error> step();
      [[nodiscard]] std::expected<void, Error>
      writeDebugGraphs(const std::filesystem::path &directory = "graph_dumps") const;

   private:
      template <typename TOption> void applyOption(TOption &&);
      void applyDefaults();
      [[nodiscard]] std::expected<void, Error> buildDefaultGraphs();
      [[nodiscard]] static std::string graphFileStem(std::string_view text);

      ApplicationName application_name_{};      ///< Name used for default window titles.
      MaxFrames max_frames_{};                 ///< Optional frame cap.
      Windows windows_{};                      ///< Startup window descriptors.
      ECS ecs_{};                              ///< Runtime entity/component storage owned by the engine.
      WindowSystem window_system_{};           ///< SDL platform window owner.
      AssetSystem assets_{};                   ///< Asset and object catalog facade.
      RenderSystem render_system_{};           ///< Renderer selection and active CPU render scene.
      ResourceSystem resources_{};             ///< Resource descriptor facade.
      TaskGraph tasks_{};                      ///< CPU task graph facade.
      RenderGraph render_graph_{};             ///< Render pass graph facade.
      ShaderSystem shaders_{};                 ///< Shader descriptor facade.
      GuiSystem gui_{};                        ///< GUI descriptor facade.
      Vector<ObjectName> user_system_tasks_{}; ///< User-system task names supplied by the facade.
      std::chrono::steady_clock::time_point last_frame_time_{}; ///< Timestamp of the previous step().
      std::uint64_t frame_{0};                 ///< Number of completed step() calls.
      bool initialized_{false};                ///< True after init() succeeds.
   };

   /// @brief Creates an engine with default options.
   inline Engine::Engine() {
      applyDefaults();
   }

   /// @brief Creates an engine from the compact compatibility config.
   inline Engine::Engine(EngineConfig config) {
      applyOption(std::move(config));
      applyDefaults();
   }

   /// @brief Creates an engine from typed options such as ApplicationName, Windows, and UserSystemTasks.
   template <typename... TOptions>
      requires(sizeof...(TOptions) > 0)
   Engine::Engine(TOptions &&...options) {
      (applyOption(std::forward<TOptions>(options)), ...);
      applyDefaults();
   }

   /// @brief Returns the major engine version.
   inline std::uint32_t Engine::versionMajor() const { return 4; }

   /// @brief Returns the major engine version through a v3-shaped accessor.
   inline std::expected<int, Error> Engine::getVersionMajor() const noexcept {
      return 4;
   }

   /// @brief Returns the printable engine version name.
   inline std::string_view Engine::versionName() const { return "v4"; }

   /// @brief Returns the asset system.
   inline AssetSystem &Engine::assets() { return assets_; }

   /// @brief Returns the render system.
   inline RenderSystem &Engine::renderSystem() { return render_system_; }

   /// @brief Returns the render system.
   inline const RenderSystem &Engine::renderSystem() const { return render_system_; }

   /// @brief Returns the GUI system.
   inline GuiSystem &Engine::gui() { return gui_; }

   /// @brief Returns the runtime ECS.
   inline ECS &Engine::ecs() { return ecs_; }

   /// @brief Returns the implementation window system.
   inline WindowSystem &Engine::windowSystem() { return window_system_; }

   /// @brief Returns the implementation window system.
   inline const WindowSystem &Engine::windowSystem() const {
      return window_system_;
   }

   /// @brief Creates SDL windows and default graphs.
   inline std::expected<void, Error> Engine::init() {
      if (initialized_) { return {}; }
      if (const auto result = window_system_.init(windows_); !result) { return result; }
      if (const auto result = buildDefaultGraphs(); !result) { return result; }
      last_frame_time_ = std::chrono::steady_clock::now();
      initialized_ = true;
      return {};
   }

   /// @brief Runs the engine until a window closes, a system fails, or the frame cap is reached.
   inline std::expected<void, Error> Engine::run() {
      if (!initialized_) {
         if (const auto result = init(); !result) { return result; }
      }
      while (true) {
         const auto status = step();
         if (!status) { return std::unexpected(status.error()); }
         if (*status == FrameStatus::stopped) { return {}; }
      }
   }

   /// @brief Polls input and advances the frame status.
   inline std::expected<FrameStatus, Error> Engine::step() {
      if (!initialized_) { return std::unexpected(Error::missing_object); }
      if (const auto result = window_system_.poll(); !result) { return std::unexpected(result.error()); }
      if (window_system_.input().wasKeyPressed(detail::debugDumpGraphHotkey)) {
         if (const auto result = writeDebugGraphs(); !result) { return std::unexpected(result.error()); }
      }

      const auto now = std::chrono::steady_clock::now();
      last_frame_time_ = now;

      ++frame_;
      if (window_system_.anyShouldClose() || (max_frames_.value.value > 0 && frame_ >= max_frames_.value.value)) {
         return FrameStatus::stopped;
      }
      return FrameStatus::running;
   }

   /// @brief Applies typed engine options; unknown option types are ignored.
   template <typename TOption>
   void Engine::applyOption(TOption &&option) {
      using Option = std::remove_cvref_t<TOption>;
      if constexpr (std::same_as<Option, EngineConfig>) {
         auto config = std::forward<TOption>(option);
         application_name_.value = std::move(config.application_name);
         max_frames_.value = config.max_frames;
      } else if constexpr (std::same_as<Option, ApplicationName>) {
         application_name_ = std::forward<TOption>(option);
      } else if constexpr (std::same_as<Option, MaxFrames>) {
         max_frames_ = std::forward<TOption>(option);
      } else if constexpr (std::same_as<Option, Windows>) {
         windows_ = std::forward<TOption>(option);
      } else if constexpr (std::same_as<Option, UserSystemTasks>) {
         user_system_tasks_ = std::forward<TOption>(option).value;
      }
   }

   /// @brief Fills small defaults after options have been applied.
   inline void Engine::applyDefaults() {
      if (windows_.value.empty()) { windows_.value.push_back(WindowDesc{}); }
      for (auto &window : windows_.value) {
         if (window.title == WindowDesc{}.title && application_name_.value != ApplicationName{}.value) {
            window.title = application_name_.value;
         }
      }
   }

   /// @brief Builds simple inspectable default graphs for debug dumps and teaching.
   inline std::expected<void, Error> Engine::buildDefaultGraphs() {
      tasks_ = {};
      render_graph_ = {};

      const auto begin = tasks_.addNode(ObjectName{.value = "task.frame_begin"});
      const auto poll = tasks_.addNode(ObjectName{.value = "task.poll_window_events"});
      const auto render = tasks_.addNode(ObjectName{.value = "task.render_graph"});
      const auto finish = tasks_.addNode(ObjectName{.value = "task.frame_finished"});
      if (!begin || !poll || !render || !finish) { return std::unexpected(Error::internal_error); }

      tasks_.addEdge(*begin, *poll);
      auto previous = *poll;
      for (const auto &task_name : user_system_tasks_) {
         const auto task = tasks_.addNode(task_name);
         if (!task) { return std::unexpected(task.error()); }
         tasks_.addEdge(previous, *task);
         previous = *task;
      }
      tasks_.addEdge(previous, *render);
      tasks_.addEdge(*render, *finish);

      const auto renderer = render_system_.createForwardRenderer();
      const std::array pass_lists{renderer.passes, gui_.passes()};
      const auto graph = render_system_.buildRenderGraph(pass_lists);
      if (!graph) { return std::unexpected(graph.error()); }
      render_graph_ = *graph;
      return {};
   }

   /// @brief Builds a v4 engine from typed options in any order.
   template <typename... TOptions> [[nodiscard]] auto makeEngine(TOptions &&...options) {
      return Engine{std::forward<TOptions>(options)...};
   }

   /// @brief Converts a window id into a compact filesystem-safe graph dump stem.
   inline std::string Engine::graphFileStem(std::string_view text) {
      std::string result{};
      result.reserve(text.size());
      for (const char ch : text) {
         const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
         result.push_back(safe ? ch : '_');
      }
      return result.empty() ? "window" : result;
   }

   /// @brief Writes task and render graph JSON dumps into a debug directory.
   inline std::expected<void, Error> Engine::writeDebugGraphs(const std::filesystem::path &directory) const {
      const auto task_path = directory / "task_graph.json";
      const auto render_path = directory / "render_graph.json";
      if (const auto result = tasks_.writeJson(task_path, "task_graph", "v4 task graph"); !result) { return result; }
      if (const auto result = render_graph_.writeJson(render_path, "render_graph", "v4 render graph"); !result) {
         return result;
      }

      for (const auto &window : window_system_.snapshot()) {
         auto renderer_id = window.renderer_id.value.empty() ? RendererId{.value = "forward"} : window.renderer_id;
         const auto renderer = render_system_.createRenderer(renderer_id);
         if (!renderer) { return std::unexpected(renderer.error()); }

         const std::array pass_lists{renderer->passes, gui_.passes()};
         const auto graph = render_system_.buildRenderGraph(pass_lists);
         if (!graph) { return std::unexpected(graph.error()); }

         const auto file = directory / ("render_graph_" + graphFileStem(window.id) + ".json");
         const auto name = "v4 render graph window=" + window.id + " renderer=" + renderer->id.value;
         if (const auto result = graph->writeJson(file, "render_graph", name); !result) { return result; }
      }

      return {};
   }

} // namespace vve::v4
