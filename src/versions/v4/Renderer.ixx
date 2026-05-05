export module VEEngine.V4:Renderer;
import std;
export import :Shaders;
export import VEEngine.V4.Graph;
export import VEEngine.V4.Handle;

/// @file
/// @brief Renderer-selection stubs; actual Vulkan renderers are intentionally absent.

export namespace vve::v4 {

   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.

   /// @brief A future render graph pass descriptor.
   struct RenderPassNode {
      using HandleType = RenderPassHandle; ///< Descriptor handle type.
      RenderPassHandle handle{};           ///< Stable render-pass handle.
      std::string name{};                  ///< Human-readable pass name.
   };

   /// @brief Minimal render graph table.
   class RenderGraph {
   public:
      /// @brief Adds a render pass node.
      [[nodiscard]] std::expected<void, Error> add(RenderPassNode pass) { return passes_.add(std::move(pass)); }

      /// @brief Adds one directed render-pass edge.
      void addEdge(RenderPassHandle from, RenderPassHandle to) { graph_.addEdge(from, to); }

      /// @brief Removes one render-pass node and all graph edges touching it.
      [[nodiscard]] std::expected<void, Error> remove(RenderPassHandle handle) {
         if (const auto removed = passes_.remove(handle); !removed) { return removed; }
         graph_.removeNode(handle);
         return {};
      }

      /// @brief Finds a render pass by handle, or returns null.
      [[nodiscard]] const RenderPassNode *find(RenderPassHandle handle) const { return passes_.find(handle); }

      /// @brief Returns render passes in dependency order and preserves isolated passes.
      [[nodiscard]] std::expected<Vector<RenderPassHandle>, Error> topologicalOrder() const {
         Vector<RenderPassHandle> nodes{};
         nodes.reserve(passes_.size());
         for (const auto &[handle, _] : passes_.all()) { nodes.push_back(handle); }
         return graph_.topologicalOrder(nodes);
      }

      /// @brief Returns render graph topology.
      [[nodiscard]] const Graph<RenderPassHandle> &graph() const { return graph_; }

      /// @brief Returns render pass count.
      [[nodiscard]] std::size_t size() const { return passes_.size(); }

   private:
      detail::GraphNodeTable<RenderPassNode> passes_{}; ///< Render passes by handle.
      Graph<RenderPassHandle> graph_{};                 ///< Render-pass ordering edges.
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
