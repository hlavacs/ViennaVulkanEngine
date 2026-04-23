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

      template <typename TItem, typename THandleAccessor>
      [[nodiscard]] std::expected<void, vve::Error>
      rebuildHandleIndex(const Vector<TItem> &items, std::unordered_map<vve::Handle::value_type, std::size_t> &indices,
                         THandleAccessor handle_accessor) {
         indices.clear();
         indices.reserve(items.size());

         for (std::size_t item_index = 0; item_index < items.size(); ++item_index) {
            const auto handle_value = handle_accessor(items[item_index]).value();
            const auto [_, inserted] = indices.emplace(handle_value, item_index);
            if (!inserted) {
               return std::unexpected(vve::Error::invalid_argument);
            }
         }

         return {};
      }

      /**
       * @brief Returns the runtime node index for a scene-node handle.
       * @param scene Runtime scene storage to search.
       * @param handle Stable scene-node handle to resolve.
       * @return Matching node index, or an error when the handle is unknown.
       */
      [[nodiscard]] std::expected<std::size_t, vve::Error> findNodeIndex(const SceneData &scene, SceneNodeHandle handle) {
         const auto node_index = scene.node_indices.find(handle.value.value());
         if (node_index == scene.node_indices.end()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return node_index->second;
      }

      /**
       * @brief Rebuilds the scene-node handle lookup cache from dense runtime storage.
       * @param scene Mutable runtime scene storage.
       * @return Empty success result, or an error when duplicate node handles are present.
       */
      [[nodiscard]] std::expected<void, vve::Error> rebuildNodeIndex(SceneData &scene) {
         return rebuildHandleIndex(scene.nodes, scene.node_indices,
                                   [](const SceneNodeDesc &node) { return node.handle.value; });
      }

      /**
       * @brief Validates that all runtime mesh-instance references point to known assets.
       * @param scene Runtime scene storage to validate.
       * @return Empty success result, or an error when an instance references an unknown mesh or material.
       */
      [[nodiscard]] std::expected<void, vve::Error> validateMeshInstances(const SceneData &scene) {
         for (const auto &mesh_instance : scene.mesh_instances) {
            if (!scene.node_indices.contains(mesh_instance.node.value.value()) ||
                !scene.mesh_indices.contains(mesh_instance.mesh.value.value())) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            if (mesh_instance.material_override.has_value() &&
                !scene.material_indices.contains(mesh_instance.material_override->value.value())) {
               return std::unexpected(vve::Error::invalid_argument);
            }
         }

         return {};
      }

      /**
       * @brief Builds first-child and next-sibling links from the parent hierarchy.
       * @param scene Mutable runtime scene storage.
       * @return Empty success result, or an error when the hierarchy references an unknown parent.
       */
      [[nodiscard]] std::expected<void, vve::Error> buildHierarchyLinks(SceneData &scene) {
         for (auto &node : scene.nodes) {
            node.first_child = {};
            node.next_sibling = {};
         }

         for (auto &node : scene.nodes) {
            if (!node.parent.value.isValid()) {
               continue;
            }

            const auto parent_index = findNodeIndex(scene, node.parent);
            if (!parent_index) {
               return std::unexpected(parent_index.error());
            }

            auto &parent = scene.nodes[*parent_index];
            node.next_sibling = parent.first_child; // New children are prepended to keep the representation compact.
            parent.first_child = node.handle;
         }

         return {};
      }

      /**
       * @brief Collects runtime root-node indices in dense storage order.
       * @param scene Runtime scene storage to inspect.
       * @return Root-node indices used to seed the top-down transform traversal.
       */
      [[nodiscard]] std::vector<std::size_t> collectRootNodeIndices(const SceneData &scene) {
         std::vector<std::size_t> root_indices{};
         root_indices.reserve(scene.nodes.size());

         for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
            if (!scene.nodes[node_index].parent.value.isValid()) {
               root_indices.push_back(node_index);
            }
         }

         return root_indices;
      }

      /**
       * @brief Resolves one parent's children into a forward traversal sequence.
       * @param scene Runtime scene storage to inspect.
       * @param parent Runtime node whose children are appended.
       * @param child_indices Output vector receiving child indices in traversal order.
       * @return Empty success result, or an error when a child handle cannot be resolved.
       */
      [[nodiscard]] std::expected<void, vve::Error>
      appendChildTraversalOrder(const SceneData &scene, const SceneNodeDesc &parent, std::vector<std::size_t> &child_indices) {
         const auto child_begin = child_indices.size();
         auto child_handle = parent.first_child;
         while (child_handle.value.isValid()) {
            const auto child_index = findNodeIndex(scene, child_handle);
            if (!child_index) {
               return std::unexpected(child_index.error());
            }

            child_indices.push_back(*child_index);
            child_handle = scene.nodes[*child_index].next_sibling;
         }

         std::reverse(child_indices.begin() + static_cast<std::ptrdiff_t>(child_begin), child_indices.end());
         return {};
      }

      /**
       * @brief Updates one node subtree recursively for the current frame.
       * @param scene Mutable runtime scene storage.
       * @param node_index Index of the runtime node to update.
       * @param parent_world World transform of the parent node, or `nullptr` for a root node.
       * @param current_frame Current frame index written into the updated subtree.
       * @return Empty success result, or an error when the hierarchy references an unknown parent.
       */
      [[nodiscard]] std::expected<void, vve::Error>
      updateNodeRecursive(SceneData &scene, std::size_t node_index, const vve::math::Mat4 *parent_world, std::uint64_t current_frame) {
         auto &node = scene.nodes[node_index];
         if (node.last_updated_frame == current_frame) {
            return {};
         }

         node.world_transform = parent_world == nullptr ? node.local_transform
                                                        : vve::math::multiply(*parent_world, node.local_transform);
         node.last_updated_frame = current_frame;

         // Visit children after their parent so transform propagation stays root-first and depth-first.
         std::vector<std::size_t> child_indices{};
         if (auto child_result = appendChildTraversalOrder(scene, node, child_indices); !child_result) {
            return child_result;
         }

         for (const auto child_index : child_indices) {
            if (auto update_result = updateNodeRecursive(scene, child_index, &node.world_transform, current_frame); !update_result) {
               return update_result;
            }
         }

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
         instance.handle = scene.handle;
         instance.name = scene.name;
         instance.source_path = scene.source_path;
         instance.textures = scene.textures;
         instance.meshes = scene.meshes;
         instance.materials = scene.materials;

         if (auto texture_index_result =
                 rebuildHandleIndex(instance.textures, instance.texture_indices,
                                    [](const ImportedTexture &texture) { return texture.handle.value; });
             !texture_index_result) {
            return std::unexpected(texture_index_result.error());
         }

         if (auto mesh_index_result = rebuildHandleIndex(instance.meshes, instance.mesh_indices,
                                                         [](const ImportedMesh &mesh) { return mesh.handle.value; });
             !mesh_index_result) {
            return std::unexpected(mesh_index_result.error());
         }

         if (auto material_index_result = rebuildHandleIndex(
                 instance.materials, instance.material_indices,
                 [](const ImportedMaterial &material) { return material.handle.value; });
             !material_index_result) {
            return std::unexpected(material_index_result.error());
         }

         // Runtime scene instantiation copies the imported node data verbatim
         // into mutable runtime scene storage.
         instance.nodes.reserve(scene.nodes.size());
         for (const auto &node : scene.nodes) {
            instance.nodes.push_back(SceneNodeDesc{.handle = node.handle,
                                                   .parent = node.parent,
                                                   .first_child = {},
                                                   .next_sibling = {},
                                                   .name = node.name,
                                                   .local_transform = node.local_transform,
                                                   .world_transform = node.local_transform,
                                                   .last_updated_frame = std::numeric_limits<std::uint64_t>::max()});
         }

         for (const auto &node : scene.nodes) {
            for (const auto &mesh_instance : node.mesh_instances) {
               instance.mesh_instances.push_back(SceneMeshInstanceDesc{.handle = mesh_instance.handle,
                                                                      .node = node.handle,
                                                                      .mesh = mesh_instance.mesh,
                                                                      .material_override =
                                                                          mesh_instance.material_override});
            }
         }

         if (auto index_result = rebuildNodeIndex(instance); !index_result) {
            return std::unexpected(index_result.error());
         }

         if (auto mesh_instance_index_result = rebuildHandleIndex(
                 instance.mesh_instances, instance.mesh_instance_indices,
                 [](const SceneMeshInstanceDesc &mesh_instance) { return mesh_instance.handle; });
             !mesh_instance_index_result) {
            return std::unexpected(mesh_instance_index_result.error());
         }

         if (auto validate_result = validateMeshInstances(instance); !validate_result) {
            return std::unexpected(validate_result.error());
         }

         if (auto hierarchy_result = buildHierarchyLinks(instance); !hierarchy_result) {
            return std::unexpected(hierarchy_result.error());
         }

         return instance;
      }

      /// @brief Updates scene transforms for the current frame.
      [[nodiscard]] std::expected<void, vve::Error> updateTransforms(const FrameContext &frame_context, SceneData &scene) {
         if (auto index_result = rebuildNodeIndex(scene); !index_result) {
            return std::unexpected(index_result.error());
         }

         if (auto hierarchy_result = buildHierarchyLinks(scene); !hierarchy_result) {
            return std::unexpected(hierarchy_result.error());
         }

         // Seed the traversal from root nodes only so every update flows top-down through the hierarchy.
         const auto root_indices = collectRootNodeIndices(scene);
         for (const auto node_index : root_indices) {
            if (auto update_result = updateNodeRecursive(scene, node_index, nullptr, frame_context.frame_index); !update_result) {
               return update_result;
            }
         }

         // Nodes not reached from a root indicate a malformed hierarchy such as a cycle.
         for (const auto &node : scene.nodes) {
            if (node.last_updated_frame != frame_context.frame_index) {
               return std::unexpected(vve::Error::invalid_argument);
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
