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
import VEEngine.V3.Types;
import VEEngine.V3.Systems;
import VEEngine;
import std;

export namespace vve::v3::detail {

   enum class CompiledTaskDependencyKind : std::uint32_t {
      explicit_order = 0,
      resource_hazard
   };

   struct CompiledTaskDependencyDesc {
      std::size_t node_index{0};
      CompiledTaskDependencyKind kind{CompiledTaskDependencyKind::explicit_order};
   };

   struct CompiledTaskNodeDesc {
      std::size_t index{0};
      std::uint32_t initial_dependency_count{0};
      Vector<CompiledTaskDependencyDesc> dependencies{};
      Vector<CompiledTaskDependencyDesc> dependents{};
      Vector<ResourceAccess> accesses{};
   };

   struct CompiledTaskGraph {
      bool valid{true};
      vve::Error error{vve::Error::internal_error};
      Vector<CompiledTaskNodeDesc> nodes{};
      Vector<std::size_t> initial_ready_nodes{};
      Vector<std::size_t> topological_order{};
   };

   struct Runtime final {
      AssetSystem asset_system{};
      ResourceSystem resource_system{};
      SceneSystem scene_system{};
      TaskGraphSystem task_graph_system{};
      Vector<std::shared_ptr<ITaskSystem>> task_systems{};
      WindowSystem window_system{};
      std::shared_ptr<WindowFrameData> window_frame{std::make_shared<WindowFrameData>()};
      GraphicsBackend graphics_backend{};
      ShaderSystem shader_system{};
      std::unique_ptr<RenderSystem> render_system{};
      Vector<WindowRenderPipeline> render_pipelines{};
      std::unique_ptr<GuiSystem> gui_system{};
      EngineRuntimeSnapshot snapshot{};
   };

   [[nodiscard]] vve::Handle makeStableHandle(std::string_view name, std::uint64_t salt = 0);
   [[nodiscard]] VVE_API CompiledTaskGraph compileTaskGraph(const TaskGraph &task_graph);
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   executeCompiledTaskGraph(const TaskGraph &task_graph, const CompiledTaskGraph &compiled_task_graph,
                            const TaskExecutionContext &execution_context);
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   executeCachedTaskGraph(const TaskGraph &task_graph, const std::optional<CompiledTaskGraph> &compiled_task_graph,
                          const TaskExecutionContext &execution_context);
   [[nodiscard]] VVE_API std::expected<Runtime, vve::Error> createRuntime(const EngineRuntimeDesc &desc);
#ifndef NDEBUG
   [[nodiscard]] VVE_API std::expected<void, vve::Error>
   exportCombinedGraphDot(const TaskGraph &task_graph, VectorConstRange<WindowRenderPipeline> render_pipelines,
                          const std::filesystem::path &output_path);
#endif

} // namespace vve::v3::detail
