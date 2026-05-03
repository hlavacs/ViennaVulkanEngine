export module VEEngine.V4:Graphs;
import std;
export import :Types;

/// @file
/// @brief Handle-addressed task and render graph stubs.

export namespace vve::v4 {

   /// @brief A scheduled unit of CPU work.
   struct TaskNode {
      Handle handle{};    ///< Stable task handle.
      ObjectName name{};  ///< Human-readable task name.
   };

   /// @brief A future render graph pass descriptor.
   struct RenderPassNode {
      Handle handle{};    ///< Stable render-pass handle.
      ObjectName name{};  ///< Human-readable pass name.
   };

   /// @brief Minimal task graph table.
   class TaskGraph {
   public:
      /// @brief Adds a task node.
      [[nodiscard]] std::expected<void, Error> add(TaskNode node) { return tasks_.add(std::move(node)); }

      /// @brief Adds one directed task edge.
      void addEdge(Handle from, Handle to) { graph_.addEdge(from, to); }

      /// @brief Finds a task by handle, or returns null.
      [[nodiscard]] const TaskNode *find(Handle handle) const { return tasks_.find(handle); }

      /// @brief Returns tasks in dependency order and preserves isolated tasks.
      [[nodiscard]] std::expected<Vector<Handle>, Error> topologicalOrder() const {
         Vector<Handle> nodes{};
         nodes.reserve(tasks_.size());
         for (const auto &[handle, _] : tasks_.all()) { nodes.push_back(handle); }
         return graph_.topologicalOrder(nodes);
      }

      /// @brief Returns task graph topology.
      [[nodiscard]] const Graph &graph() const { return graph_; }

      /// @brief Returns task count.
      [[nodiscard]] std::size_t size() const { return tasks_.size(); }

   private:
      DescriptorMap<TaskNode> tasks_{}; ///< Tasks by handle.
      Graph graph_{};                   ///< Task dependency edges.
   };

   /// @brief Minimal render graph table.
   class RenderGraph {
   public:
      /// @brief Adds a render pass node.
      [[nodiscard]] std::expected<void, Error> add(RenderPassNode pass) { return passes_.add(std::move(pass)); }

      /// @brief Adds one directed render-pass edge.
      void addEdge(Handle from, Handle to) { graph_.addEdge(from, to); }

      /// @brief Finds a render pass by handle, or returns null.
      [[nodiscard]] const RenderPassNode *find(Handle handle) const { return passes_.find(handle); }

      /// @brief Returns render passes in dependency order and preserves isolated passes.
      [[nodiscard]] std::expected<Vector<Handle>, Error> topologicalOrder() const {
         Vector<Handle> nodes{};
         nodes.reserve(passes_.size());
         for (const auto &[handle, _] : passes_.all()) { nodes.push_back(handle); }
         return graph_.topologicalOrder(nodes);
      }

      /// @brief Returns render graph topology.
      [[nodiscard]] const Graph &graph() const { return graph_; }

      /// @brief Returns render pass count.
      [[nodiscard]] std::size_t size() const { return passes_.size(); }

   private:
      DescriptorMap<RenderPassNode> passes_{}; ///< Render passes by handle.
      Graph graph_{};                          ///< Render-pass ordering edges.
   };

} // namespace vve::v4
