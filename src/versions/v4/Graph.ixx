export module VEEngine.V4.Graph;
import std;
export import VEEngine.V4.Error;
export import VEEngine.V4.Handle;
export import VEEngine.V4.Vector;

/**
 * @file
 * @brief v4 implementation of typed-handle tree and directed graph topology.
 */
export namespace vve::v4 {

   /// @brief Tree topology: one root plus parent-to-child and child-to-parent maps.
   template <typename THandle> struct BasicTree {
      THandle root{};    ///< Root node handle.
      std::unordered_multimap<THandle, THandle, HandleHash<THandle>> children{}; ///< Parent mapped to children.
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
      [[nodiscard]] std::expected<Vector<THandle>, Error>
      topologicalOrder(const Vector<THandle> &nodes) const {
         std::map<THandle, std::uint32_t> incoming_counts{};
         std::map<THandle, Vector<THandle>> ordered_children{};
         for (const auto node : nodes) {
            if (!node.valid()) { return std::unexpected(Error::invalid_handle); }
            incoming_counts.try_emplace(node, 0);
         }

         for (const auto &[from, to] : outgoing) {
            if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
            if (!incoming_counts.contains(from) || !incoming_counts.contains(to)) {
               return std::unexpected(Error::missing_object);
            }
            ordered_children[from].push_back(to);
            ++incoming_counts[to];
         }

         std::set<THandle> ready{};
         Vector<THandle> ordered{};
         ordered.reserve(incoming_counts.size());
         for (const auto &[node, count] : incoming_counts) {
            if (count == 0) { ready.insert(node); }
         }

         while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            ordered.push_back(node);
            for (const auto child : ordered_children[node]) {
               auto &count = incoming_counts[child];
               if (--count == 0) { ready.insert(child); }
            }
         }

         if (ordered.size() != incoming_counts.size()) { return std::unexpected(Error::cycle_detected); }
         return ordered;
      }

   };

   namespace detail {

      /// @brief Tiny ordered table for subsystem-owned graph descriptors.
      template <typename TNode> class GraphNodeTable {
      public:
         using HandleType = typename TNode::HandleType; ///< Strong handle accepted by this table.

         /// @brief Inserts a graph node descriptor with a valid unique handle.
         [[nodiscard]] std::expected<void, Error> add(TNode node) {
            if (!node.handle.valid()) { return std::unexpected(Error::invalid_handle); }
            const auto [_, inserted] = nodes_.emplace(node.handle, std::move(node));
            if (!inserted) { return std::unexpected(Error::duplicate_object); }
            return {};
         }

         /// @brief Removes a node descriptor by handle.
         [[nodiscard]] std::expected<void, Error> remove(HandleType handle) {
            if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
            if (nodes_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
            return {};
         }

         /// @brief Finds a graph node by handle, or returns null.
         [[nodiscard]] const TNode *find(HandleType handle) const {
            const auto it = nodes_.find(handle);
            return it == nodes_.end() ? nullptr : std::addressof(it->second);
         }

         /// @brief Returns descriptor count.
         [[nodiscard]] std::size_t size() const { return nodes_.size(); }

         /// @brief Exposes read-only descriptor storage for deterministic iteration.
         [[nodiscard]] const std::map<HandleType, TNode> &all() const { return nodes_; }

      private:
         std::map<HandleType, TNode> nodes_{}; ///< Ordered node descriptors by typed handle.
      };

   } // namespace detail

} // namespace vve::v4
