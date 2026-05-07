export module VEEngine.V4:RenderPass;
import std;
export import :Types;

/// @file
/// @brief Render-pass contracts, milestones, and the small render-pass DAG.

export namespace vve::v4 {

   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.

   /// @brief Iterable milestone container used as dependency anchors between systems.
   struct RenderMilestone {
      inline static constexpr std::string_view frame_begin{"frame_begin"};             ///< Root of each frame graph.
      inline static constexpr std::string_view depth_prepass{"depth_prepass"};         ///< Camera depth is available.
      inline static constexpr std::string_view shadow_depth{"shadow_depth"};           ///< Shadow-map input is ready.
      inline static constexpr std::string_view raytraced_shadow{"raytraced_shadow"};   ///< Ray-traced shadow input.
      inline static constexpr std::string_view gbuffer{"gbuffer"}; ///< Deferred G-buffer is available.
      inline static constexpr std::string_view deferred_lighting{"deferred_lighting"}; ///< Deferred lighting is ready.
      inline static constexpr std::string_view raytraced_scene{"raytraced_scene"};     ///< Ray-traced scene color.
      inline static constexpr std::string_view scene_color{"scene_color"};             ///< Main scene color is ready.
      inline static constexpr std::string_view gui{"gui"};                             ///< GUI overlay has been drawn.
      inline static constexpr std::string_view present{"present"};                     ///< Back buffer is presentable.

      inline static constexpr std::array values{frame_begin, depth_prepass, shadow_depth, raytraced_shadow, gbuffer,
                                                deferred_lighting, raytraced_scene, scene_color, gui, present};

      [[nodiscard]] constexpr auto begin() const noexcept { return values.begin(); } ///< First milestone iterator.
      [[nodiscard]] constexpr auto end() const noexcept { return values.end(); }     ///< Past-end milestone iterator.
      [[nodiscard]] constexpr std::size_t size() const noexcept { return values.size(); } ///< Milestone count.
      [[nodiscard]] static constexpr std::span<const std::string_view> all() noexcept { return values; } ///< All ids.
   };

   /// @brief One planned render pass and the educational data needed to verify it.
   struct RenderPassContract {
      std::string_view id{};                          ///< Pass selector id.
      std::span<const std::string_view> depends_on{}; ///< Pass ids that must complete first.
      std::string_view shader_file{};                 ///< Slang source file used by the pass.
      std::string_view vertex_entry{};                ///< Vertex shader entry point.
      std::string_view fragment_entry{};              ///< Fragment shader entry point.
      std::string_view inputs{};                      ///< Human-readable pass inputs.
      std::string_view outputs{};                     ///< Human-readable pass outputs.
      bool writes_debug_data{};                       ///< Whether this pass writes host-verifiable data.
   };

   /// @brief One flat pass list supplied by a renderer or another engine system.
   struct RenderPassList {
      std::span<const RenderPassContract> passes{}; ///< Passes supplied by one producer.
   };

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Internal render graph pass record.
   struct RenderPassRecord {
      using HandleType = RenderPassHandle; ///< Table handle type.
      RenderPassHandle handle{};           ///< Stable render-pass handle.
      ObjectName name{};                   ///< Human-readable pass name.
   };

   /// @brief Internal ordered table for render-pass records.
   class RenderPassRecordTable {
   public:
      [[nodiscard]] std::expected<void, Error> add(RenderPassRecord pass);
      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle);
      [[nodiscard]] const RenderPassRecord *find(RenderPassHandle handle) const;
      [[nodiscard]] std::size_t size() const;
      [[nodiscard]] const std::map<RenderPassHandle, RenderPassRecord> &all() const;

   private:
      std::map<RenderPassHandle, RenderPassRecord> passes_{}; ///< Render-pass records by handle.
   };

   /// @brief Internal directed topology for render-pass dependencies.
   class RenderPassTopology {
   public:
      void addEdge(RenderPassHandle from, RenderPassHandle to);
      void removeNode(RenderPassHandle node);
      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error>
      topologicalOrder(const Vector<RenderPassHandle> &nodes) const;

   private:
      void removeOutgoingEdge(RenderPassHandle from, RenderPassHandle to);
      void removeIncomingEdge(RenderPassHandle to, RenderPassHandle from);

      using EdgeMap = std::unordered_multimap<RenderPassHandle, RenderPassHandle, HandleHash<RenderPassHandle>>;

      EdgeMap outgoing_{}; ///< Forward edges.
      EdgeMap incoming_{}; ///< Reverse edges.
   };

} // namespace vve::v4

export namespace vve::v4 {

   /// @brief Minimal render graph table.
   class RenderGraph {
   public:
      [[nodiscard]] std::expected<RenderPassHandle, Error> addPass(ObjectName name);
      void addEdge(RenderPassHandle from, RenderPassHandle to);
      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle);
      [[nodiscard]] bool contains(RenderPassHandle handle) const;
      [[nodiscard]] std::expected<ObjectName, Error> passName(RenderPassHandle handle) const;
      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error> topologicalOrder() const;
      [[nodiscard]] std::size_t passCount() const;

   private:
      RenderPassRecordTable passes_{}; ///< Render passes by handle.
      RenderPassTopology graph_{};     ///< Render-pass ordering edges.
   };

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Adds one render-pass record.
   inline std::expected<void, Error> RenderPassRecordTable::add(RenderPassRecord pass) {
      if (!pass.handle.valid()) { return std::unexpected(Error::invalid_handle); }
      const auto [_, inserted] = passes_.emplace(pass.handle, std::move(pass));
      if (!inserted) { return std::unexpected(Error::duplicate_object); }
      return {};
   }

   /// @brief Removes one render-pass record.
   inline std::expected<void, Error> RenderPassRecordTable::remove(RenderPassHandle handle) {
      if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
      if (passes_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
      return {};
   }

   /// @brief Returns a render-pass record or nullptr.
   inline const RenderPassRecord *RenderPassRecordTable::find(RenderPassHandle handle) const {
      const auto pass = passes_.find(handle);
      return pass == passes_.end() ? nullptr : std::addressof(pass->second);
   }

   /// @brief Returns record count.
   inline std::size_t RenderPassRecordTable::size() const { return passes_.size(); }

   /// @brief Returns all records for deterministic graph traversal.
   inline const std::map<RenderPassHandle, RenderPassRecord> &RenderPassRecordTable::all() const { return passes_; }

   /// @brief Adds one dependency edge.
   inline void RenderPassTopology::addEdge(RenderPassHandle from, RenderPassHandle to) {
      outgoing_.emplace(from, to);
      incoming_.emplace(to, from);
   }

   /// @brief Removes a node and every edge touching it.
   inline void RenderPassTopology::removeNode(RenderPassHandle node) {
      const auto [first_child, last_child] = outgoing_.equal_range(node);
      for (auto it = first_child; it != last_child; ++it) { removeIncomingEdge(it->second, node); }
      outgoing_.erase(node);

      const auto [first_parent, last_parent] = incoming_.equal_range(node);
      for (auto it = first_parent; it != last_parent; ++it) { removeOutgoingEdge(it->second, node); }
      incoming_.erase(node);
   }

   /// @brief Returns nodes in dependency order and reports invalid, missing, or cyclic edges.
   inline std::expected<Vector<RenderPassHandle>, Error>
   RenderPassTopology::topologicalOrder(const Vector<RenderPassHandle> &nodes) const {
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

   /// @brief Removes one forward edge.
   inline void RenderPassTopology::removeOutgoingEdge(RenderPassHandle from, RenderPassHandle to) {
      auto [first, last] = outgoing_.equal_range(from);
      for (auto it = first; it != last;) { it = it->second == to ? outgoing_.erase(it) : std::next(it); }
   }

   /// @brief Removes one reverse edge.
   inline void RenderPassTopology::removeIncomingEdge(RenderPassHandle to, RenderPassHandle from) {
      auto [first, last] = incoming_.equal_range(to);
      for (auto it = first; it != last;) { it = it->second == from ? incoming_.erase(it) : std::next(it); }
   }

   /// @brief Adds a render pass and returns its handle.
   inline std::expected<RenderPassHandle, Error> RenderGraph::addPass(ObjectName name) {
      const auto handle = makeCounterHandle<RenderPassHandle>();
      auto added = passes_.add(RenderPassRecord{.handle = handle, .name = std::move(name)});
      if (!added) { return std::unexpected(added.error()); }
      return handle;
   }

   /// @brief Adds one directed render-pass edge.
   inline void RenderGraph::addEdge(RenderPassHandle from, RenderPassHandle to) { graph_.addEdge(from, to); }

   /// @brief Removes one render-pass node and all graph edges touching it.
   inline std::expected<void, Error> RenderGraph::remove(RenderPassHandle handle) {
      if (const auto removed = passes_.remove(handle); !removed) { return removed; }
      graph_.removeNode(handle);
      return {};
   }

   /// @brief Returns whether a render pass exists.
   inline bool RenderGraph::contains(RenderPassHandle handle) const { return passes_.find(handle) != nullptr; }

   /// @brief Returns the render pass name.
   inline std::expected<ObjectName, Error> RenderGraph::passName(RenderPassHandle handle) const {
      const auto *pass = passes_.find(handle);
      if (pass == nullptr) { return std::unexpected(Error::missing_object); }
      return pass->name;
   }

   /// @brief Returns render passes in dependency order and preserves isolated passes.
   inline std::expected<Vector<RenderPassHandle>, Error> RenderGraph::topologicalOrder() const {
      Vector<RenderPassHandle> nodes{};
      nodes.reserve(passes_.size());
      for (const auto &[handle, _] : passes_.all()) { nodes.push_back(handle); }
      return graph_.topologicalOrder(nodes);
   }

   /// @brief Returns render pass count.
   inline std::size_t RenderGraph::passCount() const { return passes_.size(); }

} // namespace vve::v4
