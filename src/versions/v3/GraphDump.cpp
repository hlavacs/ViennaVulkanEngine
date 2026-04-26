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

   static const std::map<TaskPhase, std::string_view> task_phase_color_map{
       {TaskPhase::automatic, "#e6e6e6"},
       {TaskPhase::begin_frame, "#d9edf7"},
       {TaskPhase::input, "#dff0d8"},
       {TaskPhase::user_update, "#fcf8e3"},
       {TaskPhase::scene, "#f5e3ff"},
       {TaskPhase::resources, "#fce5cd"},
       {TaskPhase::render, "#ead1dc"},
       {TaskPhase::end_frame, "#d0e0e3"},
       {TaskPhase::post_frame, "#eeeeee"},
   };

   /**
    * @brief Returns the DOT node color assigned to a task phase.
    * @param phase Task phase to colorize.
    * @return Hex color string used in DOT output.
   */
   [[nodiscard]] static std::string_view taskPhaseColor(TaskPhase phase) noexcept {
      return vve::detail::mapValueOr(task_phase_color_map, phase, std::string_view{"#eeeeee"});
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
         output << "    label=\""
                << escapeDotLabel(std::format("Render Graph: {}\\nrenderer={}\\nshader={}\\nstages={} sets={} vk_modules={} bound={} vk_pipeline={}",
                                              pipeline.window_id, pipeline.renderer.id,
                                              pipeline.shader_program.value.value(),
                                              pipeline.pipeline_layout.shader_stages.size(),
                                              pipeline.pipeline_layout.descriptor_sets.size(),
                                              pipeline.backend_resources.shader_module_count,
                                              pipeline.renderer_binding.ready_for_pipeline_creation ? "yes" : "no",
                                              pipeline.graphics_pipeline.vulkan_pipeline_created ? "created" : "none"))
                << "\";\n";
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
