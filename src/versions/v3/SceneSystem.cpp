module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class SceneSystem final : public ISceneSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "SceneSystem";
    }

    [[nodiscard]] std::expected<SceneData, vve::Error> instantiate(
        const ImportedScene& scene) override {
        SceneData instance{};
        instance.handle = scene.handle;
        instance.nodes.reserve(scene.nodes.size());
        for (const auto& node : scene.nodes) {
            instance.nodes.push_back(SceneNodeDesc{
                .handle = node.handle,
                .parent = node.parent,
                .name = node.name,
                .local_transform = node.local_transform
            });
        }
        return instance;
    }

    [[nodiscard]] std::expected<void, vve::Error> updateTransforms(
        const FrameContext&,
        SceneData& scene) override {
        for (auto& node : scene.nodes) {
            node.local_transform = node.local_transform;
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> cullVisibility(
        const FrameContext&,
        const SceneData&) override {
        return {};
    }

    void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData&) override {
        const auto update_transforms_task = builder.addTask(
            "task.update_transforms",
            TaskKernelId::update_transforms,
            {},
            {TaskGraphBuilder::taskHandleFor("task.begin_frame")},
            {},
            "Update Transforms");
        const auto cull_visibility_cpu_task = builder.addTask(
            "task.cull_visibility_cpu",
            TaskKernelId::cull_visibility_cpu,
            {},
            {TaskGraphBuilder::taskHandleFor("task.update_transforms")},
            {},
            "Cull Visibility CPU");

        builder.setTaskCallback(
            update_transforms_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Error::invalid_argument);
                }

                return updateTransforms(*execution_context.frame_context, *execution_context.scene);
            });

        builder.setTaskCallback(
            cull_visibility_cpu_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Error::invalid_argument);
                }

                return cullVisibility(*execution_context.frame_context, *execution_context.scene);
            });
    }
};

} // namespace

std::unique_ptr<ISceneSystem> detail::createSceneSystem() {
    return std::make_unique<SceneSystem>();
}

} // namespace vve::v3
