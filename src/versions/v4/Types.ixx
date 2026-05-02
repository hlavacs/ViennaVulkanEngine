export module VEEngine.V4:Types;
import std;
export import :ECS;
export import VEEngine;

/// @file
/// @brief v4 handle-addressable descriptor types built on the shared Math.ixx geometry layer.

export namespace vve::v4 {

   namespace math = ::vve::math; ///< Version-local alias for the shared math namespace.

   using Scalar = math::Scalar; ///< Short alias for the configured math scalar type.
   using Vec2   = math::Vec2;   ///< Short alias for the configured 2D vector type.
   using Vec3   = math::Vec3;   ///< Short alias for the configured 3D vector type.
   using Vec4   = math::Vec4;   ///< Short alias for the configured 4D vector type.
   using Quat   = math::Quat;   ///< Short alias for the configured quaternion type.
   using Mat4   = math::Mat4;   ///< Short alias for the configured 4x4 matrix type.

   /// @brief Returns the additive identity for the selected scalar type.
   [[nodiscard]] inline constexpr Scalar zero() noexcept { return math::zero(); }
   /// @brief Returns the multiplicative identity for the selected scalar type.
   [[nodiscard]] inline constexpr Scalar one() noexcept { return math::one(); }
   /// @brief Returns the zero vector.
   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return math::zeroVec3(); }
   /// @brief Returns a vector with all coordinates set to one.
   [[nodiscard]] inline Vec3 oneVec3() noexcept { return math::oneVec3(); }
   /// @brief Returns the identity rotation.
   [[nodiscard]] inline Quat identityQuat() noexcept { return math::identityQuat(); }
   /// @brief Returns a 4x4 identity matrix.
   [[nodiscard]] inline Mat4 identityMat4() noexcept { return math::identityMat4(); }

   using Position  = ::vve::Position;  ///< Shared strong position wrapper.
   using Direction = ::vve::Direction; ///< Shared strong direction wrapper.
   using Scale     = ::vve::Scale;     ///< Shared strong scale wrapper.
   using Rotation  = ::vve::Rotation;  ///< Shared strong rotation wrapper.
   using Transform = ::vve::Transform; ///< Shared transform component.
   using Bounds    = ::vve::Bounds;    ///< Shared axis-aligned bounds type.
   using Camera    = ::vve::Camera;    ///< Shared camera geometry type.

   /// @brief Material texture slot meaning.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief High-level light shape.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief A material reference to one texture descriptor.
   struct TextureBinding {
      Handle texture{};                                    ///< Referenced TextureDescriptor handle.
      TextureSemantic semantic{TextureSemantic::unknown};  ///< Intended material slot.
      std::uint32_t uv_set{0};                             ///< UV channel used by the texture.
   };

   /// @brief A scene node reference to renderable geometry and material.
   struct MeshUse {
      Handle mesh{};     ///< Referenced MeshDescriptor handle.
      Handle material{}; ///< Referenced MaterialDescriptor handle.
   };

   /// @brief Tree topology: one root plus parent-to-child handle edges.
   struct Tree {
      Handle root{};                           ///< Root node handle.
      std::multimap<Handle, Handle> children{}; ///< Parent node handle mapped to child node handles.

      /// @brief Adds one parent-to-child tree edge.
      void addChild(Handle parent, Handle child) { children.emplace(parent, child); }
      /// @brief Returns all children for a parent handle.
      [[nodiscard]] auto childRange(Handle parent) const { return children.equal_range(parent); }
   };

   /// @brief Generic directed graph topology stored as parent-to-child handle edges.
   struct Graph {
      std::multimap<Handle, Handle> edges{}; ///< Source node handle mapped to destination node handles.

      /// @brief Adds one directed edge.
      void addEdge(Handle from, Handle to) { edges.emplace(from, to); }
      /// @brief Returns all outgoing edges for a node handle.
      [[nodiscard]] auto childRange(Handle node) const { return edges.equal_range(node); }
      /// @brief Returns nodes in dependency order, or cycle_detected when the graph is cyclic.
      [[nodiscard]] std::expected<Vector<Handle>, Error> topologicalOrder(const Vector<Handle> &nodes) const {
         std::map<Handle, std::uint32_t> incoming{};
         std::map<Handle, Vector<Handle>> outgoing{};
         for (const auto node : nodes) {
            if (!node.valid()) {
               return std::unexpected(Error::invalid_handle);
            }
            incoming.try_emplace(node, 0);
         }

         for (const auto &[from, to] : edges) {
            if (!from.valid() || !to.valid()) {
               return std::unexpected(Error::invalid_handle);
            }
            if (!incoming.contains(from) || !incoming.contains(to)) {
               return std::unexpected(Error::missing_object);
            }
            outgoing[from].push_back(to);
            ++incoming[to];
         }

         std::set<Handle> ready{};
         Vector<Handle> ordered{};
         ordered.reserve(incoming.size());
         for (const auto &[node, count] : incoming) {
            if (count == 0) {
               ready.insert(node);
            }
         }

         while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            ordered.push_back(node);
            for (const auto child : outgoing[node]) {
               auto &count = incoming[child];
               if (--count == 0) {
                  ready.insert(child);
               }
            }
         }

         if (ordered.size() != incoming.size()) {
            return std::unexpected(Error::cycle_detected);
         }
         return ordered;
      }
   };

   /// @brief Scene graph node descriptor stored by handle in ObjectCatalog.
   struct NodeDescriptor {
      Handle handle{};               ///< Stable 64-bit node handle.
      std::string name{};            ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Mesh geometry descriptor; actual vertex buffers are added later.
   struct MeshDescriptor {
      Handle handle{};              ///< Stable 64-bit mesh handle.
      std::string name{};           ///< Human-readable mesh name.
      std::uint64_t vertex_count{0}; ///< Number of vertices in source geometry.
      std::uint64_t index_count{0};  ///< Number of indices in source geometry.
      Handle material{};            ///< Default material handle.
      Bounds bounds{};              ///< Object-space bounds.
   };

   /// @brief Material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      Handle handle{};                  ///< Stable 64-bit material handle.
      std::string name{};               ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Texture descriptor; pixel storage and GPU upload are future steps.
   struct TextureDescriptor {
      Handle handle{};             ///< Stable 64-bit texture handle.
      std::string name{};          ///< Human-readable texture name.
      std::filesystem::path source{}; ///< Source file path or logical asset path.
      std::uint32_t width{0};      ///< Source width in pixels.
      std::uint32_t height{0};     ///< Source height in pixels.
      std::uint32_t channels{0};   ///< Source channel count.
   };

   /// @brief Light descriptor used by renderers and scene systems.
   struct LightDescriptor {
      Handle handle{};                    ///< Stable 64-bit light handle.
      std::string name{};                 ///< Human-readable light name.
      LightKind kind{LightKind::unknown}; ///< Light shape.
      Position position{};                ///< Light position for point/spot lights.
      Direction direction{};              ///< Light direction for directional/spot lights.
      Vec3 color{1.0F, 1.0F, 1.0F};       ///< Linear light color.
      Scalar intensity{1.0F};             ///< Relative light intensity.
   };

   /// @brief Camera descriptor used to create runtime cameras.
   struct CameraDescriptor {
      Handle handle{};                    ///< Stable 64-bit camera handle.
      std::string name{};                 ///< Human-readable camera name.
      Position position{};                ///< Camera position.
      Direction forward{};                ///< Camera forward direction.
      Scalar fov_y_radians{1.0471976F};   ///< Vertical field of view.
      Scalar near_plane{0.1F};            ///< Near clipping plane.
      Scalar far_plane{1000.0F};          ///< Far clipping plane.
   };

   /// @brief Scene descriptor stores only handles to objects kept in descriptor maps.
   struct SceneDescriptor {
      Handle handle{};           ///< Stable 64-bit scene handle.
      std::string name{};        ///< Human-readable scene name.
      Tree tree{};               ///< Scene hierarchy; nodes do not store child vectors.
      Vector<Handle> nodes{};    ///< All node handles in the scene.
      Vector<Handle> meshes{};   ///< Mesh handles used by the scene.
      Vector<Handle> materials{}; ///< Material handles used by the scene.
      Vector<Handle> textures{}; ///< Texture handles used by the scene.
      Vector<Handle> lights{};   ///< Light handles used by the scene.
      Vector<Handle> cameras{};  ///< Camera handles used by the scene.
   };

   /// @brief Simple descriptor table keyed by Handle.
   template <typename TDescriptor> class DescriptorMap {
   public:
      /// @brief Inserts a descriptor; descriptors must expose a valid `handle` member.
      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) {
            return std::unexpected(Error::invalid_handle);
         }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) {
            return std::unexpected(Error::duplicate_object);
         }
         return {};
      }

      /// @brief Finds a descriptor by handle, or returns null.
      [[nodiscard]] const TDescriptor *find(Handle handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Finds a mutable descriptor by handle, or returns null.
      [[nodiscard]] TDescriptor *find(Handle handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Returns true when the map contains the handle.
      [[nodiscard]] bool contains(Handle handle) const { return descriptors_.contains(handle); }
      /// @brief Returns descriptor count.
      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }
      /// @brief Exposes read-only descriptor storage for tests and iteration.
      [[nodiscard]] const std::map<Handle, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<Handle, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central imported-object catalog; every loaded object is found by 64-bit handle.
   struct ObjectCatalog {
      DescriptorMap<SceneDescriptor> scenes{};       ///< Scenes by handle.
      DescriptorMap<NodeDescriptor> nodes{};         ///< Nodes by handle.
      DescriptorMap<MeshDescriptor> meshes{};        ///< Meshes by handle.
      DescriptorMap<MaterialDescriptor> materials{}; ///< Materials by handle.
      DescriptorMap<TextureDescriptor> textures{};   ///< Textures by handle.
      DescriptorMap<LightDescriptor> lights{};       ///< Lights by handle.
      DescriptorMap<CameraDescriptor> cameras{};     ///< Cameras by handle.
   };

} // namespace vve::v4
