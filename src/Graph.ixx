export module VEEngine:Graph;
import std;
export import :Error;

/**
 * @file
 * @brief Shared typed-handle graph and tree topology templates.
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

   /// @brief Generic directed graph topology addressed by typed handles.
   template <typename THandle> struct Graph {
      std::multimap<THandle, THandle> edges{}; ///< Source node handle mapped to destination node handles.

      /// @brief Adds one directed edge.
      void addEdge(THandle from, THandle to) { edges.emplace(from, to); }

      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(THandle node) const { return edges.equal_range(node); }

      /// @brief Returns nodes in dependency order, or cycle_detected when the graph is cyclic.
      [[nodiscard]] std::expected<std::vector<THandle>, Error>
      topologicalOrder(const std::vector<THandle> &nodes) const {
         std::map<THandle, std::uint32_t> incoming{};
         std::map<THandle, std::vector<THandle>> outgoing{};
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
         std::vector<THandle> ordered{};
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
} // namespace vve
