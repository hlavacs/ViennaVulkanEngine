export module VEEngine.V4:RenderPass;
import std;
export import :Types;

/// @file
/// @brief Render-pass contracts, milestones, and a small dependency graph.

export namespace vve::v4 {

   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.

   /// @brief Iterable milestone container used as dependency anchors between systems.
   struct RenderMilestone {
      inline static constexpr std::string_view frame_begin{"frame_begin"};             ///< Root of each frame graph.
      inline static constexpr std::string_view depth_prepass{"depth_prepass"};         ///< Camera depth is available.
      inline static constexpr std::string_view shadow_depth{"shadow_depth"};           ///< Shadow-map input is ready.
      inline static constexpr std::string_view raytraced_shadow{"raytraced_shadow"};   ///< Ray-traced shadow input.
      inline static constexpr std::string_view gbuffer{"gbuffer"};                     ///< Deferred G-buffer is ready.
      inline static constexpr std::string_view deferred_lighting{"deferred_lighting"}; ///< Deferred lighting is ready.
      inline static constexpr std::string_view raytraced_scene{"raytraced_scene"};     ///< Ray-traced scene color.
      inline static constexpr std::string_view scene_color{"scene_color"};             ///< Main scene color is ready.
      inline static constexpr std::string_view gui{"gui"};                             ///< GUI overlay has been drawn.
      inline static constexpr std::string_view frame_finished{"frame_finished"};       ///< Frame is ready to present.

      inline static constexpr std::array values{frame_begin, depth_prepass, shadow_depth, raytraced_shadow, gbuffer,
                                                deferred_lighting, raytraced_scene, scene_color, gui, frame_finished};

      [[nodiscard]] constexpr auto begin() const noexcept { return values.begin(); } ///< First milestone iterator.
      [[nodiscard]] constexpr auto end() const noexcept { return values.end(); }     ///< Past-end milestone iterator.
      [[nodiscard]] constexpr std::size_t size() const noexcept { return values.size(); } ///< Milestone count.
      [[nodiscard]] static constexpr std::span<const std::string_view> all() noexcept { return values; } ///< All names.
   };

   /// @brief One planned render node and the educational data needed to verify it.
   struct RenderPassContract {
      std::string_view name{};                        ///< Pass or milestone name.
      std::span<const std::string_view> depends_on{}; ///< Node names that must complete first.
      std::string_view shader_file{};                 ///< Slang source file used by the pass.
      std::string_view vertex_entry{};                ///< Vertex shader entry point.
      std::string_view fragment_entry{};              ///< Fragment shader entry point.
      std::string_view inputs{};                      ///< Human-readable pass inputs.
      std::string_view outputs{};                     ///< Human-readable pass outputs.
      bool milestone{};                               ///< True for meta nodes that anchor real passes.
      bool writes_debug_data{};                       ///< Whether this pass writes host-verifiable data.
   };

   /// @brief Minimal DAG storing pass names and dependency edges.
   class RenderGraph {
   public:
      [[nodiscard]] std::expected<RenderPassHandle, Error> addPass(ObjectName name);
      void addEdge(RenderPassHandle from, RenderPassHandle to);
      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle);
      [[nodiscard]] bool contains(RenderPassHandle handle) const;
      [[nodiscard]] std::expected<ObjectName, Error> passName(RenderPassHandle handle) const;
      [[nodiscard]] std::expected<RenderPassHandle, Error> passHandle(std::string_view name) const;
      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error> topologicalOrder() const;
      [[nodiscard]] std::size_t passCount() const;
      [[nodiscard]] std::string toJson(std::string_view name = "render_graph") const;
      [[nodiscard]] std::expected<void, Error> writeJson(const std::filesystem::path &path,
                                                         std::string_view name = "render_graph") const;

   private:
      using EdgeMap = std::unordered_multimap<RenderPassHandle, RenderPassHandle, HandleHash<RenderPassHandle>>;

      void removeOutgoingEdge(RenderPassHandle from, RenderPassHandle to);
      void removeIncomingEdge(RenderPassHandle to, RenderPassHandle from);

      std::map<RenderPassHandle, ObjectName> passes_{}; ///< Pass names by handle.
      EdgeMap outgoing_{};                              ///< Forward dependency edges.
      EdgeMap incoming_{};                              ///< Reverse dependency edges.
   };

   /// @brief Adds a render pass and returns its handle.
   inline std::expected<RenderPassHandle, Error> RenderGraph::addPass(ObjectName name) {
      const auto handle = makeCounterHandle<RenderPassHandle>();
      const auto [_, inserted] = passes_.emplace(handle, std::move(name));
      if (!inserted) { return std::unexpected(Error::duplicate_object); }
      return handle;
   }

   /// @brief Adds one directed render-pass edge.
   inline void RenderGraph::addEdge(RenderPassHandle from, RenderPassHandle to) {
      outgoing_.emplace(from, to);
      incoming_.emplace(to, from);
   }

   /// @brief Removes one render-pass node and all graph edges touching it.
   inline std::expected<void, Error> RenderGraph::remove(RenderPassHandle handle) {
      if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
      if (passes_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }

      const auto [first_child, last_child] = outgoing_.equal_range(handle);
      for (auto it = first_child; it != last_child; ++it) { removeIncomingEdge(it->second, handle); }
      outgoing_.erase(handle);

      const auto [first_parent, last_parent] = incoming_.equal_range(handle);
      for (auto it = first_parent; it != last_parent; ++it) { removeOutgoingEdge(it->second, handle); }
      incoming_.erase(handle);
      return {};
   }

   /// @brief Returns whether a render pass exists.
   inline bool RenderGraph::contains(RenderPassHandle handle) const { return passes_.contains(handle); }

   /// @brief Returns the render pass name.
   inline std::expected<ObjectName, Error> RenderGraph::passName(RenderPassHandle handle) const {
      const auto pass = passes_.find(handle);
      if (pass == passes_.end()) { return std::unexpected(Error::missing_object); }
      return pass->second;
   }

   /// @brief Returns the render pass handle for a name.
   inline std::expected<RenderPassHandle, Error> RenderGraph::passHandle(std::string_view name) const {
      for (const auto &[handle, pass_name] : passes_) {
         if (pass_name.value == name) { return handle; }
      }
      return std::unexpected(Error::missing_object);
   }

   /// @brief Returns render passes in dependency order and preserves isolated passes.
   inline std::expected<Vector<RenderPassHandle>, Error> RenderGraph::topologicalOrder() const {
      std::map<RenderPassHandle, std::uint32_t> incoming_counts{};
      std::map<RenderPassHandle, Vector<RenderPassHandle>> ordered_children{};
      for (const auto &[handle, _] : passes_) { incoming_counts.try_emplace(handle, 0); }

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

   /// @brief Returns render pass count.
   inline std::size_t RenderGraph::passCount() const { return passes_.size(); }

   /// @brief Removes one forward edge.
   inline void RenderGraph::removeOutgoingEdge(RenderPassHandle from, RenderPassHandle to) {
      auto [first, last] = outgoing_.equal_range(from);
      for (auto it = first; it != last;) { it = it->second == to ? outgoing_.erase(it) : std::next(it); }
   }

   /// @brief Removes one reverse edge.
   inline void RenderGraph::removeIncomingEdge(RenderPassHandle to, RenderPassHandle from) {
      auto [first, last] = incoming_.equal_range(to);
      for (auto it = first; it != last;) { it = it->second == from ? incoming_.erase(it) : std::next(it); }
   }

   /// @brief Returns a simple node-and-edge JSON dump for graph visualization tools.
   inline std::string RenderGraph::toJson(std::string_view name) const {
      std::string json{};
      json += "{\n  \"kind\": \"render_graph\",\n  \"name\": ";
      detail::appendJsonString(json, name);
      json += ",\n  \"nodes\": [";

      bool first_node{true};
      for (const auto &[handle, pass_name] : passes_) {
         json += first_node ? "\n" : ",\n";
         first_node = false;
         json += "    {\"id\": ";
         detail::appendJsonString(json, detail::jsonHandleId(handle));
         json += ", \"name\": ";
         detail::appendJsonString(json, pass_name.value);
         json += "}";
      }

      json += "\n  ],\n  \"edges\": [";
      std::vector<std::pair<RenderPassHandle, RenderPassHandle>> edges{};
      edges.reserve(outgoing_.size());
      for (const auto &[from, to] : outgoing_) { edges.emplace_back(from, to); }
      std::ranges::sort(edges);

      bool first_edge{true};
      for (const auto [from, to] : edges) {
         const auto from_name = passes_.find(from);
         const auto to_name = passes_.find(to);
         json += first_edge ? "\n" : ",\n";
         first_edge = false;
         json += "    {\"from\": ";
         detail::appendJsonString(json, detail::jsonHandleId(from));
         json += ", \"to\": ";
         detail::appendJsonString(json, detail::jsonHandleId(to));
         json += ", \"from_name\": ";
         detail::appendJsonString(json, from_name == passes_.end() ? "" : from_name->second.value);
         json += ", \"to_name\": ";
         detail::appendJsonString(json, to_name == passes_.end() ? "" : to_name->second.value);
         json += "}";
      }

      json += "\n  ]\n}\n";
      return json;
   }

   /// @brief Writes the render graph JSON dump to disk.
   inline std::expected<void, Error> RenderGraph::writeJson(const std::filesystem::path &path,
                                                            std::string_view name) const {
      return detail::writeJsonFile(path, toJson(name));
   }

} // namespace vve::v4
