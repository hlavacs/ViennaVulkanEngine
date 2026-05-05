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
      [[nodiscard]] THandle root() const { return impl_.root; }
      void setRoot(THandle root) { impl_.root = root; }

      void addChild(THandle parent, THandle child) { impl_.addChild(parent, child); }

      void removeNode(THandle handle) { impl_.removeNode(handle); }

      [[nodiscard]] auto childRange(THandle parent) const { return impl_.childRange(parent); }

      [[nodiscard]] std::optional<THandle> parentOf(THandle child) const { return impl_.parentOf(child); }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicTree<THandle>;

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
         auto ordered = impl_.topologicalOrder(std::move(nodes).implementation());
         if (!ordered) { return std::unexpected(ordered.error()); }
         return Vector<THandle>{std::move(*ordered)};
      }

   private:
      VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Graph<THandle> impl_{};
   }; ///< Facade directed graph topology.

} // namespace vve
