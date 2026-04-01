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
        std::span<ITaskSystem* const> task_systems,
        IRenderSystem& render_system,
        const RenderGraph& render_graph) override {
        TaskGraphBuilder builder{};

        [[maybe_unused]] const auto begin_frame = builder.addTask(
            "task.begin_frame",
            TaskKernelId::begin_frame,
            [](const TaskExecutionContext&) -> std::expected<void, vve::Result> {
                return {};
            },
            {},
            {},
            "Begin Frame");

        for (auto* const task_system : task_systems) {
            if (task_system != nullptr) {
                task_system->registerTasks(builder, scene);
            }
        }

        auto pre_engine_dependencies = builder.leafTasks();
        if (pre_engine_dependencies.empty()) {
            pre_engine_dependencies.push_back(TaskNodeHandle{
                vve::Handle::fromHash(std::string_view{"task.begin_frame"})});
        }

        const auto transforms = builder.addTask(
            "task.update_transforms",
            TaskKernelId::update_transforms,
            [](const TaskExecutionContext&) -> std::expected<void, vve::Result> {
                return {};
            },
            std::move(pre_engine_dependencies),
            {},
            "Update Transforms");

        const auto uploads = builder.addTask(
            "task.upload_resources",
            TaskKernelId::upload_resources,
            [](const TaskExecutionContext&) -> std::expected<void, vve::Result> {
                return {};
            },
            {transforms},
            {},
            "Upload Resources");

        const auto culling = builder.addTask(
            "task.cull_visibility",
            TaskKernelId::cull_visibility,
            [](const TaskExecutionContext&) -> std::expected<void, vve::Result> {
                return {};
            },
            {uploads},
            {},
            "Cull Visibility");

        const auto build_draw_packets = builder.addTask(
            "task.build_draw_packets",
            TaskKernelId::build_draw_packets,
            [](const TaskExecutionContext&) -> std::expected<void, vve::Result> {
                return {};
            },
            {culling},
            {},
            "Build Draw Packets");

        const auto record_render_graph = builder.addTask(
            "task.record_render_graph",
            TaskKernelId::record_render_graph,
            [&render_system, &render_graph](
                const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return render_system.record(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph,
                    RenderTaskPhase::main);
            },
            {build_draw_packets},
            {},
            "Record Render Graph");

        const auto record_post_processing = builder.addTask(
            "task.record_post_processing",
            TaskKernelId::record_post_processing,
            [&render_system, &render_graph](
                const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return render_system.record(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph,
                    RenderTaskPhase::post_process);
            },
            {record_render_graph},
            {},
            "Record Post Processing");

        builder.addTask(
            "task.record_post_post_processing",
            TaskKernelId::record_post_post_processing,
            [&render_system, &render_graph](
                const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return render_system.record(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph,
                    RenderTaskPhase::post_post_process);
            },
            {record_post_processing},
            {},
            "Record Post Post Processing");

        return std::move(builder).build();
    }
};

} // namespace

std::unique_ptr<ITaskGraphSystem> detail::createTaskGraphSystem() {
    return std::make_unique<TaskGraphSystem>();
}

} // namespace vve::v3
