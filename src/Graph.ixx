export module VEEngine:Graph;
import std;
export import :Types;

/**
 * @file
 * @brief Shared typed-handle graph, tree, and scene-node topology types.
 */
export namespace vve {

   /// @brief Tree topology: one root plus parent-to-child handle edges.
   template <typename THandle> struct BasicTree {
      THandle root{};                             ///< Root node handle.
      std::multimap<THandle, THandle> children{}; ///< Parent node handle mapped to child node handles.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(THandle parent, THandle child) { children.emplace(parent, child); }

      /// @brief Returns all children for a parent handle.
      [[nodiscard]] auto childRange(THandle parent) const { return children.equal_range(parent); }

   };

   using Tree = BasicTree<NodeHandle>; ///< Scene-tree topology uses node handles.

   /// @brief Generic directed graph topology addressed by typed handles.
   template <typename THandle> struct Graph {
      std::multimap<THandle, THandle> edges{}; ///< Source node handle mapped to destination node handles.

      /// @brief Adds one directed edge.
      void addEdge(THandle from, THandle to) { edges.emplace(from, to); }

      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(THandle node) const { return edges.equal_range(node); }

      /// @brief Returns nodes in dependency order, or cycle_detected when the graph is cyclic.
      [[nodiscard]] std::expected<Vector<THandle>, Error> topologicalOrder(const Vector<THandle> &nodes) const {
         std::map<THandle, std::uint32_t> incoming{};
         std::map<THandle, Vector<THandle>> outgoing{};
         for (const auto node : nodes) {
            if (!node.valid()) { return std::unexpected(Error::invalid_handle); }
            incoming.try_emplace(node, 0);
         }

         for (const auto &[from, to] : edges) {
            if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
            if (!incoming.contains(from) || !incoming.contains(to)) { return std::unexpected(Error::missing_object); }
            outgoing[from].push_back(to);
            ++incoming[to];
         }

         std::set<THandle> ready{};
         Vector<THandle> ordered{};
         ordered.reserve(incoming.size());
         for (const auto &[node, count] : incoming) {
            if (count == 0) { ready.insert(node); }
         }

         while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            ordered.push_back(node);
            for (const auto child : outgoing[node]) {
               auto &count = incoming[child];
               if (--count == 0) { ready.insert(child); }
            }
         }

         if (ordered.size() != incoming.size()) { return std::unexpected(Error::cycle_detected); }
         return ordered;
      }

   };

   /// @brief Scene graph node descriptor stored by handle in ObjectCatalog.
   struct NodeDescriptor {
      using HandleType = NodeHandle; ///< Descriptor handle type.
      NodeHandle handle{};           ///< Stable node handle.
      ObjectName name{};             ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Scene descriptor stores only handles to objects kept in descriptor maps.
   struct SceneDescriptor {
      using HandleType = SceneHandle;     ///< Descriptor handle type.
      SceneHandle handle{};               ///< Stable scene handle.
      ObjectName name{};                  ///< Human-readable scene name.
      Tree tree{};                        ///< Scene hierarchy; nodes do not store child vectors.
      Vector<NodeHandle> nodes{};         ///< All node handles in the scene.
      Vector<MeshHandle> meshes{};        ///< Mesh handles used by the scene.
      Vector<MaterialHandle> materials{}; ///< Material handles used by the scene.
      Vector<TextureHandle> textures{};   ///< Texture handles used by the scene.
      Vector<LightHandle> lights{};       ///< Light handles used by the scene.
      Vector<CameraHandle> cameras{};     ///< Camera handles used by the scene.
   };

   /// @brief Central imported-object catalog; every loaded object is found by 64-bit handle.
   struct ObjectCatalog {
      DescriptorMap<SceneDescriptor> scenes{};       ///< Scenes by handle.
      DescriptorMap<NodeDescriptor> nodes{};         ///< Nodes by handle.
      DescriptorMap<MeshDescriptor> meshes{};        ///< Meshes by handle.
      DescriptorMap<MaterialDescriptor> materials{}; ///< Materials by handle.
      DescriptorMap<TextureDescriptor> textures{};   ///< Textures by handle.
      DescriptorMap<LightDescriptor> lights{};       ///< Lights by handle.
      DescriptorMap<CameraDescriptor> cameras{};     ///< Cameras by handle.
   };

} // namespace vve
