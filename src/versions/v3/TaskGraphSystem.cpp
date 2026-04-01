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
        IGraphicsBackend& graphics_backend,
        IResourceSystem& resource_system,
        ISceneSystem& scene_system,
        IRenderSystem& render_system,
        const RenderGraph& render_graph) override {
        TaskGraphBuilder builder{};

        const auto begin_frame = builder.addTask(
            "task.begin_frame",
            TaskKernelId::begin_frame,
            {},
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
            {},
            std::move(pre_engine_dependencies),
            {},
            "Update Transforms");

        const auto uploads = builder.addTask(
            "task.upload_resources",
            TaskKernelId::upload_resources,
            {},
            {transforms},
            {},
            "Upload Resources");

        const auto cull_visibility_cpu = builder.addTask(
            "task.cull_visibility_cpu",
            TaskKernelId::cull_visibility_cpu,
            {},
            {uploads},
            {},
            "Cull Visibility CPU");

        const auto cull_visibility_gpu = builder.addTask(
            "task.cull_visibility_gpu",
            TaskKernelId::cull_visibility_gpu,
            {},
            {cull_visibility_cpu},
            {},
            "Cull Visibility GPU");

        const auto build_draw_packets = builder.addTask(
            "task.build_draw_packets",
            TaskKernelId::build_draw_packets,
            {},
            {cull_visibility_gpu},
            {},
            "Build Draw Packets");

        const auto record_render_graph = builder.addTask(
            "task.record_render_graph",
            TaskKernelId::record_render_graph,
            {},
            {build_draw_packets},
            {},
            "Record Render Graph");

        const auto record_post_processing = builder.addTask(
            "task.record_post_processing",
            TaskKernelId::record_post_processing,
            {},
            {record_render_graph},
            {},
            "Record Post Processing");

        const auto consume_frame_output = builder.addTask(
            "task.consume_frame_output",
            TaskKernelId::consume_frame_output,
            {},
            {record_post_processing},
            {},
            "Consume Frame Output");

        const auto end_frame = builder.addTask(
            "task.end_frame",
            TaskKernelId::end_frame,
            {},
            {consume_frame_output},
            {},
            "End Frame");

        graphics_backend.bindTaskCallbacks(builder, begin_frame, end_frame);
        scene_system.bindTaskCallbacks(builder, transforms, cull_visibility_cpu);
        resource_system.bindTaskCallbacks(builder, uploads);
        render_system.bindTaskCallbacks(
            builder,
            render_graph,
            cull_visibility_gpu,
            build_draw_packets,
            record_render_graph,
            record_post_processing,
            consume_frame_output);

        return std::move(builder).build();
    }
};

} // namespace

std::unique_ptr<ITaskGraphSystem> detail::createTaskGraphSystem() {
    return std::make_unique<TaskGraphSystem>();
}

} // namespace vve::v3
