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

   template <typename THandle>
   using BasicTree = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicTree<THandle>; ///< Facade tree topology.
   template <typename THandle>
   using Graph = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Graph<THandle>; ///< Facade directed graph topology.

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
