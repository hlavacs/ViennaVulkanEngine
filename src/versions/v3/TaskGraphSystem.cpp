module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

   class DefaultTaskGraphSystemImplementation {
   public:
      [[nodiscard]] std::string_view name() const noexcept { return "TaskGraphSystem"; }

      [[nodiscard]] TaskGraph build(const SceneData &scene, SegmentedConstRange<ITaskSystem *> task_systems,
                                    WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                    ResourceSystem &resource_system, SceneSystem &scene_system,
                                    RenderSystem &render_system,
                                    SegmentedConstRange<WindowRenderPipeline> render_pipelines) {
         TaskGraphBuilder builder{};

         graphics_backend.registerTasks(builder);
         window_system.registerTasks(builder);

         const auto begin_frame = TaskGraphBuilder::taskHandleFor("task.begin_frame");
         const auto poll_window_events = TaskGraphBuilder::taskHandleFor("task.poll_window_events");
         const auto end_frame = TaskGraphBuilder::taskHandleFor("task.end_frame");
         for (auto *const task_system : task_systems) {
            if (task_system != nullptr) {
               task_system->registerTasks(builder, scene);
            }
         }

         for (const auto &root : builder.rootTasks()) {
            if (root.value == begin_frame.value || root.value == poll_window_events.value ||
                root.value == end_frame.value) {
               continue;
            }

            builder.addDependency(root, poll_window_events);
         }

         std::vector<TaskNodeHandle> user_leaf_tasks{};
         for (const auto &leaf : builder.leafTasks()) {
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
            for (const auto &leaf : user_leaf_tasks) {
               if (leaf.value == update_transforms->value) {
                  continue;
               }

               builder.addDependency(*update_transforms, leaf);
            }
         }

         if (const auto end_frame_task = builder.findTask("task.end_frame")) {
            for (const auto &pipeline : render_pipelines) {
               builder.addDependency(*end_frame_task, TaskGraphBuilder::taskHandleFor(std::format(
                                                          "task.window.{}.consume_frame_output", pipeline.window_id)));
            }
         }

         return std::move(builder).build();
      }
   };

   template <>
   TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>::TaskGraphSystemFacade()
       : implementation_(new DefaultTaskGraphSystemImplementation(),
                         [](DefaultTaskGraphSystemImplementation *implementation) { delete implementation; }) {}

   std::string_view TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   template <>
   TaskGraph TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>::build(
       const SceneData &scene, SegmentedConstRange<ITaskSystem *> task_systems, WindowSystem &window_system,
       GraphicsBackend &graphics_backend, ResourceSystem &resource_system, SceneSystem &scene_system,
       RenderSystem &render_system, SegmentedConstRange<WindowRenderPipeline> render_pipelines) {
      return implementation_->build(scene, task_systems, window_system, graphics_backend, resource_system, scene_system,
                                    render_system, render_pipelines);
   }

   template class TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>;

} // namespace vve::v3
