module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine.V4;
import std;
export import VEEngine.V4.Math;
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

   using TaskHandle = TypedHandle<decltype([] {})>; ///< v4 CPU task graph node handle.

   /// @brief User-system task names supplied by the facade for graph dumps.
   struct UserSystemTasks {
      Vector<ObjectName> value{}; ///< Task names already formatted for the task graph.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal task graph storing task names and dependency edges.
   class TaskGraph {
   public:
      [[nodiscard]] std::expected<TaskHandle, Error> addTask(ObjectName name);
      void addEdge(TaskHandle from, TaskHandle to);
      [[nodiscard]] std::expected<void, Error> remove(TaskHandle handle);
      [[nodiscard]] bool contains(TaskHandle handle) const;
      [[nodiscard]] std::expected<ObjectName, Error> taskName(TaskHandle handle) const;
      [[nodiscard]] std::expected<TaskHandle, Error> taskHandle(std::string_view name) const;
      [[nodiscard]] std::expected<Vector<TaskHandle>, Error> topologicalOrder() const;
      [[nodiscard]] std::size_t taskCount() const;
      [[nodiscard]] std::string toJson(std::string_view name = "task_graph") const;
      [[nodiscard]] std::expected<void, Error> writeJson(const std::filesystem::path &path,
                                                         std::string_view name = "task_graph") const;

   private:
      using EdgeMap = std::unordered_multimap<TaskHandle, TaskHandle, HandleHash<TaskHandle>>;

      void removeOutgoingEdge(TaskHandle from, TaskHandle to);
      void removeIncomingEdge(TaskHandle to, TaskHandle from);

      std::map<TaskHandle, ObjectName> tasks_{}; ///< Task names by handle.
      EdgeMap outgoing_{};                       ///< Forward dependency edges.
      EdgeMap incoming_{};                       ///< Reverse dependency edges.
   };

   /// @brief Adds a task and returns its handle.
   inline std::expected<TaskHandle, Error> TaskGraph::addTask(ObjectName name) {
      const auto handle = makeCounterHandle<TaskHandle>();
      const auto [_, inserted] = tasks_.emplace(handle, std::move(name));
      if (!inserted) { return std::unexpected(Error::duplicate_object); }
      return handle;
   }

   /// @brief Adds one directed task edge.
   inline void TaskGraph::addEdge(TaskHandle from, TaskHandle to) {
      outgoing_.emplace(from, to);
      incoming_.emplace(to, from);
   }

   /// @brief Removes one task node and all graph edges touching it.
   inline std::expected<void, Error> TaskGraph::remove(TaskHandle handle) {
      if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
      if (tasks_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }

      const auto [first_child, last_child] = outgoing_.equal_range(handle);
      for (auto it = first_child; it != last_child; ++it) { removeIncomingEdge(it->second, handle); }
      outgoing_.erase(handle);

      const auto [first_parent, last_parent] = incoming_.equal_range(handle);
      for (auto it = first_parent; it != last_parent; ++it) { removeOutgoingEdge(it->second, handle); }
      incoming_.erase(handle);
      return {};
   }

   /// @brief Returns whether a task exists.
   inline bool TaskGraph::contains(TaskHandle handle) const { return tasks_.contains(handle); }

   /// @brief Returns the task name.
   inline std::expected<ObjectName, Error> TaskGraph::taskName(TaskHandle handle) const {
      const auto task = tasks_.find(handle);
      if (task == tasks_.end()) { return std::unexpected(Error::missing_object); }
      return task->second;
   }

   /// @brief Returns the task handle for a name.
   inline std::expected<TaskHandle, Error> TaskGraph::taskHandle(std::string_view name) const {
      for (const auto &[handle, task_name] : tasks_) {
         if (task_name.value == name) { return handle; }
      }
      return std::unexpected(Error::missing_object);
   }

   /// @brief Returns tasks in dependency order and preserves isolated tasks.
   inline std::expected<Vector<TaskHandle>, Error> TaskGraph::topologicalOrder() const {
      std::map<TaskHandle, std::uint32_t> incoming_counts{};
      std::map<TaskHandle, Vector<TaskHandle>> ordered_children{};
      for (const auto &[handle, _] : tasks_) { incoming_counts.try_emplace(handle, 0); }

      for (const auto &[from, to] : outgoing_) {
         if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
         if (!incoming_counts.contains(from) || !incoming_counts.contains(to)) {
            return std::unexpected(Error::missing_object);
         }
         ordered_children[from].push_back(to);
         ++incoming_counts[to];
      }

      std::set<TaskHandle> ready{};
      Vector<TaskHandle> ordered{};
      ordered.reserve(incoming_counts.size());
      for (const auto &[node, count] : incoming_counts) {
         if (count == 0) { ready.insert(node); }
      }

      while (!ready.empty()) {
         const auto node = *ready.begin();
         ready.erase(ready.begin());
         ordered.push_back(node);
         for (const auto child : ordered_children[node]) {
            auto &count = incoming_counts[child];
            if (--count == 0) { ready.insert(child); }
         }
      }

      if (ordered.size() != incoming_counts.size()) { return std::unexpected(Error::cycle_detected); }
      return ordered;
   }

   /// @brief Returns task count.
   inline std::size_t TaskGraph::taskCount() const { return tasks_.size(); }

   /// @brief Removes one forward edge.
   inline void TaskGraph::removeOutgoingEdge(TaskHandle from, TaskHandle to) {
      auto [first, last] = outgoing_.equal_range(from);
      for (auto it = first; it != last;) { it = it->second == to ? outgoing_.erase(it) : std::next(it); }
   }

   /// @brief Removes one reverse edge.
   inline void TaskGraph::removeIncomingEdge(TaskHandle to, TaskHandle from) {
      auto [first, last] = incoming_.equal_range(to);
      for (auto it = first; it != last;) { it = it->second == from ? incoming_.erase(it) : std::next(it); }
   }

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

      const auto begin = tasks_.addTask(ObjectName{.value = "task.frame_begin"});
      const auto poll = tasks_.addTask(ObjectName{.value = "task.poll_window_events"});
      const auto render = tasks_.addTask(ObjectName{.value = "task.render_graph"});
      const auto finish = tasks_.addTask(ObjectName{.value = "task.frame_finished"});
      if (!begin || !poll || !render || !finish) { return std::unexpected(Error::internal_error); }

      tasks_.addEdge(*begin, *poll);
      auto previous = *poll;
      for (const auto &task_name : user_system_tasks_) {
         const auto task = tasks_.addTask(task_name);
         if (!task) { return std::unexpected(task.error()); }
         tasks_.addEdge(previous, *task);
         previous = *task;
      }
      tasks_.addEdge(previous, *render);
      tasks_.addEdge(*render, *finish);

      const RenderSystem render_system{};
      const auto renderer = render_system.createForwardRenderer();
      const std::array pass_lists{renderer.passes, gui_.passes()};
      const auto graph = render_system.buildRenderGraph(pass_lists);
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

   /// @brief Returns a simple node-and-edge JSON dump for graph visualization tools.
   inline std::string TaskGraph::toJson(std::string_view name) const {
      std::string json{};
      json += "{\n  \"kind\": \"task_graph\",\n  \"name\": ";
      detail::appendJsonString(json, name);
      json += ",\n  \"nodes\": [";

      bool first_node{true};
      for (const auto &[handle, task_name] : tasks_) {
         json += first_node ? "\n" : ",\n";
         first_node = false;
         json += "    {\"id\": ";
         detail::appendJsonString(json, detail::jsonHandleId(handle));
         json += ", \"name\": ";
         detail::appendJsonString(json, task_name.value);
         json += "}";
      }

      json += "\n  ],\n  \"edges\": [";
      std::vector<std::pair<TaskHandle, TaskHandle>> edges{};
      edges.reserve(outgoing_.size());
      for (const auto &[from, to] : outgoing_) { edges.emplace_back(from, to); }
      std::ranges::sort(edges);

      bool first_edge{true};
      for (const auto [from, to] : edges) {
         const auto from_name = tasks_.find(from);
         const auto to_name = tasks_.find(to);
         json += first_edge ? "\n" : ",\n";
         first_edge = false;
         json += "    {\"from\": ";
         detail::appendJsonString(json, detail::jsonHandleId(from));
         json += ", \"to\": ";
         detail::appendJsonString(json, detail::jsonHandleId(to));
         json += ", \"from_name\": ";
         detail::appendJsonString(json, from_name == tasks_.end() ? "" : from_name->second.value);
         json += ", \"to_name\": ";
         detail::appendJsonString(json, to_name == tasks_.end() ? "" : to_name->second.value);
         json += "}";
      }

      json += "\n  ]\n}\n";
      return json;
   }

   /// @brief Writes the task graph JSON dump to disk.
   inline std::expected<void, Error> TaskGraph::writeJson(const std::filesystem::path &path,
                                                          std::string_view name) const {
      return detail::writeJsonFile(path, toJson(name));
   }

   /// @brief Writes task and render graph JSON dumps into a debug directory.
   inline std::expected<void, Error> Engine::writeDebugGraphs(const std::filesystem::path &directory) const {
      const auto task_path = directory / "task_graph.json";
      const auto render_path = directory / "render_graph.json";
      if (const auto result = tasks_.writeJson(task_path, "v4 task graph"); !result) { return result; }
      if (const auto result = render_graph_.writeJson(render_path, "v4 render graph"); !result) { return result; }

      const RenderSystem render_system{};
      for (const auto &window : window_system_.snapshot()) {
         auto renderer_id = window.renderer_id.value.empty() ? RendererId{.value = "forward"} : window.renderer_id;
         const auto renderer = render_system.createRenderer(renderer_id);
         if (!renderer) { return std::unexpected(renderer.error()); }

         const std::array pass_lists{renderer->passes, gui_.passes()};
         const auto graph = render_system.buildRenderGraph(pass_lists);
         if (!graph) { return std::unexpected(graph.error()); }

         const auto file = directory / ("render_graph_" + graphFileStem(window.id) + ".json");
         const auto name = "v4 render graph window=" + window.id + " renderer=" + renderer->id.value;
         if (const auto result = graph->writeJson(file, name); !result) { return result; }
      }

      return {};
   }

} // namespace vve::v4
