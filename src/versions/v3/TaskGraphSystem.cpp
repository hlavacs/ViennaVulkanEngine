module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class TaskGraphSystem final : public ITaskGraphSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "TaskGraphSystem";
    }

    [[nodiscard]] TaskGraph build(
        const SceneData& scene,
        const FrameContext&) override {
        TaskGraph graph{};
        if (scene.nodes.empty()) {
            return graph;
        }

        const auto transforms = TaskNodeHandle{detail::makeStableHandle("task.update_transforms")};
        const auto culling = TaskNodeHandle{detail::makeStableHandle("task.cull_visibility")};
        const auto draws = TaskNodeHandle{detail::makeStableHandle("task.build_draw_packets")};

        graph.nodes.push_back(TaskNodeDesc{
            .handle = transforms,
            .kernel = TaskKernelId::update_transforms
        });
        graph.nodes.push_back(TaskNodeDesc{
            .handle = culling,
            .kernel = TaskKernelId::cull_visibility,
            .depends_on = {transforms}
        });
        graph.nodes.push_back(TaskNodeDesc{
            .handle = draws,
            .kernel = TaskKernelId::build_draw_packets,
            .depends_on = {culling}
        });
        return graph;
    }
};

} // namespace

std::unique_ptr<ITaskGraphSystem> detail::createTaskGraphSystem() {
    return std::make_unique<TaskGraphSystem>();
}

} // namespace vve::v3
