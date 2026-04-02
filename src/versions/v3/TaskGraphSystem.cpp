module VEEngine.V3;
import std;
import :Internal;

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
        IWindowSystem& window_system,
        IGraphicsBackend& graphics_backend,
        IResourceSystem& resource_system,
        ISceneSystem& scene_system,
        IRenderSystem& render_system,
        std::span<const WindowRenderPipeline> render_pipelines) override {
        TaskGraphBuilder builder{};

        graphics_backend.registerTasks(builder);
        window_system.registerTasks(builder);

        const auto begin_frame = TaskGraphBuilder::taskHandleFor("task.begin_frame");
        const auto poll_window_events = TaskGraphBuilder::taskHandleFor("task.poll_window_events");
        const auto end_frame = TaskGraphBuilder::taskHandleFor("task.end_frame");
        for (auto* const task_system : task_systems) {
            if (task_system != nullptr) {
                task_system->registerTasks(builder, scene);
            }
        }

        for (const auto& root : builder.rootTasks()) {
            if (root.value == begin_frame.value ||
                root.value == poll_window_events.value ||
                root.value == end_frame.value) {
                continue;
            }

            builder.addDependency(root, poll_window_events);
        }

        std::vector<TaskNodeHandle> user_leaf_tasks{};
        for (const auto& leaf : builder.leafTasks()) {
            if (leaf.value == begin_frame.value || leaf.value == end_frame.value) {
                continue;
            }

            user_leaf_tasks.push_back(leaf);
        }

        scene_system.registerTasks(builder, scene);
        resource_system.registerTasks(builder, scene);
        render_system.registerTasks(builder, scene, render_pipelines);

        const auto update_transforms = builder.findTask("task.update_transforms");
        if (update_transforms.has_value()) {
            for (const auto& leaf : user_leaf_tasks) {
                if (leaf.value == update_transforms->value) {
                    continue;
                }

                builder.addDependency(*update_transforms, leaf);
            }
        }

        if (const auto end_frame_task = builder.findTask("task.end_frame")) {
            for (const auto& pipeline : render_pipelines) {
                builder.addDependency(
                    *end_frame_task,
                    TaskGraphBuilder::taskHandleFor(
                        std::format("task.window.{}.consume_frame_output", pipeline.window_id)));
            }
        }

        return std::move(builder).build();
    }
};

} // namespace

std::unique_ptr<ITaskGraphSystem> detail::createTaskGraphSystem() {
    return std::make_unique<TaskGraphSystem>();
}

} // namespace vve::v3
