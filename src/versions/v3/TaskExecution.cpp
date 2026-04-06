module VEEngine.V3;
import :Internal;
import std;

namespace vve::v3::detail {

   namespace {

      [[nodiscard]] bool hasCompiledDependency(const CompiledTaskNodeDesc &node, std::size_t dependency_index) {
         return std::ranges::any_of(node.dependencies, [dependency_index](const CompiledTaskDependencyDesc &dependency) {
            return dependency.node_index == dependency_index;
         });
      }

      void addCompiledDependency(CompiledTaskGraph &compiled_graph, std::size_t node_index,
                                 std::size_t dependency_index, CompiledTaskDependencyKind kind) {
         auto &node = compiled_graph.nodes[node_index];
         if (hasCompiledDependency(node, dependency_index)) {
            return;
         }

         node.dependencies.push_back(CompiledTaskDependencyDesc{.node_index = dependency_index, .kind = kind});
         ++node.initial_dependency_count;
         compiled_graph.nodes[dependency_index].dependents.push_back(
             CompiledTaskDependencyDesc{.node_index = node_index, .kind = kind});
      }

      [[nodiscard]] std::uint64_t taskSchedulingKey(const TaskNodeDesc &node, std::size_t index) {
         return (static_cast<std::uint64_t>(static_cast<std::underlying_type_t<TaskPhase>>(node.phase)) << 32U) |
                static_cast<std::uint64_t>(index);
      }

      void addResourceHazardDependencies(const TaskGraph &task_graph, CompiledTaskGraph &compiled_graph) {
         std::unordered_map<vve::Handle::value_type, std::size_t> last_writer{};
         std::unordered_map<vve::Handle::value_type, std::vector<std::size_t>> last_readers{};

         std::vector<std::size_t> ordered_indices(task_graph.nodes.size());
         std::iota(ordered_indices.begin(), ordered_indices.end(), std::size_t{0});
         std::ranges::sort(ordered_indices, [&task_graph](std::size_t left, std::size_t right) {
            return taskSchedulingKey(task_graph.nodes[left], left) < taskSchedulingKey(task_graph.nodes[right], right);
         });

         for (const auto node_index : ordered_indices) {
            const auto &node = task_graph.nodes[node_index];
            for (const auto &access : node.accesses) {
               const auto resource_key = access.resource.value();
               if (access.write) {
                  if (const auto writer = last_writer.find(resource_key); writer != last_writer.end()) {
                     addCompiledDependency(compiled_graph, node_index, writer->second,
                                          CompiledTaskDependencyKind::resource_hazard);
                  }

                  if (const auto readers = last_readers.find(resource_key); readers != last_readers.end()) {
                     for (const auto reader_index : readers->second) {
                        addCompiledDependency(compiled_graph, node_index, reader_index,
                                             CompiledTaskDependencyKind::resource_hazard);
                     }
                  }

                  last_writer[resource_key] = node_index;
                  last_readers[resource_key].clear();
               } else {
                  if (const auto writer = last_writer.find(resource_key); writer != last_writer.end()) {
                     addCompiledDependency(compiled_graph, node_index, writer->second,
                                          CompiledTaskDependencyKind::resource_hazard);
                  }

                  auto &readers = last_readers[resource_key];
                  if (!std::ranges::contains(readers, node_index)) {
                     readers.push_back(node_index);
                  }
               }
            }
         }
      }

   } // namespace

   CompiledTaskGraph compileTaskGraph(const TaskGraph &task_graph) {
      CompiledTaskGraph compiled_graph{};
      compiled_graph.nodes.reserve(task_graph.nodes.size());
      compiled_graph.initial_ready_nodes.reserve(task_graph.nodes.size());
      compiled_graph.topological_order.reserve(task_graph.nodes.size());

      std::unordered_map<vve::Handle::value_type, std::size_t> node_indices{};
      node_indices.reserve(task_graph.nodes.size());

      for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
         const auto handle_value = task_graph.nodes[index].handle.value.value();
         if (!node_indices.emplace(handle_value, index).second) {
            compiled_graph.valid = false;
            compiled_graph.error = vve::Error::invalid_argument;
            return compiled_graph;
         }

         compiled_graph.nodes.push_back(CompiledTaskNodeDesc{
             .index = index,
             .initial_dependency_count = 0,
             .dependencies = {},
             .dependents = {},
             .accesses = task_graph.nodes[index].accesses});
      }

      for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
         const auto &node = task_graph.nodes[index];
         auto &compiled_node = compiled_graph.nodes[index];
         compiled_node.dependencies.reserve(node.depends_on.size());

         for (const auto &dependency : node.depends_on) {
            const auto dependency_it = node_indices.find(dependency.value.value());
            if (dependency_it == node_indices.end()) {
               compiled_graph.valid = false;
               compiled_graph.error = vve::Error::invalid_argument;
               return compiled_graph;
            }
            if (static_cast<std::underlying_type_t<TaskPhase>>(task_graph.nodes[dependency_it->second].phase) >
                static_cast<std::underlying_type_t<TaskPhase>>(node.phase)) {
               compiled_graph.valid = false;
               compiled_graph.error = vve::Error::invalid_argument;
               return compiled_graph;
            }

            addCompiledDependency(compiled_graph, index, dependency_it->second, CompiledTaskDependencyKind::explicit_order);
         }
      }

      addResourceHazardDependencies(task_graph, compiled_graph);

      std::vector<std::uint32_t> remaining_dependencies{};
      remaining_dependencies.reserve(compiled_graph.nodes.size());
      for (const auto &node : compiled_graph.nodes) {
         remaining_dependencies.push_back(node.initial_dependency_count);
      }

      std::vector<std::size_t> ready_nodes{};
      ready_nodes.reserve(compiled_graph.nodes.size());
      for (std::size_t index = 0; index < compiled_graph.nodes.size(); ++index) {
         if (remaining_dependencies[index] == 0) {
            ready_nodes.push_back(index);
            compiled_graph.initial_ready_nodes.push_back(index);
         }
      }

      while (!ready_nodes.empty()) {
         const auto node_index = ready_nodes.front();
         ready_nodes.erase(ready_nodes.begin());
         compiled_graph.topological_order.push_back(node_index);

         for (const auto &dependent : compiled_graph.nodes[node_index].dependents) {
            auto &dependency_count = remaining_dependencies[dependent.node_index];
            if (dependency_count == 0) {
               compiled_graph.valid = false;
               compiled_graph.error = vve::Error::internal_error;
               return compiled_graph;
            }

            --dependency_count;
            if (dependency_count == 0) {
               ready_nodes.push_back(dependent.node_index);
            }
         }
      }

      if (compiled_graph.topological_order.size() != task_graph.nodes.size()) {
         compiled_graph.valid = false;
         compiled_graph.error = vve::Error::invalid_argument;
      }

      return compiled_graph;
   }

   std::expected<void, vve::Error>
   executeCompiledTaskGraph(const TaskGraph &task_graph, const CompiledTaskGraph &compiled_task_graph,
                            const TaskExecutionContext &execution_context) {
      if (!compiled_task_graph.valid) {
         return std::unexpected(compiled_task_graph.error);
      }
      if (compiled_task_graph.nodes.size() != task_graph.nodes.size()) {
         return std::unexpected(vve::Error::invalid_argument);
      }
      if (compiled_task_graph.topological_order.size() != task_graph.nodes.size()) {
         return std::unexpected(vve::Error::invalid_argument);
      }

      for (const auto node_index : compiled_task_graph.topological_order) {
         if (node_index >= task_graph.nodes.size()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto &node = task_graph.nodes[node_index];
         if (node.callback) {
            if (auto callback_result = node.callback(execution_context); !callback_result) {
               return callback_result;
            }
         }
      }

      return {};
   }

   std::expected<void, vve::Error>
   executeCachedTaskGraph(const TaskGraph &task_graph, const std::optional<CompiledTaskGraph> &compiled_task_graph,
                          const TaskExecutionContext &execution_context) {
      if (!compiled_task_graph.has_value() || !compiled_task_graph->valid) {
         return std::unexpected(compiled_task_graph.has_value() ? compiled_task_graph->error
                                                                : vve::Error::invalid_argument);
      }

      return executeCompiledTaskGraph(task_graph, *compiled_task_graph, execution_context);
   }

} // namespace vve::v3::detail
