export module VEEngine:Graph;
import std;
import VEEngine.V4;
import :Error;
import :Vector;
import :Types;

/**
 * @file
 * @brief Public graph contract backed by the selected engine implementation.
 */
export namespace vve {

   template <typename THandle> class BasicTree {
   public:
      THandle root{}; ///< Public root handle required by the tree contract.

      void addChild(THandle parent, THandle child) {
         syncToImplementation();
         impl_.addChild(parent, child);
         syncFromImplementation();
      }

      void removeNode(THandle handle) {
         syncToImplementation();
         impl_.removeNode(handle);
         syncFromImplementation();
      }

      [[nodiscard]] auto childRange(THandle parent) const {
         syncToImplementation();
         return impl_.childRange(parent);
      }

      [[nodiscard]] std::optional<THandle> parentOf(THandle child) const {
         syncToImplementation();
         return impl_.parentOf(child);
      }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicTree<THandle>;

      void syncToImplementation() const { impl_.root = root; }
      void syncFromImplementation() { root = impl_.root; }

      mutable Impl impl_{};
   }; ///< Facade tree topology.

   template <typename THandle> class Graph {
   public:
      void addEdge(THandle from, THandle to) { impl_.addEdge(from, to); }
      void removeEdge(THandle from, THandle to) { impl_.removeEdge(from, to); }
      void removeNode(THandle handle) { impl_.removeNode(handle); }

      [[nodiscard]] auto childRange(THandle from) const { return impl_.childRange(from); }
      [[nodiscard]] auto parentRange(THandle to) const { return impl_.parentRange(to); }
      [[nodiscard]] std::expected<Vector<THandle>, Error> topologicalOrder(Vector<THandle> nodes) const {
         return impl_.topologicalOrder(std::move(nodes));
      }

   private:
      VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Graph<THandle> impl_{};
   }; ///< Facade directed graph topology.

   template <typename TTree, typename THandle> concept BasicTreeLike =
      requires(TTree tree, THandle parent, THandle child) {
         { tree.root } -> std::same_as<THandle &>;
         tree.addChild(parent, child);
         tree.removeNode(child);
         { tree.childRange(parent) };
         { tree.parentOf(child) } -> std::same_as<std::optional<THandle>>;
      }; ///< Contract for public tree topology templates.

   template <typename TGraph, typename THandle> concept GraphLike =
      requires(TGraph graph, THandle from, THandle to, Vector<THandle> nodes) {
         graph.addEdge(from, to);
         graph.removeEdge(from, to);
         graph.removeNode(from);
         { graph.childRange(from) };
         { graph.parentRange(to) };
         { graph.topologicalOrder(nodes) } -> std::same_as<std::expected<Vector<THandle>, Error>>;
      }; ///< Contract for public directed graph topology templates.

   static_assert(BasicTreeLike<BasicTree<NodeHandle>, NodeHandle>);
   static_assert(GraphLike<Graph<NodeHandle>, NodeHandle>);

} // namespace vve
