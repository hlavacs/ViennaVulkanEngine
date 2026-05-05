export module VEEngine.V4:Renderer;
import std;
export import :Shaders;
export import VEEngine.V4.Graph;
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

      /// @brief Returns render graph topology.
      [[nodiscard]] const Graph<RenderPassHandle> &graph() const { return graph_; }

      /// @brief Returns render pass count.
      [[nodiscard]] std::size_t passCount() const { return passes_.size(); }

   private:
      detail::GraphNodeTable<RenderPassRecord> passes_{}; ///< Render passes by handle.
      Graph<RenderPassHandle> graph_{};                   ///< Render-pass ordering edges.
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
