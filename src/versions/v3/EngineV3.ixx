export module VEEngine.V3;
import VEEngine;
export import VEEngine.V3.Types;
export import VEEngine.V3.Systems;
import std;

namespace vve::v3 {

export inline [[nodiscard]] std::expected<void, vve::Result> executeTaskGraph(
    const TaskGraph& task_graph,
    const TaskExecutionContext& execution_context) {
    std::unordered_map<vve::Handle::value_type, std::size_t> node_indices{};
    node_indices.reserve(task_graph.nodes.size());

    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        const auto handle_value = task_graph.nodes[index].handle.value.value();
        if (!node_indices.emplace(handle_value, index).second) {
            return std::unexpected(vve::Result::invalid_argument);
        }
    }

    std::vector<std::vector<std::size_t>> dependents(task_graph.nodes.size());
    std::vector<std::size_t> remaining_dependencies(task_graph.nodes.size(), 0);
    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        const auto& node = task_graph.nodes[index];
        remaining_dependencies[index] = node.depends_on.size();

        for (const auto& dependency : node.depends_on) {
            const auto dependency_it = node_indices.find(dependency.value.value());
            if (dependency_it == node_indices.end()) {
                return std::unexpected(vve::Result::invalid_argument);
            }

            dependents[dependency_it->second].push_back(index);
        }
    }

    std::vector<std::size_t> ready_nodes{};
    ready_nodes.reserve(task_graph.nodes.size());
    for (std::size_t index = 0; index < task_graph.nodes.size(); ++index) {
        if (remaining_dependencies[index] == 0) {
            ready_nodes.push_back(index);
        }
    }

    std::size_t completed_nodes = 0;
    while (!ready_nodes.empty()) {
        const auto node_index = ready_nodes.front();
        ready_nodes.erase(ready_nodes.begin());

        const auto& node = task_graph.nodes[node_index];
        if (node.callback) {
            if (auto callback_result = node.callback(execution_context); !callback_result) {
                return callback_result;
            }
        }

        ++completed_nodes;
        for (const auto dependent_index : dependents[node_index]) {
            auto& dependency_count = remaining_dependencies[dependent_index];
            if (dependency_count == 0) {
                return std::unexpected(vve::Result::internal_error);
            }

            --dependency_count;
            if (dependency_count == 0) {
                ready_nodes.push_back(dependent_index);
            }
        }
    }

    if (completed_nodes != task_graph.nodes.size()) {
        return std::unexpected(vve::Result::invalid_argument);
    }

    return {};
}

export std::unique_ptr<vve::detail::EngineImpl> makeEngine(const vve::EngineConfig& config);

} // namespace vve::v3
