module;

#include "FacadeMacros.hpp"

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 task-graph assembly implementation.
 *
 * This file collects subsystem-owned tasks into a single frame task graph and
 * inserts the engine's implicit dependency rules between the major phases.
 */
namespace vve::v3 {

   namespace {

      /// @brief Attaches window-input polling as a prerequisite for all root tasks except the built-in frame tasks.
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

      /// @brief Collects leaf tasks that must complete before scene work begins.
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

      /// @brief Makes the scene update task depend on all earlier user leaf tasks.
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

      /// @brief Makes end-of-frame execution wait for all per-window output consumers.
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

   /**
    * @brief Concrete task-graph assembly policy for v3.
    *
    * The implementation coordinates task registration order and adds the
    * engine's implicit cross-subsystem edges.
    */
   class DefaultTaskGraphSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "TaskGraphSystem"; }

      /**
       * @brief Builds the frame task graph from subsystem task registrations.
       * @return Immutable frame task graph ready for compilation and execution.
       */
      [[nodiscard]] TaskGraph build(const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems,
                                    WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                    ResourceSystem &resource_system, SceneSystem &scene_system,
                                    RenderSystem &render_system,
                                    std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
                                    VectorConstRange<WindowRenderPipeline> render_pipelines) {
         // Start with an empty builder and let each subsystem append its own
         TaskGraphBuilder builder{}; // task set through its subsystem facade.

         graphics_backend.registerTasks(builder);
         window_system.registerTasks(builder);

         const auto begin_frame = TaskGraphBuilder::taskHandleFor("task.begin_frame");
         const auto poll_window_events = TaskGraphBuilder::taskHandleFor("task.poll_window_events");
         const auto end_frame = TaskGraphBuilder::taskHandleFor("task.end_frame");
         // User task systems extend the graph before the built-in scene and
         // render phases are added so implicit edges can bridge between them.
         for (auto *const task_system : task_systems) {
            if (task_system != nullptr) {
               task_system->registerTasks(builder, scene);
            }
         }
         if (extra_tasks) {
            extra_tasks(builder, scene);
         }

         // Apply engine policy edges after all pre-scene tasks are known.
         attachImplicitInputDependencies(builder, begin_frame, poll_window_events, end_frame);
         const auto user_leaf_tasks = collectPreSceneLeafTasks(builder, begin_frame, end_frame);

         scene_system.registerTasks(builder, scene);
         resource_system.registerTasks(builder, scene, graphics_backend);
         render_system.registerTasks(builder, scene, graphics_backend, render_pipelines);

         attachSceneStartDependencies(builder, user_leaf_tasks);
         attachEndFrameDependencies(builder, render_pipelines);

         // Finalize the declarative builder into the immutable graph consumed by
         return std::move(builder).build(); // the task-graph compiler.
      }
   };

   /// @brief Constructs the public task-graph facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(TaskGraphSystemFacade, DefaultTaskGraphSystemImplementation, (), ())

   /// @brief Returns the task-graph-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(TaskGraphSystemFacade, DefaultTaskGraphSystemImplementation, name, (), (),
                               const noexcept, std::string_view)

   /// @brief Builds the frame task graph through the public task-graph facade.
   VVE_V3_DEFINE_FACADE_METHOD(TaskGraphSystemFacade, DefaultTaskGraphSystemImplementation, build,
                               (const SceneData &scene, VectorConstRange<ITaskSystem *> task_systems,
                                WindowSystem &window_system, GraphicsBackend &graphics_backend,
                                ResourceSystem &resource_system, SceneSystem &scene_system,
                                RenderSystem &render_system,
                                std::function<void(TaskGraphBuilder &, const SceneData &)> extra_tasks,
                                VectorConstRange<WindowRenderPipeline> render_pipelines),
                               (scene, task_systems, window_system, graphics_backend, resource_system, scene_system,
                                render_system, std::move(extra_tasks), render_pipelines),
                               , TaskGraph)

   /// @brief Emits the explicit task-graph facade instantiation for v3.
   template class TaskGraphSystemFacade<DefaultTaskGraphSystemImplementation>;

} // namespace vve::v3
