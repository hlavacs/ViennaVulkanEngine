export module VEEngine.V4:RenderSystem;
import std;
import :RendererForward;
export import :Types;
export import VEEngine.V4.Handle;

/// @file
/// @brief CPU render data and renderer selection system.

export namespace vve::v4 {

   using RenderPassHandle     = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.
   using RenderMeshHandle     = TypedHandle<decltype([] {})>; ///< v4 render mesh handle.
   using RenderMaterialHandle = TypedHandle<decltype([] {})>; ///< v4 render material handle.
   using RenderInstanceHandle = TypedHandle<decltype([] {})>; ///< v4 render instance handle.

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

      using EdgeMap = std::unordered_multimap<RenderPassHandle, RenderPassHandle, HandleHash<RenderPassHandle>>;

      EdgeMap outgoing_{}; ///< Forward edges.
      EdgeMap incoming_{}; ///< Reverse edges.
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

   /// @brief Vertex payload accepted by the first forward renderer milestone.
   struct RenderVertex {
      Vec3 position{zeroVec3()};                       ///< Object-space position.
      Vec3 normal{Vec3(zero(), one(), zero())};        ///< Object-space normal.
      Vec2 uv{zero(), zero()};                         ///< First texture coordinate.
   };

   /// @brief CPU-side mesh data ready to become Vulkan vertex and index buffers.
   struct RenderMesh {
      RenderMeshHandle handle{};       ///< Stable render mesh handle.
      Vector<RenderVertex> vertices{}; ///< Source vertices.
      Vector<std::uint32_t> indices{}; ///< Triangle indices.
      Bounds bounds{};                 ///< Object-space bounds.
   };

   /// @brief CPU-side material data for a simple forward pass.
   struct RenderMaterial {
      RenderMaterialHandle handle{};                     ///< Stable render material handle.
      LinearColor base_color{.value = oneVec3()};        ///< Base color factor.
      TextureHandle base_color_texture{};                ///< Optional texture handle; invalid means none.
      std::filesystem::path base_color_texture_source{}; ///< Optional source path for diagnostics.
   };

   /// @brief One draw item connecting mesh, material, and transforms.
   struct RenderInstance {
      RenderInstanceHandle handle{};          ///< Stable render instance handle.
      RenderMeshHandle mesh{};                ///< Mesh drawn by this instance.
      RenderMaterialHandle material{};        ///< Material used by this instance.
      Transform local_transform{};            ///< Source scene local transform.
      Mat4 world_transform{identityMat4()};   ///< World transform used by rendering.
   };

   /// @brief One-light model used by the first shadow-map milestone.
   struct RenderDirectionalLight {
      Direction direction_to_light{.value = Vec3(-0.5F, 1.0F, 0.25F)}; ///< Direction from surface to light.
      LinearColor color{.value = oneVec3()};                           ///< Direct light color.
      LightIntensity intensity{.value = one()};                        ///< Direct light intensity.
      LinearColor ambient{.value = Vec3(0.04F, 0.04F, 0.04F)};          ///< Small ambient term.
      Mat4 light_view_projection{identityMat4()};                      ///< Future shadow-map light matrix.
   };

   /// @brief Camera data consumed by render passes.
   struct RenderCamera {
      Camera camera{};                                      ///< Facade camera description.
      PixelExtent target_extent{.width = 1, .height = 1};   ///< Render target size.
   };

   /// @brief Minimal CPU render scene built before Vulkan upload.
   class RenderScene {
   public:
      [[nodiscard]] RenderMaterialHandle addMaterial(RenderMaterial material = {}) {
         material.handle = makeCounterHandle<RenderMaterialHandle>();
         materials_.push_back(std::move(material));
         return materials_.back().handle;
      }

      [[nodiscard]] RenderMeshHandle addMesh(Vector<RenderVertex> vertices, Vector<std::uint32_t> indices,
                                             Bounds bounds = {}) {
         auto mesh = RenderMesh{.handle = makeCounterHandle<RenderMeshHandle>(),
                                .vertices = std::move(vertices),
                                .indices = std::move(indices),
                                .bounds = bounds};
         meshes_.push_back(std::move(mesh));
         return meshes_.back().handle;
      }

      [[nodiscard]] std::expected<RenderInstanceHandle, Error>
      addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local = {},
                  Mat4 world = identityMat4()) {
         if (!findMesh(mesh) || !findMaterial(material)) { return std::unexpected(Error::missing_object); }
         auto instance = RenderInstance{.handle = makeCounterHandle<RenderInstanceHandle>(),
                                        .mesh = mesh,
                                        .material = material,
                                        .local_transform = local,
                                        .world_transform = world};
         instances_.push_back(std::move(instance));
         return instances_.back().handle;
      }

      void setCamera(RenderCamera camera) { camera_ = std::move(camera); }

      void setDirectionalLight(RenderDirectionalLight light) { light_ = std::move(light); }

      void clear() { meshes_ = {}; materials_ = {}; instances_ = {}; camera_ = {}; light_ = {}; }

      [[nodiscard]] const RenderMesh *findMesh(RenderMeshHandle handle) const {
         const auto it = std::ranges::find(meshes_, handle, &RenderMesh::handle);
         return it == meshes_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] const RenderMaterial *findMaterial(RenderMaterialHandle handle) const {
         const auto it = std::ranges::find(materials_, handle, &RenderMaterial::handle);
         return it == materials_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] const RenderInstance *findInstance(RenderInstanceHandle handle) const {
         const auto it = std::ranges::find(instances_, handle, &RenderInstance::handle);
         return it == instances_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] std::size_t meshCount() const { return meshes_.size(); }

      [[nodiscard]] std::size_t materialCount() const { return materials_.size(); }

      [[nodiscard]] std::size_t instanceCount() const { return instances_.size(); }

      [[nodiscard]] const std::optional<RenderCamera> &camera() const { return camera_; }

      [[nodiscard]] const std::optional<RenderDirectionalLight> &directionalLight() const { return light_; }

      [[nodiscard]] const Vector<RenderInstance> &instances() const { return instances_; }

   private:
      Vector<RenderMesh> meshes_{};                  ///< CPU meshes awaiting upload.
      Vector<RenderMaterial> materials_{};           ///< CPU materials awaiting descriptor creation.
      Vector<RenderInstance> instances_{};           ///< Draw items.
      std::optional<RenderCamera> camera_{};         ///< Optional active camera.
      std::optional<RenderDirectionalLight> light_{}; ///< Optional active directional light.
   };

   /// @brief Renderer choice descriptor returned by RenderSystem.
   struct RendererDescriptor {
      using HandleType = RendererHandle;       ///< Descriptor handle type.
      RendererHandle handle{};                 ///< Stable renderer descriptor handle.
      RendererId id{.value = "forward"};       ///< Renderer id chosen by the application.
      bool shadow_maps{true};                  ///< Whether this renderer intends to use shadow maps.
   };

   /// @brief Minimal renderer factory; later it will choose concrete renderer implementations by id.
   class RenderSystem {
   public:
      /// @brief Creates a renderer descriptor from a user-selected renderer id.
      [[nodiscard]] std::expected<RendererDescriptor, Error> createRenderer(RendererId id) const {
         const auto selector = std::string_view{id.value};
         const auto match = std::ranges::find(renderer_choices, selector, &RendererChoice::id);
         if (match != renderer_choices.end()) { return createDescriptor(*match); }
         return std::unexpected(Error::invalid_argument);
      }

      /// @brief Creates the default forward-renderer descriptor.
      [[nodiscard]] RendererDescriptor createForwardRenderer() const {
         return createDescriptor(renderer_choices.front());
      }

   private:
      /// @brief One row in the renderer registry.
      struct RendererChoice {
         std::string_view id{}; ///< Renderer selector id.
         bool shadow_maps{};    ///< Whether the renderer uses shadow maps.
      };

      /// @brief Creates a renderer descriptor from one registry row.
      [[nodiscard]] static RendererDescriptor createDescriptor(RendererChoice choice) {
         return RendererDescriptor{.handle = makeCounterHandle<RendererHandle>(),
                                   .id = RendererId{.value = std::string(choice.id)},
                                   .shadow_maps = choice.shadow_maps};
      }

      inline static constexpr std::array renderer_choices{
          RendererChoice{.id = RendererForward::id(), .shadow_maps = RendererForward::usesShadowMaps()}};

   };

} // namespace vve::v4
