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

export module VEEngine.V3:Internal;
import :Types;
import :Systems;
import VEEngine;
import std;

/**
 * @file
 * @brief Internal v3 runtime types and helper declarations.
 *
 * This module centralizes compiled task-graph state and runtime assembly data
 * shared across v3 implementation units.
 */
export namespace vve::v3::detail {

   template <typename TImplementation, typename... TArgs>
   [[nodiscard]] std::unique_ptr<TImplementation, void (*)(TImplementation *)> makeImplementation(TArgs &&...args) {
      return {new TImplementation(std::forward<TArgs>(args)...),
              [](TImplementation *implementation) { delete implementation; }};
   }

   template <typename TFunction> [[nodiscard]] TaskCallback requireFrame(TFunction function) {
      return [function = std::move(function)](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
         if (execution_context.frame_context == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return std::invoke(function, *execution_context.frame_context);
      };
   }

   template <typename TFunction> [[nodiscard]] TaskCallback requireFrameScene(TFunction function) {
      return [function = std::move(function)](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
         if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return std::invoke(function, *execution_context.frame_context, *execution_context.scene);
      };
   }

   template <typename TFunction> [[nodiscard]] TaskCallback requireWindowFrame(TFunction function) {
      return [function = std::move(function)](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
         if (execution_context.window_frame == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return std::invoke(function, *execution_context.window_frame);
      };
   }

   template <typename TFunction> [[nodiscard]] TaskCallback requireWorld(TFunction function) {
      return [function = std::move(function)](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
         if (execution_context.world == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return std::invoke(function, *execution_context.world);
      };
   }

   template <typename TFunction> [[nodiscard]] TaskCallback requireFrameWindowFrameWorld(TFunction function) {
      return [function = std::move(function)](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
         if (execution_context.frame_context == nullptr || execution_context.window_frame == nullptr ||
             execution_context.world == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return std::invoke(function, *execution_context.frame_context, *execution_context.window_frame,
                            *execution_context.world);
      };
   }

   /// @brief Distinguishes explicit graph edges from synthesized hazard edges.
   enum class CompiledTaskDependencyKind : std::uint32_t {
      explicit_order = 0, ///< Dependency came from an explicit task-graph edge.
      resource_hazard     ///< Dependency was added to enforce a resource hazard ordering.
   };

   /// @brief One compiled dependency edge in the executable task graph.
   struct CompiledTaskDependencyDesc {
      std::size_t node_index{0};                                                   ///< Index of the dependency node in the compiled node array.
      CompiledTaskDependencyKind kind{CompiledTaskDependencyKind::explicit_order}; ///< Origin of the dependency edge.
   };

   /// @brief Executable task-node metadata derived from a declarative task node.
   struct CompiledTaskNodeDesc {
      std::size_t index{0};                              ///< Index of this node in the compiled node array.
      std::uint32_t initial_dependency_count{0};         ///< Number of dependencies before execution begins.
      Vector<CompiledTaskDependencyDesc> dependencies{}; ///< Incoming dependency edges.
      Vector<CompiledTaskDependencyDesc> dependents{};   ///< Outgoing dependency edges.
      Vector<ResourceAccess> accesses{};                 ///< Cached resource access declarations.
   };

   /**
    * @brief Reusable compiled execution plan derived from a declarative task graph.
    *
    * The compiled form intentionally stores richer dependency metadata than
    * the current executor strictly needs so future hazard-aware and parallel
    * scheduling can reuse the same structure.
    */
   struct CompiledTaskGraph {
      bool valid{true};                             ///< Whether compilation succeeded.
      vve::Error error{vve::Error::internal_error}; ///< Error code when `valid` is false.
      Vector<CompiledTaskNodeDesc> nodes{};         ///< Compiled nodes in execution-plan order.
      Vector<std::size_t> initial_ready_nodes{};    ///< Nodes ready to run at the beginning of execution.
      Vector<std::size_t> topological_order{};      ///< Topological order used by the current executor.
   };

   /// @brief Fully assembled v3 runtime object.
   struct Runtime final {
      AssetSystem asset_system{};                          ///< Asset-import subsystem facade.
      ResourceSystem resource_system{};                    ///< Resource-management subsystem facade.
      SceneSystem scene_system{};                          ///< Scene-management subsystem facade.
      std::unique_ptr<SceneLoader> scene_loader{};         ///< Scene-loading orchestration facade.
      TaskGraphSystem task_graph_system{};                 ///< Task-graph assembly subsystem facade.
      Vector<std::shared_ptr<ITaskSystem>> task_systems{}; ///< User-supplied task systems extending the runtime.
      WindowSystem window_system{};                        ///< Window/platform subsystem facade.
      /// @brief Shared frame-local window snapshot.
      std::shared_ptr<WindowFrameData> window_frame{std::make_shared<WindowFrameData>()};
      GraphicsBackend graphics_backend{};                 ///< Graphics backend facade.
      ShaderSystem shader_system{};                       ///< Shader reflection subsystem facade.
      std::unique_ptr<RenderSystem> render_system{};      ///< Render subsystem facade stored via indirection.
      Vector<WindowRenderPipeline> render_pipelines{};    ///< Per-window render pipelines.
      std::unique_ptr<GuiSystem> gui_system{};            ///< Optional GUI subsystem facade.
      EngineRuntimeSnapshot snapshot{};                   ///< Human-readable runtime snapshot for diagnostics.
   };

   /// @brief Synchronizes runtime window state into the world-facing window cache.
   VVE_API void syncWorldWindows(const WindowFrameData &window_frame, std::vector<vve::WindowInfo> &windows);
   /// @brief Synchronizes runtime input events into the world-facing input snapshot.
   VVE_API void syncWorldInput(const WindowFrameData &window_frame, vve::InputState &input);
   /// @brief Ensures the task graph contains the task that syncs runtime state into the world facade.
   [[nodiscard]] VVE_API TaskNodeHandle ensureWorldSyncTask(std::vector<vve::WindowInfo> &world_windows,
                                                            vve::InputState &input_state,
                                                            vve::detail::WorldRuntimeAccess &world_runtime_access,
                                                            TaskGraphBuilder &builder);
   /// @brief Builds a deterministic handle from a stable textual seed plus optional salt.
   [[nodiscard]] vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt = 0);
   /// @brief Compiles a declarative task graph into executable dependency metadata.
   [[nodiscard]] VVE_API CompiledTaskGraph compileTaskGraph(const TaskGraph &task_graph);
   /// @brief Executes a compiled task graph using the supplied runtime execution context.
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   executeCompiledTaskGraph(const TaskGraph &task_graph, const CompiledTaskGraph &compiled_task_graph,
                            const TaskExecutionContext &execution_context);
   /// @brief Executes a cached compiled task graph when one is available.
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   executeCachedTaskGraph(const TaskGraph &task_graph, const std::optional<CompiledTaskGraph> &compiled_task_graph,
                          const TaskExecutionContext &execution_context);
   /// @brief Assembles the concrete v3 runtime from the runtime descriptor.
   [[nodiscard]] VVE_API std::expected<Runtime, vve::Error> createRuntime(const EngineRuntimeDesc &desc);
   /// @brief Creates backend-owned Vulkan resources for each assembled window render pipeline.
   [[nodiscard]] VVE_API std::expected<void, vve::Error> createRuntimePipelineResources(Runtime &runtime);
#ifndef NDEBUG
   /// @brief Registers the debug-only task that exports a combined task/render graph dump.
   VVE_API void registerDebugGraphDumpTask(std::function<const TaskGraph *()> task_graph_accessor,
                                           VectorConstRange<WindowRenderPipeline> render_pipelines,
                                           std::vector<vve::WindowInfo> &world_windows, vve::InputState &input_state,
                                           vve::detail::WorldRuntimeAccess &world_runtime_access,
                                           TaskGraphBuilder &builder);
   /// @brief Exports a combined task/render graph DOT file for debugging.
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   exportCombinedGraphDot(const TaskGraph &task_graph, VectorConstRange<WindowRenderPipeline> render_pipelines,
                          const std::filesystem::path &output_path);
#endif

} // namespace vve::v3::detail
