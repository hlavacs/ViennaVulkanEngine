module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief Debug graph-dump helpers for task and render graphs.
 *
 * These utilities export DOT descriptions that make graph structure easier to
 * inspect during development and debugging.
 */
namespace vve::v3::detail {

#ifndef NDEBUG

   /**
    * @brief Returns a stable label for a task phase.
    * @param phase Task phase to name.
    * @return Lowercase phase label used in DOT output.
    */
   [[nodiscard]] static std::string_view taskPhaseName(TaskPhase phase) noexcept {
      switch (phase) {
      case TaskPhase::automatic:
         return "automatic";
      case TaskPhase::begin_frame:
         return "begin_frame";
      case TaskPhase::input:
         return "input";
      case TaskPhase::user_update:
         return "user_update";
      case TaskPhase::scene:
         return "scene";
      case TaskPhase::resources:
         return "resources";
      case TaskPhase::render:
         return "render";
      case TaskPhase::end_frame:
         return "end_frame";
      case TaskPhase::post_frame:
         return "post_frame";
      }

      return "unknown";
   }

   /**
    * @brief Returns the DOT node color assigned to a task phase.
    * @param phase Task phase to colorize.
    * @return Hex color string used in DOT output.
    */
   [[nodiscard]] static std::string_view taskPhaseColor(TaskPhase phase) noexcept {
      switch (phase) {
      case TaskPhase::automatic:
         return "#e6e6e6";
      case TaskPhase::begin_frame:
         return "#d9edf7";
      case TaskPhase::input:
         return "#dff0d8";
      case TaskPhase::user_update:
         return "#fcf8e3";
      case TaskPhase::scene:
         return "#f5e3ff";
      case TaskPhase::resources:
         return "#fce5cd";
      case TaskPhase::render:
         return "#ead1dc";
      case TaskPhase::end_frame:
         return "#d0e0e3";
      case TaskPhase::post_frame:
         return "#eeeeee";
      }

      return "#eeeeee";
   }

   /**
    * @brief Returns a stable label for a built-in task kernel.
    * @param kernel Task kernel to name.
    * @return Lowercase kernel label used in diagnostics.
    */
   [[nodiscard]] static std::string_view taskKernelName(TaskKernelId kernel) noexcept {
      switch (kernel) {
      case TaskKernelId::none:
         return "none";
      case TaskKernelId::begin_frame:
         return "begin_frame";
      case TaskKernelId::poll_window_events:
         return "poll_window_events";
      case TaskKernelId::update_transforms:
         return "update_transforms";
      case TaskKernelId::sample_animations:
         return "sample_animations";
      case TaskKernelId::cull_visibility_cpu:
         return "cull_visibility_cpu";
      case TaskKernelId::cull_visibility_gpu:
         return "cull_visibility_gpu";
      case TaskKernelId::build_draw_packets:
         return "build_draw_packets";
      case TaskKernelId::upload_resources:
         return "upload_resources";
      case TaskKernelId::record_render_graph:
         return "record_render_graph";
      case TaskKernelId::consume_frame_output:
         return "consume_frame_output";
      case TaskKernelId::end_frame:
         return "end_frame";
      }

      return "unknown";
   }

   /**
    * @brief Returns a stable label for a built-in render kernel.
    * @param kernel Render kernel to name.
    * @return Lowercase kernel label used in diagnostics.
    */
   [[nodiscard]] static std::string_view renderKernelName(RenderKernelId kernel) noexcept {
      switch (kernel) {
      case RenderKernelId::none:
         return "none";
      case RenderKernelId::depth_prepass:
         return "depth_prepass";
      case RenderKernelId::forward_opaque:
         return "forward_opaque";
      case RenderKernelId::deferred_gbuffer:
         return "deferred_gbuffer";
      case RenderKernelId::deferred_lighting:
         return "deferred_lighting";
      case RenderKernelId::path_trace:
         return "path_trace";
      case RenderKernelId::shadow_map:
         return "shadow_map";
      case RenderKernelId::ray_traced_shadows:
         return "ray_traced_shadows";
      case RenderKernelId::post_process:
         return "post_process";
      case RenderKernelId::post_post_process:
         return "post_post_process";
      case RenderKernelId::imgui:
         return "imgui";
      }

      return "unknown";
   }

   /**
    * @brief Escapes text so it can be embedded safely inside a DOT label.
    * @param text Raw label text.
    * @return Escaped DOT-compatible label string.
    */
   [[nodiscard]] static std::string escapeDotLabel(std::string_view text) {
      std::string result{};
      result.reserve(text.size());
      for (const auto ch : text) {
         switch (ch) {
         case '\\':
            result += "\\\\";
            break;
         case '"':
            result += "\\\"";
            break;
         case '\n':
            result += "\\n";
            break;
         default:
            result.push_back(ch);
            break;
         }
      }
      return result;
   }

   /**
    * @brief Collects render-pass indices that have no explicit predecessors.
    * @param graph Render graph being inspected.
    * @param roots Output vector receiving root pass indices.
    */
   static void appendRenderRoots(const RenderGraph &graph, std::vector<std::size_t> &roots) {
      roots.clear();
      roots.reserve(graph.passes.size());
      for (std::size_t index = 0; index < graph.passes.size(); ++index) {
         if (graph.passes[index].depends_on.empty()) {
            roots.push_back(index);
         }
      }
   }

   /**
    * @brief Collects render-pass indices that are not used as prerequisites by later passes.
    * @param graph Render graph being inspected.
    * @param leaves Output vector receiving leaf pass indices.
    */
   static void appendRenderLeaves(const RenderGraph &graph, std::vector<std::size_t> &leaves) {
      std::unordered_set<vve::Handle::value_type> dependency_handles{};
      dependency_handles.reserve(graph.passes.size());
      for (const auto &pass : graph.passes) {
         for (const auto &dependency : pass.depends_on) {
            dependency_handles.insert(dependency.value.value());
         }
      }

      leaves.clear();
      leaves.reserve(graph.passes.size());
      for (std::size_t index = 0; index < graph.passes.size(); ++index) {
         if (!dependency_handles.contains(graph.passes[index].handle.value.value())) {
            leaves.push_back(index);
         }
      }
   }

   /**
    * @brief Writes a combined task-graph and render-graph DOT dump.
    * @param task_graph Declarative frame task graph.
    * @param render_pipelines Per-window render pipelines attached to the frame.
    * @param output_path Destination `.dot` file path.
    * @return Empty success result, or an I/O error when the dump cannot be written.
    */
   std::expected<void, vve::Error>
   exportCombinedGraphDot(const TaskGraph &task_graph, VectorConstRange<WindowRenderPipeline> render_pipelines,
                          const std::filesystem::path &output_path) {
      std::error_code create_error{};
      // Debug dumps create their parent directory on demand so the task can be
      // triggered from a clean workspace.
      std::filesystem::create_directories(output_path.parent_path(), create_error);
      if (create_error) {
         return std::unexpected(vve::Error::internal_error);
      }

      std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
      if (!output.is_open()) {
         return std::unexpected(vve::Error::internal_error);
      }

      output << "digraph FrameGraph {\n";
      output << "  rankdir=TB;\n";
      output << "  compound=true;\n";
      output << "  newrank=true;\n";
      output << "  splines=ortho;\n";
      output << "  graph [fontname=\"Consolas\", pad=\"0.3\", nodesep=\"0.45\", ranksep=\"0.9 equally\"];\n";
      output << "  node [fontname=\"Consolas\", margin=\"0.18,0.10\"];\n";
      output << "  edge [fontname=\"Consolas\"];\n";
      output << "  subgraph cluster_tasks {\n";
      output << "    label=\"Task Graph\";\n";
      output << "    color=\"#8aa1b1\";\n";
      output << "    style=rounded;\n";

      constexpr std::array task_phases{TaskPhase::begin_frame, TaskPhase::input, TaskPhase::user_update,
                                       TaskPhase::scene, TaskPhase::resources, TaskPhase::render,
                                       TaskPhase::end_frame, TaskPhase::post_frame};

      for (std::size_t phase_index = 0; phase_index < task_phases.size(); ++phase_index) {
         const auto phase = task_phases[phase_index];
         bool has_phase_nodes = false;
         for (const auto &node : task_graph.nodes) {
            if (node.phase == phase) {
               has_phase_nodes = true;
               break;
            }
         }
         if (!has_phase_nodes) {
            continue;
         }

         output << "    subgraph cluster_task_phase_" << phase_index << " {\n";
         output << "      label=\"" << taskPhaseName(phase) << "\";\n";
         output << "      color=\"" << taskPhaseColor(phase) << "\";\n";
         output << "      style=filled;\n";
         output << "      fillcolor=\"" << taskPhaseColor(phase) << "22\";\n";
         output << "      rank=same;\n";

         for (const auto &node : task_graph.nodes) {
            if (node.phase != phase) {
               continue;
            }
            // Each task node includes both the human-readable label and the
            // built-in kernel id so graph dumps stay informative during debugging.
            output << "      \"task::" << node.handle.value.value() << "\""
                   << " [shape=box, style=\"filled,rounded\", fillcolor=\"" << taskPhaseColor(phase)
                   << "\", label=\""
                   << escapeDotLabel(std::format("{}\\nkernel={}", node.debug_name, taskKernelName(node.kernel)))
                   << "\"];\n";
         }
         output << "    }\n";
      }

      for (const auto &node : task_graph.nodes) {
         for (const auto &dependency : node.depends_on) {
            output << "    \"task::" << dependency.value.value() << "\" -> "
                   << "\"task::" << node.handle.value.value() << "\""
                   << " [color=\"#58708a\"];\n";
         }
      }
      output << "  }\n";

      std::vector<std::size_t> render_roots{};
      std::vector<std::size_t> render_leaves{};
      for (const auto &pipeline : render_pipelines) {
         // Each window receives its own subgraph so per-window render graphs
         // remain visually separated in the combined dump.
         output << "  subgraph cluster_render_" << pipeline.window.value.value() << " {\n";
         output << "    label=\"Render Graph: " << escapeDotLabel(pipeline.window_id) << "\";\n";
         output << "    color=\"#97c47f\";\n";
         output << "    style=rounded;\n";

         for (std::size_t index = 0; index < pipeline.graph.passes.size(); ++index) {
            const auto &pass = pipeline.graph.passes[index];
            output << "    \"render::" << escapeDotLabel(pipeline.window_id) << "::" << pass.handle.value.value()
                   << "\" [shape=ellipse, style=filled, fillcolor=\"#e7f4df\", label=\""
                   << escapeDotLabel(std::format("{}\\nkernel={}", pass.debug_name, renderKernelName(pass.kernel)))
                   << "\"];\n";
         }

         for (const auto &pass : pipeline.graph.passes) {
            for (const auto &dependency : pass.depends_on) {
               output << "    \"render::" << escapeDotLabel(pipeline.window_id) << "::" << dependency.value.value()
                      << "\" -> "
                      << "\"render::" << escapeDotLabel(pipeline.window_id) << "::" << pass.handle.value.value()
                      << "\" [color=\"#5c8a57\"];\n";
            }
         }
         output << "  }\n";

         appendRenderRoots(pipeline.graph, render_roots);
         const auto record_task = TaskGraphBuilder::taskHandleFor(
             std::format("task.window.{}.record_render_graph", pipeline.window_id));
         for (const auto root_index : render_roots) {
            // Dashed edges connect the task DAG to the per-window render graph
            // without implying that render passes are task nodes themselves.
            output << "  \"task::" << record_task.value.value() << "\" -> "
                   << "\"render::" << escapeDotLabel(pipeline.window_id) << "::"
                   << pipeline.graph.passes[root_index].handle.value.value() << "\" "
                   << "[style=dashed, color=\"#6f8f6b\", minlen=2];\n";
         }

         appendRenderLeaves(pipeline.graph, render_leaves);
         const auto consume_task = TaskGraphBuilder::taskHandleFor(
             std::format("task.window.{}.consume_frame_output", pipeline.window_id));
         for (const auto leaf_index : render_leaves) {
            output << "  \"render::" << escapeDotLabel(pipeline.window_id) << "::"
                   << pipeline.graph.passes[leaf_index].handle.value.value() << "\" -> "
                   << "\"task::" << consume_task.value.value() << "\" "
                   << "[style=dashed, color=\"#6f8f6b\", minlen=2];\n";
         }
      }

      output << "}\n";
      return {};
   }

#endif

} // namespace vve::v3::detail
