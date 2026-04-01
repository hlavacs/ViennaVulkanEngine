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

    [[nodiscard]] std::expected<SceneData, vve::Result> instantiate(
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

    [[nodiscard]] std::expected<void, vve::Result> updateTransforms(
        const FrameContext&,
        SceneData& scene) override {
        for (auto& node : scene.nodes) {
            node.local_transform = node.local_transform;
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> cullVisibility(
        const FrameContext&,
        const SceneData&) override {
        return {};
    }

    void bindTaskCallbacks(
        TaskGraphBuilder& builder,
        TaskNodeHandle update_transforms_task,
        TaskNodeHandle cull_visibility_cpu_task) override {
        builder.setTaskCallback(
            update_transforms_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return updateTransforms(*execution_context.frame_context, *execution_context.scene);
            });

        builder.setTaskCallback(
            cull_visibility_cpu_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
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
