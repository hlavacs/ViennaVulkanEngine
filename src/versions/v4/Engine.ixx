module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine.V4;
import std;
export import VEEngine.V4.Math;
export import :World;
export import :Window;
export import :Assets;
export import :RenderSystem;
export import :Resources;
export import :Shaders;
export import VEEngine.V4.Handle;
export import :Gui;

/// @file
/// @brief Small v4 runtime facade: world, SDL windows, input, systems, and stub subsystems.

export namespace vve::v4 {

   using TaskHandle = TypedHandle<decltype([] {})>; ///< v4 CPU task graph node handle.

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

      /// @brief Primary trait for detecting UserSystems options.
      template <typename T> struct IsUserSystemsOption : std::false_type {};

      /// @brief Specialization that detects UserSystems options.
      template <typename... TSystems> struct IsUserSystemsOption<UserSystems<TSystems...>> : std::true_type {};

      /// @brief Finds the first UserSystems option in an option pack.
      template <typename TDefault, typename... TOptions> struct FindUserSystemsOption {
         using type = TDefault; ///< Fallback when no user-system bundle is provided.
      };

      /// @brief Recursive option-pack scanner.
      template <typename TDefault, typename TFirst, typename... TRest>
      struct FindUserSystemsOption<TDefault, TFirst, TRest...> {
         using TNormalized = std::remove_cvref_t<TFirst>; ///< Option type stripped of references and cv-qualifiers.
         using type = std::conditional_t<IsUserSystemsOption<TNormalized>::value, TNormalized,
                                         typename FindUserSystemsOption<TDefault, TRest...>::type>;
      };

      /// @brief Invokes a system hook and accepts either void or expected<void, Error>.
      template <typename TCallable> [[nodiscard]] std::expected<void, Error> callSystemHook(TCallable &&callable) {
         using TResult = std::invoke_result_t<TCallable>;
         if constexpr (std::same_as<TResult, std::expected<void, Error>>) {
            return std::invoke(std::forward<TCallable>(callable));
         } else {
            std::invoke(std::forward<TCallable>(callable));
            return {};
         }
      }

      /// @brief Returns a readable user-system name for debug graph nodes.
      template <typename TSystem> [[nodiscard]] std::string systemDebugName(const TSystem &system) {
         if constexpr (requires { std::string_view{system.name()}; }) {
            return std::string{std::string_view{system.name()}};
         } else if constexpr (requires { std::string_view{TSystem::name()}; }) {
            return std::string{std::string_view{TSystem::name()}};
         } else {
            return typeid(TSystem).name();
         }
      }

      /// @brief Converts a window id into a compact filesystem-safe graph dump stem.
      [[nodiscard]] inline std::string graphFileStem(std::string_view text) {
         std::string result{};
         result.reserve(text.size());
         for (const char ch : text) {
            const bool safe = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
            result.push_back(safe ? ch : '_');
         }
         return result.empty() ? "window" : result;
      }

   } // namespace detail

   /// @brief Educational v4 engine shell with SDL windows and lightweight world views.
   template <typename... TSystems> class Engine {
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
      [[nodiscard]] World world();
      [[nodiscard]] World world() const;
      [[nodiscard]] AssetSystem &assets();
      [[nodiscard]] GuiSystem &gui();
      [[nodiscard]] ECS &ecs();
      [[nodiscard]] std::expected<void, Error> init();
      [[nodiscard]] std::expected<void, Error> run();
      [[nodiscard]] std::expected<FrameStatus, Error> step();
      [[nodiscard]] std::expected<void, Error>
      writeDebugGraphs(const std::filesystem::path &directory = "graph_dumps") const;

   private:
      void applyOption(EngineConfig config);
      void applyOption(ApplicationName option);
      void applyOption(MaxFrames option);
      void applyOption(Windows option);
      void applyOption(UserSystems<TSystems...> option);
      template <typename TOption> void applyOption(TOption &&);
      void applyDefaults();
      [[nodiscard]] std::expected<void, Error> initSystems();
      [[nodiscard]] std::expected<void, Error> updateSystems(const FrameContext &frame,
                                                            const WindowFrameData &window_frame);
      template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system);
      template <typename TSystem>
      [[nodiscard]] std::expected<void, Error> updateOne(TSystem &system, const FrameContext &frame,
                                                         const WindowFrameData &window_frame);
      [[nodiscard]] std::expected<void, Error> buildDefaultGraphs();
      template <typename TSystem>
      [[nodiscard]] std::expected<TaskHandle, Error> addSystemTask(TaskHandle previous, TSystem &system);

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
      std::optional<std::tuple<TSystems...>> systems_{}; ///< User systems supplied by the application.
      std::chrono::steady_clock::time_point last_frame_time_{}; ///< Timestamp of the previous step().
      std::uint64_t frame_{0};                 ///< Number of completed step() calls.
      bool initialized_{false};                ///< True after init() succeeds.
   };

   /// @brief Creates an engine with default options.
   template <typename... TSystems> Engine<TSystems...>::Engine() {
      applyDefaults();
   }

   /// @brief Creates an engine from the compact compatibility config.
   template <typename... TSystems> Engine<TSystems...>::Engine(EngineConfig config) {
      applyOption(std::move(config));
      applyDefaults();
   }

   /// @brief Creates an engine from typed options such as ApplicationName, Windows, and UserSystems.
   template <typename... TSystems>
   template <typename... TOptions>
      requires(sizeof...(TOptions) > 0)
   Engine<TSystems...>::Engine(TOptions &&...options) {
      (applyOption(std::forward<TOptions>(options)), ...);
      applyDefaults();
   }

   /// @brief Returns the major engine version.
   template <typename... TSystems> std::uint32_t Engine<TSystems...>::versionMajor() const { return 4; }

   /// @brief Returns the major engine version through a v3-shaped accessor.
   template <typename... TSystems>
   std::expected<int, Error> Engine<TSystems...>::getVersionMajor() const noexcept {
      return 4;
   }

   /// @brief Returns the printable engine version name.
   template <typename... TSystems> std::string_view Engine<TSystems...>::versionName() const { return "v4"; }

   /// @brief Creates an example-facing world view over engine-owned subsystems.
   template <typename... TSystems> World Engine<TSystems...>::world() {
      auto result = World{ecs_};
      result.bindSubsystems(assets_, gui_, window_system_);
      return result;
   }

   /// @brief Creates an example-facing world view from a const engine handle.
   template <typename... TSystems> World Engine<TSystems...>::world() const {
      return const_cast<Engine<TSystems...> *>(this)->world();
   }

   /// @brief Returns the asset system.
   template <typename... TSystems> AssetSystem &Engine<TSystems...>::assets() { return assets_; }

   /// @brief Returns the GUI system.
   template <typename... TSystems> GuiSystem &Engine<TSystems...>::gui() { return gui_; }

   /// @brief Returns the runtime ECS.
   template <typename... TSystems> ECS &Engine<TSystems...>::ecs() { return ecs_; }

   /// @brief Creates SDL windows and calls optional user-system init(World&) hooks.
   template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::init() {
      if (initialized_) { return {}; }
      if (const auto result = window_system_.init(windows_); !result) { return result; }
      if (const auto result = buildDefaultGraphs(); !result) { return result; }
      last_frame_time_ = std::chrono::steady_clock::now();
      initialized_ = true;
      return initSystems();
   }

   /// @brief Runs the engine until a window closes, a system fails, or the frame cap is reached.
   template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::run() {
      if (!initialized_) {
         if (const auto result = init(); !result) { return result; }
      }
      while (true) {
         const auto status = step();
         if (!status) { return std::unexpected(status.error()); }
         if (*status == FrameStatus::stopped) { return {}; }
      }
   }

   /// @brief Polls input and calls optional user-system update hooks.
   template <typename... TSystems> std::expected<FrameStatus, Error> Engine<TSystems...>::step() {
      if (!initialized_) { return std::unexpected(Error::missing_object); }
      if (const auto result = window_system_.poll(); !result) { return std::unexpected(result.error()); }
      if (window_system_.input().wasKeyPressed(detail::debugDumpGraphHotkey)) {
         if (const auto result = writeDebugGraphs(); !result) { return std::unexpected(result.error()); }
      }

      const auto now = std::chrono::steady_clock::now();
      const std::chrono::duration<double> delta = now - last_frame_time_;
      last_frame_time_ = now;

      const FrameContext frame{.frame_index = FrameCount{.value = frame_},
                               .delta_time = DeltaTime{.seconds = delta.count()}};
      const WindowFrameData window_frame{.windows = window_system_.snapshot()};
      if (const auto result = updateSystems(frame, window_frame); !result) {
         return std::unexpected(result.error());
      }

      ++frame_;
      if (window_system_.anyShouldClose() || (max_frames_.value.value > 0 && frame_ >= max_frames_.value.value)) {
         return FrameStatus::stopped;
      }
      return FrameStatus::running;
   }

   /// @brief Writes task and render graph JSON dumps into a debug directory.
   template <typename... TSystems>
   std::expected<void, Error> Engine<TSystems...>::writeDebugGraphs(const std::filesystem::path &directory) const {
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

         const auto file = directory / ("render_graph_" + detail::graphFileStem(window.id) + ".json");
         const auto name = "v4 render graph window=" + window.id + " renderer=" + renderer->id.value;
         if (const auto result = graph->writeJson(file, name); !result) { return result; }
      }

      return {};
   }

   /// @brief Applies the compact compatibility config.
   template <typename... TSystems> void Engine<TSystems...>::applyOption(EngineConfig config) {
      application_name_.value = std::move(config.application_name);
      max_frames_.value = config.max_frames;
   }

   /// @brief Applies the application-name typed option.
   template <typename... TSystems> void Engine<TSystems...>::applyOption(ApplicationName option) {
      application_name_ = std::move(option);
   }

   /// @brief Applies the frame-cap typed option.
   template <typename... TSystems> void Engine<TSystems...>::applyOption(MaxFrames option) { max_frames_ = option; }

   /// @brief Applies the startup-window typed option.
   template <typename... TSystems> void Engine<TSystems...>::applyOption(Windows option) {
      windows_ = std::move(option);
   }

   /// @brief Applies the user-system typed option.
   template <typename... TSystems> void Engine<TSystems...>::applyOption(UserSystems<TSystems...> option) {
      systems_.emplace(std::move(option.value));
   }

   /// @brief Ignores unknown option types so examples can evolve one option at a time.
   template <typename... TSystems>
   template <typename TOption>
   void Engine<TSystems...>::applyOption(TOption &&) {}

   /// @brief Fills small defaults after options have been applied.
   template <typename... TSystems> void Engine<TSystems...>::applyDefaults() {
      if (windows_.value.empty()) { windows_.value.push_back(WindowDesc{}); }
      for (auto &window : windows_.value) {
         if (window.title == WindowDesc{}.title && application_name_.value != ApplicationName{}.value) {
            window.title = application_name_.value;
         }
      }
   }

   /// @brief Builds simple inspectable default graphs for debug dumps and teaching.
   template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::buildDefaultGraphs() {
      tasks_ = {};
      render_graph_ = {};

      const auto begin = tasks_.addTask(ObjectName{.value = "task.frame_begin"});
      const auto poll = tasks_.addTask(ObjectName{.value = "task.poll_window_events"});
      const auto render = tasks_.addTask(ObjectName{.value = "task.render_graph"});
      const auto finish = tasks_.addTask(ObjectName{.value = "task.frame_finished"});
      if (!begin || !poll || !render || !finish) { return std::unexpected(Error::internal_error); }

      tasks_.addEdge(*begin, *poll);
      auto previous = *poll;
      if (systems_.has_value()) {
         std::expected<void, Error> result{};
         std::apply([&](auto &...system) {
            ((result ? [&] {
                auto task = addSystemTask(previous, system);
                if (!task) {
                   result = std::unexpected(task.error());
                } else {
                   previous = *task;
                }
             }() : void()), ...);
         }, *systems_);
         if (!result) { return result; }
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

   /// @brief Adds one user-system update node after the previous frame task.
   template <typename... TSystems>
   template <typename TSystem>
   std::expected<TaskHandle, Error> Engine<TSystems...>::addSystemTask(TaskHandle previous, TSystem &system) {
      auto name = std::string{"task.update_system."} + detail::systemDebugName(system);
      const auto task = tasks_.addTask(ObjectName{.value = std::move(name)});
      if (!task) { return std::unexpected(task.error()); }
      tasks_.addEdge(previous, *task);
      return *task;
   }

   /// @brief Calls init(World&) on each user system when that hook exists.
   template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::initSystems() {
      if (!systems_.has_value()) { return {}; }
      auto result = std::expected<void, Error>{};
      std::apply([&](auto &...system) { ((result ? result = initOne(system) : result), ...); }, *systems_);
      return result;
   }

   /// @brief Calls the best matching update hook on each user system.
   template <typename... TSystems>
   std::expected<void, Error> Engine<TSystems...>::updateSystems(const FrameContext &frame,
                                                                 const WindowFrameData &window_frame) {
      if (!systems_.has_value()) { return {}; }
      auto result = std::expected<void, Error>{};
      std::apply([&](auto &...system) {
         ((result ? result = updateOne(system, frame, window_frame) : result), ...);
      }, *systems_);
      return result;
   }

   /// @brief Calls one system's init hook if present.
   template <typename... TSystems>
   template <typename TSystem>
   std::expected<void, Error> Engine<TSystems...>::initOne(TSystem &system) {
      auto world_view = world();
      if constexpr (requires { system.init(world_view); }) {
         return detail::callSystemHook([&]() -> decltype(auto) { return system.init(world_view); });
      } else {
         return {};
      }
   }

   /// @brief Calls one system's most expressive update hook if present.
   template <typename... TSystems>
   template <typename TSystem>
   std::expected<void, Error> Engine<TSystems...>::updateOne(TSystem &system, const FrameContext &frame,
                                                             const WindowFrameData &window_frame) {
      auto world_view = world();
      if constexpr (requires { system.update(world_view, frame, window_frame); }) {
         return detail::callSystemHook([&]() -> decltype(auto) {
            return system.update(world_view, frame, window_frame);
         });
      } else if constexpr (requires { system.update(world_view, frame); }) {
         return detail::callSystemHook([&]() -> decltype(auto) { return system.update(world_view, frame); });
      } else if constexpr (requires { system.update(world_view); }) {
         return detail::callSystemHook([&]() -> decltype(auto) { return system.update(world_view); });
      } else {
         return {};
      }
   }

   namespace detail {

      /// @brief Maps a UserSystems option to the matching Engine specialization.
      template <typename TUserSystems> struct EngineTypeFromUserSystems;

      /// @brief Specialization that injects user-system types into Engine.
      template <typename... TSystems> struct EngineTypeFromUserSystems<UserSystems<TSystems...>> {
         using type = Engine<TSystems...>; ///< Engine specialization for this user-system pack.
      };

   } // namespace detail

   /// @brief Builds a v4 engine from typed options in any order.
   template <typename... TOptions> [[nodiscard]] auto makeEngine(TOptions &&...options) {
      using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
      using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;
      return TEngine(std::forward<TOptions>(options)...);
   }

} // namespace vve::v4
