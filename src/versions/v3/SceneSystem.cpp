module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 scene-system implementation.
 *
 * The scene system owns runtime scene instantiation and the scene-phase tasks
 * that update transforms and perform coarse visibility work.
 */
namespace vve::v3 {

   /**
    * @brief Concrete scene-system implementation used by v3.
    */
   class DefaultSceneSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "SceneSystem"; }

      /// @brief Converts imported scene data into runtime scene data.
      [[nodiscard]] std::expected<SceneData, vve::Error> instantiate(const ImportedScene &scene) {
         SceneData instance{};
         // Runtime scene instantiation currently copies the imported node data
         instance.handle = scene.handle; // verbatim into the mutable runtime representation.
         instance.nodes.reserve(scene.nodes.size());
         for (const auto &node : scene.nodes) {
            instance.nodes.push_back(SceneNodeDesc{.handle = node.handle,
                                                   .parent = node.parent,
                                                   .name = node.name,
                                                   .local_transform = node.local_transform});
         }
         return instance;
      }

      /// @brief Updates scene transforms for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> updateTransforms(const FrameContext &, SceneData &scene) {
         // The current placeholder scene system leaves transforms unchanged but
         for (auto &node : scene.nodes) { // still preserves the task seam for future animation and hierarchy work.
            node.local_transform = node.local_transform;
         }

         return {};
      }

      /// @brief Performs CPU-side visibility work for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibility(const FrameContext &, const SceneData &) {
         return {};
      }

      /// @brief Registers the built-in scene-phase tasks.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &) {
         const auto update_transforms_task =
              builder.addTask("task.update_transforms", TaskKernelId::update_transforms, {},
                             {TaskGraphBuilder::taskHandleFor("task.begin_frame")}, {}, "Update Transforms",
                             TaskPhase::scene);
         const auto cull_visibility_cpu_task =
              builder.addTask("task.cull_visibility_cpu", TaskKernelId::cull_visibility_cpu, {},
                             {TaskGraphBuilder::taskHandleFor("task.update_transforms")}, {}, "Cull Visibility CPU",
                             TaskPhase::scene);

         [[maybe_unused]] const auto transforms_callback_set = builder.setTaskCallback(
             update_transforms_task,
             [this](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return updateTransforms(*execution_context.frame_context, *execution_context.scene);
             });

         [[maybe_unused]] const auto cull_callback_set = builder.setTaskCallback(
             cull_visibility_cpu_task,
             [this](const TaskExecutionContext &execution_context) -> std::expected<void, vve::Error> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                   return std::unexpected(vve::Error::invalid_argument);
                }

                return cullVisibility(*execution_context.frame_context, *execution_context.scene);
             });
      }
   };

   /// @brief Constructs the public scene-system facade around the concrete implementation.
   template <>
   SceneSystemFacade<DefaultSceneSystemImplementation>::SceneSystemFacade()
       : implementation_(new DefaultSceneSystemImplementation(),
                         [](DefaultSceneSystemImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the scene-system name for the public facade.
   std::string_view SceneSystemFacade<DefaultSceneSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Instantiates a runtime scene through the public facade.
   template <>
   std::expected<SceneData, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::instantiate(const ImportedScene &scene) {
      return implementation_->instantiate(scene);
   }

   /// @brief Updates scene transforms through the public facade.
   template <>
   std::expected<void, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::updateTransforms(const FrameContext &frame_context,
                                                                         SceneData &scene) {
      return implementation_->updateTransforms(frame_context, scene);
   }

   /// @brief Performs CPU visibility work through the public facade.
   template <>
   std::expected<void, vve::Error>
   SceneSystemFacade<DefaultSceneSystemImplementation>::cullVisibility(const FrameContext &frame_context,
                                                                       const SceneData &scene) {
      return implementation_->cullVisibility(frame_context, scene);
   }

   /// @brief Registers scene tasks through the public facade.
   template <>
   void SceneSystemFacade<DefaultSceneSystemImplementation>::registerTasks(TaskGraphBuilder &builder,
                                                                           const SceneData &scene) {
      implementation_->registerTasks(builder, scene);
   }

   /// @brief Emits the explicit scene-system facade instantiation for v3.
   template class SceneSystemFacade<DefaultSceneSystemImplementation>;

} // namespace vve::v3
