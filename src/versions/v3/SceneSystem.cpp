module;

#include "FacadeMacros.hpp"

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

   namespace {

      /**
       * @brief Returns the runtime node index for a scene-node handle.
       * @param scene Runtime scene storage to search.
       * @param handle Stable scene-node handle to resolve.
       * @return Matching node index, or an error when the handle is unknown.
       */
      [[nodiscard]] std::expected<std::size_t, vve::Error> findNodeIndex(const SceneData &scene, SceneNodeHandle handle) {
         for (std::size_t index = 0; index < scene.nodes.size(); ++index) {
            if (scene.nodes[index].handle.value == handle.value) {
               return index;
            }
         }

         return std::unexpected(vve::Error::invalid_argument);
      }

      /**
       * @brief Updates one node world transform recursively for the current frame.
       * @param scene Mutable runtime scene storage.
       * @param node_index Index of the runtime node to update.
       * @param current_frame Current frame index used to avoid duplicate work.
       * @return Empty success result, or an error when the hierarchy references an unknown parent.
       */
      [[nodiscard]] std::expected<void, vve::Error>
      updateNodeRecursive(SceneData &scene, std::size_t node_index, std::uint64_t current_frame) {
         auto &node = scene.nodes[node_index];
         if (node.last_updated_frame == current_frame) {
            return {};
         }

         if (node.parent.value.isValid()) {
            const auto parent_index = findNodeIndex(scene, node.parent);
            if (!parent_index) {
               return std::unexpected(parent_index.error());
            }

            if (auto update_result = updateNodeRecursive(scene, *parent_index, current_frame); !update_result) {
               return update_result;
            }

            node.world_transform = vve::math::multiply(scene.nodes[*parent_index].world_transform, node.local_transform);
         } else {
            node.world_transform = node.local_transform;
         }

         node.last_updated_frame = current_frame;
         return {};
      }

   } // namespace

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
                                                   .local_transform = node.local_transform,
                                                   .world_transform = node.local_transform,
                                                   .last_updated_frame = 0});
         }
         return instance;
      }

      /// @brief Updates scene transforms for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> updateTransforms(const FrameContext &frame_context, SceneData &scene) {
         // An empty scene is a valid runtime state and therefore a no-op.
         for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
            if (auto update_result = updateNodeRecursive(scene, node_index, frame_context.frame_index); !update_result) {
               return update_result;
            }
         }

         return {};
      }

      /// @brief Performs CPU-side visibility work for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibility(const FrameContext &, const SceneData &) {
         return {};
      }

      /// @brief Registers the built-in scene-phase tasks.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &) {
         const auto update_transforms_task = builder.addTask(
             "task.update_transforms", TaskKernelId::update_transforms,
             detail::requireFrameScene([this](const FrameContext &frame_context, SceneData &scene) {
                return updateTransforms(frame_context, scene);
             }),
             {TaskGraphBuilder::taskHandleFor("task.begin_frame")}, {}, "Update Transforms", TaskPhase::scene);
         [[maybe_unused]] const auto cull_visibility_cpu_task = builder.addTask(
             "task.cull_visibility_cpu", TaskKernelId::cull_visibility_cpu,
             detail::requireFrameScene([this](const FrameContext &frame_context, SceneData &scene) {
                return cullVisibility(frame_context, scene);
             }),
             {update_transforms_task}, {}, "Cull Visibility CPU", TaskPhase::scene);
      }
   };

   /// @brief Constructs the public scene-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(SceneSystemFacade, DefaultSceneSystemImplementation, (), ())

   /// @brief Returns the scene-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneSystemFacade, DefaultSceneSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Instantiates a runtime scene through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneSystemFacade, DefaultSceneSystemImplementation, instantiate,
                               (const ImportedScene &scene), (scene), , std::expected<SceneData, vve::Error>)

   /// @brief Updates scene transforms through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneSystemFacade, DefaultSceneSystemImplementation, updateTransforms,
                               (const FrameContext &frame_context, SceneData &scene), (frame_context, scene), ,
                               std::expected<void, vve::Error>)

   /// @brief Performs CPU visibility work through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(SceneSystemFacade, DefaultSceneSystemImplementation, cullVisibility,
                               (const FrameContext &frame_context, const SceneData &scene), (frame_context, scene), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers scene tasks through the public facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(SceneSystemFacade, DefaultSceneSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder, const SceneData &scene), (builder, scene), )

   /// @brief Emits the explicit scene-system facade instantiation for v3.
   template class SceneSystemFacade<DefaultSceneSystemImplementation>;

} // namespace vve::v3
