module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

   namespace {

      void attachImplicitInputDependencies(TaskGraphBuilder &builder, TaskNodeHandle begin_frame,
                                           TaskNodeHandle poll_window_events, TaskNodeHandle end_frame) {
         for (const auto &root : builder.rootTasks()) {
            if (root.value == begin_frame.value || root.value == poll_window_events.value ||
                root.value == end_frame.value) {
               continue;
            }

            [[maybe_unused]] const auto added = builder.addDependency(root, poll_window_events);
         }
      }

      [[nodiscard]] std::vector<TaskNodeHandle>
      collectPreSceneLeafTasks(const TaskGraphBuilder &builder, TaskNodeHandle begin_frame, TaskNodeHandle end_frame) {
         std::vector<TaskNodeHandle> user_leaf_tasks{};
         for (const auto &leaf : builder.leafTasks()) {
            if (leaf.value == begin_frame.value || leaf.value == end_frame.value) {
               continue;
            }

            user_leaf_tasks.push_back(leaf);
         }

         return user_leaf_tasks;
      }

      void attachSceneStartDependencies(TaskGraphBuilder &builder, const std::vector<TaskNodeHandle> &pre_scene_leaf_tasks) {
         const auto update_transforms = builder.findTask("task.update_transforms");
         if (!update_transforms.has_value()) {
            return;
         }

         for (const auto &leaf : pre_scene_leaf_tasks) {
            if (leaf.value == update_transforms->value) {
               continue;
            }

            [[maybe_unused]] const auto added = builder.addDependency(*update_transforms, leaf);
         }
      }

      void attachEndFrameDependencies(TaskGraphBuilder &builder, VectorConstRange<WindowRenderPipeline> render_pipelines) {
         const auto end_frame_task = builder.findTask("task.end_frame");
         if (!end_frame_task.has_value()) {
            return;
         }

         for (const auto &pipeline : render_pipelines) {
            [[maybe_unused]] const auto added = builder.addDependency(
                *end_frame_task, TaskGraphBuilder::taskHandleFor(std::format("task.window.{}.consume_frame_output",
                                                                             pipeline.window_id)));
         }
      }

   } // namespace

   class DefaultTaskGraphSystemImplementation {
   public:
      [[nodiscard]] std::string_view name() const noexcept { return "TaskGraphSystem"; }

      [[nodiscard]] TaskGraph build(const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems,
                                    WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                    ResourceSystem &resource_system, SceneSystem &scene_system,
                                    RenderSystem &render_system,
                                    std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
                                    VectorConstRange<WindowRenderPipeline> render_pipelines) {
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
         if (extra_tasks) {
            extra_tasks(builder, scene);
         }

         attachImplicitInputDependencies(builder, begin_frame, poll_window_events, end_frame);
         const auto user_leaf_tasks = collectPreSceneLeafTasks(builder, begin_frame, end_frame);

         scene_system.registerTasks(builder, scene);
         resource_system.registerTasks(builder, scene);
         render_system.registerTasks(builder, scene, render_pipelines);

         attachSceneStartDependencies(builder, user_leaf_tasks);
         attachEndFrameDependencies(builder, render_pipelines);

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
       const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems, WindowSystem &window_system,
       GraphicsBackend &graphics_backend, ResourceSystem &resource_system, SceneSystem &scene_system,
       RenderSystem &render_system, std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
       VectorConstRange<WindowRenderPipeline> render_pipelines) {
      return implementation_->build(scene, task_systems, window_system, graphics_backend, resource_system, scene_system,
                                    render_system, std::move(extra_tasks), render_pipelines);
   }

   template class TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>;

} // namespace vve::v3
