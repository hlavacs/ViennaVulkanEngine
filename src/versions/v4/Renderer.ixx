export module VEEngine.V4:Renderer;
import std;
export import :Shaders;
export import VEEngine.V4.Handle;

/// @file
/// @brief Renderer-selection stubs; actual Vulkan renderers are intentionally absent.

export namespace vve::v4 {

   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Internal future render graph pass record.
   struct RenderPassRecord {
      using HandleType = RenderPassHandle; ///< Table handle type.
      RenderPassHandle handle{};           ///< Stable render-pass handle.
      ObjectName name{};                   ///< Human-readable pass name.
   };

   /// @brief Internal ordered table for render-pass records.
   class RenderPassRecordTable {
   public:
      [[nodiscard]] std::expected<void, Error> add(RenderPassRecord pass) {
         if (!pass.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = passes_.emplace(pass.handle, std::move(pass));
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return {};
      }

      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle) {
         if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
         if (passes_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
         return {};
      }

      [[nodiscard]] const RenderPassRecord *find(RenderPassHandle handle) const {
         const auto pass = passes_.find(handle);
         return pass == passes_.end() ? nullptr : std::addressof(pass->second);
      }

      [[nodiscard]] std::size_t size() const { return passes_.size(); }
      [[nodiscard]] const std::map<RenderPassHandle, RenderPassRecord> &all() const { return passes_; }

   private:
      std::map<RenderPassHandle, RenderPassRecord> passes_{}; ///< Render-pass records by handle.
   };

   /// @brief Internal directed topology for render-pass dependencies.
   class RenderPassTopology {
   public:
      void addEdge(RenderPassHandle from, RenderPassHandle to) {
         outgoing_.emplace(from, to);
         incoming_.emplace(to, from);
      }

      void removeNode(RenderPassHandle node) {
         const auto [first_child, last_child] = outgoing_.equal_range(node);
         for (auto it = first_child; it != last_child; ++it) { removeIncomingEdge(it->second, node); }
         outgoing_.erase(node);

         const auto [first_parent, last_parent] = incoming_.equal_range(node);
         for (auto it = first_parent; it != last_parent; ++it) { removeOutgoingEdge(it->second, node); }
         incoming_.erase(node);
      }

      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error>
      topologicalOrder(const Vector<RenderPassHandle> &nodes) const {
         std::map<RenderPassHandle, std::uint32_t> incoming_counts{};
         std::map<RenderPassHandle, Vector<RenderPassHandle>> ordered_children{};
         for (const auto node : nodes) {
            if (!node.valid()) { return std::unexpected(Error::invalid_handle); }
            incoming_counts.try_emplace(node, 0);
         }

         for (const auto &[from, to] : outgoing_) {
            if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
            if (!incoming_counts.contains(from) || !incoming_counts.contains(to)) {
               return std::unexpected(Error::missing_object);
            }
            ordered_children[from].push_back(to);
            ++incoming_counts[to];
         }

         std::set<RenderPassHandle> ready{};
         Vector<RenderPassHandle> ordered{};
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

   private:
      void removeOutgoingEdge(RenderPassHandle from, RenderPassHandle to) {
         auto [first, last] = outgoing_.equal_range(from);
         for (auto it = first; it != last;) { it = it->second == to ? outgoing_.erase(it) : std::next(it); }
      }

      void removeIncomingEdge(RenderPassHandle to, RenderPassHandle from) {
         auto [first, last] = incoming_.equal_range(to);
         for (auto it = first; it != last;) { it = it->second == from ? incoming_.erase(it) : std::next(it); }
      }

      std::unordered_multimap<RenderPassHandle, RenderPassHandle, HandleHash<RenderPassHandle>> outgoing_{}; ///< Forward edges.
      std::unordered_multimap<RenderPassHandle, RenderPassHandle, HandleHash<RenderPassHandle>> incoming_{}; ///< Reverse edges.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal render graph table.
   class RenderGraph {
   public:
      /// @brief Adds a render pass and returns its handle.
      [[nodiscard]] std::expected<RenderPassHandle, Error> addPass(ObjectName name) {
         const auto handle = makeCounterHandle<RenderPassHandle>();
         auto added = passes_.add(RenderPassRecord{.handle = handle, .name = std::move(name)});
         if (!added) { return std::unexpected(added.error()); }
         return handle;
      }

      /// @brief Adds one directed render-pass edge.
      void addEdge(RenderPassHandle from, RenderPassHandle to) { graph_.addEdge(from, to); }

      /// @brief Removes one render-pass node and all graph edges touching it.
      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle) {
         if (const auto removed = passes_.remove(handle); !removed) { return removed; }
         graph_.removeNode(handle);
         return {};
      }

      /// @brief Returns whether a render pass exists.
      [[nodiscard]] bool contains(RenderPassHandle handle) const { return passes_.find(handle) != nullptr; }

      /// @brief Returns the render pass name.
      [[nodiscard]] std::expected<ObjectName, Error> passName(RenderPassHandle handle) const {
         const auto *pass = passes_.find(handle);
         if (pass == nullptr) { return std::unexpected(Error::missing_object); }
         return pass->name;
      }

      /// @brief Returns render passes in dependency order and preserves isolated passes.
      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error> topologicalOrder() const {
         Vector<RenderPassHandle> nodes{};
         nodes.reserve(passes_.size());
         for (const auto &[handle, _] : passes_.all()) { nodes.push_back(handle); }
         return graph_.topologicalOrder(nodes);
      }

      /// @brief Returns render pass count.
      [[nodiscard]] std::size_t passCount() const { return passes_.size(); }

   private:
      RenderPassRecordTable passes_{}; ///< Render passes by handle.
      RenderPassTopology graph_{};     ///< Render-pass ordering edges.
   };

   /// @brief Renderer choice descriptor returned by the factory.
   struct RendererDescriptor {
      using HandleType = RendererHandle; ///< Descriptor handle type.
      RendererHandle handle{};      ///< Stable renderer descriptor handle.
      RendererId id{.value = "forward"}; ///< Renderer id chosen by the application.
      bool shadow_maps{true};       ///< Whether this renderer intends to use shadow maps.
   };

   /// @brief Minimal factory; later it will choose concrete renderer implementations by id.
   class RendererFactory {
   public:
      /// @brief Creates the default forward-renderer descriptor.
      [[nodiscard]] RendererDescriptor createForwardRenderer() const {
         return RendererDescriptor{.handle = makeCounterHandle<RendererHandle>(),
                                   .id = RendererId{.value = "forward"},
                                   .shadow_maps = true};
      }

   };

} // namespace vve::v4
