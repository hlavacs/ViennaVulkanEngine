module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

   class DefaultSceneSystemImplementation {
   public:
      [[nodiscard]] std::string_view name() const noexcept {
         return "SceneSystem";
      }

      [[nodiscard]] std::expected<SceneData, vve::Error>
      instantiate(const ImportedScene &scene) {
         SceneData instance{};
         instance.handle = scene.handle;
         instance.nodes.reserve(scene.nodes.size());
         for (const auto &node : scene.nodes) {
            instance.nodes.push_back(
                SceneNodeDesc{.handle = node.handle,
                              .parent = node.parent,
                              .name = node.name,
                              .local_transform = node.local_transform});
         }
         return instance;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      updateTransforms(const FrameContext &, SceneData &scene) {
         for (auto &node : scene.nodes) {
            node.local_transform = node.local_transform;
         }

         return {};
      }

      [[nodiscard]] std::expected<void, vve::Error>
      cullVisibility(const FrameContext &, const SceneData &) {
         return {};
      }

      void registerTasks(TaskGraphBuilder &builder, const SceneData &) {
         const auto update_transforms_task = builder.addTask(
             "task.update_transforms", TaskKernelId::update_transforms, {},
             {TaskGraphBuilder::taskHandleFor("task.begin_frame")}, {},
             "Update Transforms");
         const auto cull_visibility_cpu_task = builder.addTask(
             "task.cull_visibility_cpu", TaskKernelId::cull_visibility_cpu, {},
             {TaskGraphBuilder::taskHandleFor("task.update_transforms")}, {},
             "Cull Visibility CPU");

         builder.setTaskCallback(
             update_transforms_task,
             [this](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr ||
                    execution_context.scene == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return updateTransforms(*execution_context.frame_context,
                                        *execution_context.scene);
             });

         builder.setTaskCallback(
             cull_visibility_cpu_task,
             [this](const TaskExecutionContext &execution_context)
                 -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr ||
                    execution_context.scene == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return cullVisibility(*execution_context.frame_context,
                                      *execution_context.scene);
             });
      }
   };

   template <>
   SceneSystemFacade<DefaultSceneSystemImplementation>::SceneSystemFacade()
       : implementation_(new DefaultSceneSystemImplementation(),
                         [](DefaultSceneSystemImplementation *implementation) {
                            delete implementation;
                         }) {}

   std::string_view
   SceneSystemFacade<DefaultSceneSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   template <>
   std::expected<SceneData, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::instantiate(
       const ImportedScene &scene) {
      return implementation_->instantiate(scene);
   }

   template <>
   std::expected<void, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::updateTransforms(
       const FrameContext &frame_context, SceneData &scene) {
      return implementation_->updateTransforms(frame_context, scene);
   }

   template <>
   std::expected<void, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::cullVisibility(
       const FrameContext &frame_context, const SceneData &scene) {
      return implementation_->cullVisibility(frame_context, scene);
   }

   template <>
   void SceneSystemFacade<DefaultSceneSystemImplementation>::registerTasks(
       TaskGraphBuilder &builder, const SceneData &scene) {
      implementation_->registerTasks(builder, scene);
   }

   template class SceneSystemFacade<DefaultSceneSystemImplementation>;

} // namespace vve::v3
