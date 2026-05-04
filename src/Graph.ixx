export module VEEngine:Graph;
import std;
export import :Error;

/**
 * @file
 * @brief Shared typed-handle graph and tree topology templates.
 */
export namespace vve {

   /// @brief Hashes typed handles by their 64-bit payload for unordered topology side tables.
   template <typename THandle> struct HandleHash {
      [[nodiscard]] std::size_t operator()(THandle handle) const noexcept {
         return std::hash<decltype(THandle::value)>{}(handle.value);
      }
   };

   /// @brief Tree topology: one root plus parent-to-child handle edges.
   template <typename THandle> struct BasicTree {
      THandle root{};    ///< Root node handle.
      std::unordered_multimap<THandle, THandle, HandleHash<THandle>> children{}; ///< Parent node mapped to child nodes.
      std::unordered_map<THandle, THandle, HandleHash<THandle>> parents{}; ///< Child node mapped to parent node.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(THandle parent, THandle child) {
         if (const auto old_parent = parents.find(child); old_parent != parents.end()) {
            removeChildEdge(old_parent->second, child);
         }
         children.emplace(parent, child);
         parents[child] = parent;
      }

      /// @brief Returns all children for a parent handle.
      [[nodiscard]] auto childRange(THandle parent) const { return children.equal_range(parent); }

      /// @brief Returns the parent of one node when the node is not the root or detached.
      [[nodiscard]] std::optional<THandle> parentOf(THandle child) const {
         const auto parent = parents.find(child);
         return parent == parents.end() ? std::optional<THandle>{} : std::optional<THandle>{parent->second};
      }

      /// @brief Removes a node from the topology by deleting incoming and outgoing tree edges.
      void removeNode(THandle node) {
         const auto [first_child, last_child] = children.equal_range(node);
         for (auto it = first_child; it != last_child; ++it) { parents.erase(it->second); }
         children.erase(node);
         if (const auto parent = parents.find(node); parent != parents.end()) {
            removeChildEdge(parent->second, node);
            parents.erase(parent);
         }
         if (root == node) { root = {}; }
      }

   private:
      /// @brief Removes all matching parent-to-child edges from the forward tree map.
      void removeChildEdge(THandle parent, THandle child) {
         auto [first, last] = children.equal_range(parent);
         for (auto it = first; it != last;) {
            it = it->second == child ? children.erase(it) : std::next(it);
         }
      }

   };

   /// @brief Generic directed graph topology addressed by typed handles.
   template <typename THandle> struct Graph {
      std::unordered_multimap<THandle, THandle, HandleHash<THandle>> outgoing{}; ///< Source mapped to destinations.
      std::unordered_multimap<THandle, THandle, HandleHash<THandle>> incoming{}; ///< Destination mapped to sources.

      /// @brief Adds one directed edge.
      void addEdge(THandle from, THandle to) {
         outgoing.emplace(from, to);
         incoming.emplace(to, from);
      }

      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(THandle node) const { return outgoing.equal_range(node); }

      /// @brief Returns all incoming edges for a node handle.
      [[nodiscard]] auto parentRange(THandle node) const { return incoming.equal_range(node); }

      /// @brief Removes all matching directed edges.
      void removeEdge(THandle from, THandle to) {
         removeOutgoingEdge(from, to);
         removeIncomingEdge(to, from);
      }

      /// @brief Removes a node from the topology by deleting incoming and outgoing graph edges.
      void removeNode(THandle node) {
         const auto [first_child, last_child] = outgoing.equal_range(node);
         for (auto it = first_child; it != last_child; ++it) { removeIncomingEdge(it->second, node); }
         outgoing.erase(node);

         const auto [first_parent, last_parent] = incoming.equal_range(node);
         for (auto it = first_parent; it != last_parent; ++it) { removeOutgoingEdge(it->second, node); }
         incoming.erase(node);
      }

   private:
      /// @brief Removes all matching forward edges without touching the reverse table.
      void removeOutgoingEdge(THandle from, THandle to) {
         auto [first, last] = outgoing.equal_range(from);
         for (auto it = first; it != last;) {
            it = it->second == to ? outgoing.erase(it) : std::next(it);
         }
      }

      /// @brief Removes all matching reverse edges without touching the forward table.
      void removeIncomingEdge(THandle to, THandle from) {
         auto [first, last] = incoming.equal_range(to);
         for (auto it = first; it != last;) {
            it = it->second == from ? incoming.erase(it) : std::next(it);
         }
      }

   public:
      /// @brief Returns nodes in dependency order, or cycle_detected when the graph is cyclic.
      [[nodiscard]] std::expected<std::vector<THandle>, Error>
      topologicalOrder(const std::vector<THandle> &nodes) const {
         std::map<THandle, std::uint32_t> incoming{};
         std::map<THandle, std::vector<THandle>> ordered_children{};
         for (const auto node : nodes) {
            if (!node.valid()) { return std::unexpected(Error::invalid_handle); }
            incoming.try_emplace(node, 0);
         }

         for (const auto &[from, to] : outgoing) {
            if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
            if (!incoming.contains(from) || !incoming.contains(to)) { return std::unexpected(Error::missing_object); }
            ordered_children[from].push_back(to);
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
            for (const auto child : ordered_children[node]) {
               auto &count = incoming[child];
               if (--count == 0) { ready.insert(child); }
            }
         }

         if (ordered.size() != incoming.size()) { return std::unexpected(Error::cycle_detected); }
         return ordered;
      }

   };
} // namespace vve
