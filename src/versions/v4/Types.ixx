export module VEEngine.V4:Types;
import std;
export import :ECS;
export import VEEngine;

/// @file
/// @brief v4 aliases for facade data plus v4-internal graph/resource handle types.

export namespace vve::v4 {

   namespace math = ::vve::math; ///< Version-local alias for the shared math namespace.

   using Scalar = math::Scalar; ///< Short alias for the configured math scalar type.
   using Vec2   = math::Vec2;   ///< Short alias for the configured 2D vector type.
   using Vec3   = math::Vec3;   ///< Short alias for the configured 3D vector type.
   using Vec4   = math::Vec4;   ///< Short alias for the configured 4D vector type.
   using Quat   = math::Quat;   ///< Short alias for the configured quaternion type.
   using Mat4   = math::Mat4;   ///< Short alias for the configured 4x4 matrix type.

   [[nodiscard]] inline constexpr Scalar zero() noexcept { return math::zero(); } ///< Scalar zero.

   [[nodiscard]] inline constexpr Scalar one() noexcept { return math::one(); } ///< Scalar one.

   [[nodiscard]] inline Vec3 zeroVec3() noexcept { return math::zeroVec3(); } ///< Zero 3D vector.

   [[nodiscard]] inline Vec3 oneVec3() noexcept { return math::oneVec3(); } ///< One-filled 3D vector.

   [[nodiscard]] inline Quat identityQuat() noexcept { return math::identityQuat(); } ///< Identity rotation.

   [[nodiscard]] inline Mat4 identityMat4() noexcept { return math::identityMat4(); } ///< Identity matrix.

   using ::vve::Bounds;
   using ::vve::Camera;
   using ::vve::ClipPlanes;
   using ::vve::DeltaTime;
   using ::vve::Direction;
   using ::vve::FovY;
   using ::vve::FrameCount;
   using ::vve::LightIntensity;
   using ::vve::LightKind;
   using ::vve::LinearColor;
   using ::vve::ObjectName;
   using ::vve::PixelExtent;
   using ::vve::Position;
   using ::vve::RendererId;
   using ::vve::Rotation;
   using ::vve::Scale;
   using ::vve::SceneHandle;
   using ::vve::Transform;

   /// @brief Strong wrapper for source vertex counts.
   struct VertexCount {
      std::uint64_t value{0}; ///< Wrapped vertex count.
   };

   /// @brief Strong wrapper for source index counts.
   struct IndexCount {
      std::uint64_t value{0}; ///< Wrapped index count.
   };

   /// @brief Strong wrapper for texture channel counts.
   struct TextureChannelCount {
      std::uint32_t value{0}; ///< Wrapped channel count.
   };

   using NodeHandle     = TypedHandle<decltype([] {})>; ///< v4 scene-node descriptor handle.
   using MeshHandle     = TypedHandle<decltype([] {})>; ///< v4 mesh descriptor handle.
   using MaterialHandle = TypedHandle<decltype([] {})>; ///< v4 material descriptor handle.
   using TextureHandle  = TypedHandle<decltype([] {})>; ///< v4 texture descriptor handle.
   using LightHandle    = TypedHandle<decltype([] {})>; ///< v4 light descriptor handle.
   using CameraHandle   = TypedHandle<decltype([] {})>; ///< v4 imported-camera descriptor handle.

   using Tree = BasicTree<NodeHandle>; ///< v4 scene-tree topology.

   using ResourceHandle  = TypedHandle<decltype([] {})>; ///< v4-internal resource handle.
   using ShaderHandle    = TypedHandle<decltype([] {})>; ///< v4-internal shader descriptor handle.
   using RendererHandle  = TypedHandle<decltype([] {})>; ///< v4-internal renderer descriptor handle.
   using GuiWidgetHandle = TypedHandle<decltype([] {})>; ///< v4-internal GUI widget handle.

   /// @brief Material texture slot meaning for imported v4 material descriptors.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief A material reference to one v4 texture descriptor.
   struct TextureBinding {
      TextureHandle texture{};                            ///< Referenced texture handle.
      TextureSemantic semantic{TextureSemantic::unknown}; ///< Intended material slot.
      std::uint32_t uv_set{0};                            ///< UV channel used by the texture.
   };

   /// @brief A scene node reference to renderable geometry and material.
   struct MeshUse {
      MeshHandle mesh{};         ///< Referenced mesh handle.
      MaterialHandle material{}; ///< Referenced material handle.
   };

   /// @brief Scene graph node descriptor stored by handle in the v4 object catalog.
   struct NodeDescriptor {
      using HandleType = NodeHandle; ///< Descriptor handle type.
      NodeHandle handle{};           ///< Stable node handle.
      ObjectName name{};             ///< Human-readable node name.
      Transform transform{};         ///< Local transform.
      Vector<MeshUse> meshes{};      ///< Mesh/material pairs attached to this node.
   };

   /// @brief Mesh geometry descriptor; actual vertex buffers are added later.
   struct MeshDescriptor {
      using HandleType = MeshHandle; ///< Descriptor handle type.
      MeshHandle handle{};           ///< Stable mesh handle.
      ObjectName name{};             ///< Human-readable mesh name.
      VertexCount vertex_count{};    ///< Number of vertices in source geometry.
      IndexCount index_count{};      ///< Number of indices in source geometry.
      MaterialHandle material{};     ///< Default material handle.
      Bounds bounds{};               ///< Object-space bounds.
   };

   /// @brief Material descriptor referencing textures by handle.
   struct MaterialDescriptor {
      using HandleType = MaterialHandle; ///< Descriptor handle type.
      MaterialHandle handle{};           ///< Stable material handle.
      ObjectName name{};                 ///< Human-readable material name.
      Vector<TextureBinding> textures{}; ///< Texture slots used by this material.
   };

   /// @brief Texture descriptor; pixel storage and GPU upload are engine implementation work.
   struct TextureDescriptor {
      using HandleType = TextureHandle;    ///< Descriptor handle type.
      TextureHandle handle{};              ///< Stable texture handle.
      ObjectName name{};                   ///< Human-readable texture name.
      std::filesystem::path source{};      ///< Source file path or logical asset path.
      PixelExtent extent{};                ///< Source dimensions in pixels.
      TextureChannelCount channels{};      ///< Source channel count.
   };

   /// @brief v4 imported-light descriptor.
   struct LightDescriptor {
      using HandleType = LightHandle;     ///< Descriptor handle type.
      LightHandle handle{};               ///< Stable light handle.
      ObjectName name{};                  ///< Human-readable light name.
      LightKind kind{LightKind::unknown}; ///< Light shape.
      Position position{};                ///< Light position for point/spot lights.
      Direction direction{};              ///< Light direction for directional/spot lights.
      LinearColor color{};                ///< Linear light color.
      LightIntensity intensity{};         ///< Relative light intensity.
   };

   /// @brief v4 imported-camera descriptor.
   struct CameraDescriptor {
      using HandleType = CameraHandle; ///< Descriptor handle type.
      CameraHandle handle{};           ///< Stable camera handle.
      ObjectName name{};               ///< Human-readable camera name.
      Position position{};             ///< Camera position.
      Direction forward{};             ///< Camera forward direction.
      FovY fov_y{};                    ///< Vertical field of view.
      ClipPlanes clip{};               ///< Near and far clipping planes.
   };

   /// @brief Scene descriptor stores only handles to objects kept in v4 descriptor maps.
   struct SceneDescriptor {
      using HandleType = SceneHandle;     ///< Descriptor handle type.
      SceneHandle handle{};               ///< Stable scene handle.
      ObjectName name{};                  ///< Human-readable scene name.
      Tree tree{};                        ///< Scene hierarchy; nodes do not store child vectors.
      Vector<NodeHandle> nodes{};         ///< All node handles in the scene.
      Vector<MeshHandle> meshes{};        ///< Mesh handles used by the scene.
      Vector<MaterialHandle> materials{}; ///< Material handles used by the scene.
      Vector<TextureHandle> textures{};   ///< Texture handles used by the scene.
      Vector<LightHandle> lights{};       ///< Light handles used by the scene.
      Vector<CameraHandle> cameras{};     ///< Camera handles used by the scene.
   };

   /// @brief Simple v4 descriptor table keyed by each descriptor's typed handle.
   template <typename TDescriptor> class DescriptorMap {
   public:
      using HandleType = typename TDescriptor::HandleType; ///< Strong handle accepted by this map.

      /// @brief Inserts a descriptor; descriptors must expose a valid `handle` member.
      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return {};
      }

      /// @brief Finds a descriptor by handle, or returns null.
      [[nodiscard]] const TDescriptor *find(HandleType handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Finds a mutable descriptor by handle, or returns null.
      [[nodiscard]] TDescriptor *find(HandleType handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      /// @brief Returns true when the map contains the handle.
      [[nodiscard]] bool contains(HandleType handle) const { return descriptors_.contains(handle); }

      /// @brief Removes a descriptor by handle.
      [[nodiscard]] std::expected<void, Error> remove(HandleType handle) {
         if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
         if (descriptors_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
         return {};
      }

      /// @brief Returns descriptor count.
      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }

      /// @brief Exposes read-only descriptor storage for tests and iteration.
      [[nodiscard]] const std::map<HandleType, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<HandleType, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central v4 imported-object catalog; every loaded object is found by 64-bit handle.
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
