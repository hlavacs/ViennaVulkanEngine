export module VEEngine.V4:Types;
import std;
export import VEEngine.V4.Vector;
export import VEEngine.V4.Error;
export import VEEngine.V4.Math;
export import VEEngine.Types;
export import VEEngine.V4.Handle;
export import VEEngine.V4.Graph;

/// @file
/// @brief v4 implementation types.

export namespace vve::v4 {

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

   using Entity         = vve::Entity;         ///< v4 ECS entity handle contract.
   using SceneHandle    = vve::SceneHandle;    ///< v4 scene descriptor handle contract.
   using WindowHandle   = vve::WindowHandle;   ///< v4 runtime window handle contract.
   using NodeHandle     = vve::NodeHandle;     ///< v4 scene-node descriptor handle contract.
   using MeshHandle     = vve::MeshHandle;     ///< v4 mesh descriptor handle contract.
   using MaterialHandle = vve::MaterialHandle; ///< v4 material descriptor handle contract.
   using TextureHandle  = vve::TextureHandle;  ///< v4 texture descriptor handle contract.
   using LightHandle    = vve::LightHandle;    ///< v4 light descriptor handle contract.
   using CameraHandle   = vve::CameraHandle;   ///< v4 imported-camera descriptor handle contract.

   using ApplicationName = vve::ApplicationName; ///< v4 app-name contract.
   using EngineConfig    = vve::EngineConfig;    ///< v4 compact engine config contract.
   using FrameContext    = vve::FrameContext;    ///< v4 frame context contract.
   using FrameStatus     = vve::FrameStatus;     ///< v4 frame status contract.
   using MaxFrames       = vve::MaxFrames;       ///< v4 frame-cap option contract.

   using Tree = BasicTree<NodeHandle>; ///< v4 scene-tree topology.

   using ResourceHandle  = TypedHandle<decltype([] {})>; ///< v4-internal resource handle.
   using ShaderHandle    = TypedHandle<decltype([] {})>; ///< v4-internal shader descriptor handle.
   using RendererHandle  = TypedHandle<decltype([] {})>; ///< v4-internal renderer descriptor handle.
   using GuiWidgetHandle = TypedHandle<decltype([] {})>; ///< v4-internal GUI widget handle.

   using vve::Bounds;              ///< Public axis-aligned bounds contract.
   using vve::Camera;              ///< Public camera contract.
   using vve::ClipPlanes;          ///< Public clip-plane contract.
   using vve::DeltaTime;           ///< Public frame delta contract.
   using vve::Direction;           ///< Public direction contract.
   using vve::FovY;                ///< Public vertical field-of-view contract.
   using vve::FrameCount;          ///< Public frame-count contract.
   using vve::IndexCount;          ///< Public index-count contract.
   using vve::LightIntensity;      ///< Public light-intensity contract.
   using vve::LightKind;           ///< Public light-kind contract.
   using vve::LinearColor;         ///< Public linear-color contract.
   using vve::ObjectName;          ///< Public object-name contract.
   using vve::PixelExtent;         ///< Public pixel-extent contract.
   using vve::Position;            ///< Public position contract.
   using vve::RendererId;          ///< Public renderer-id contract.
   using vve::Rotation;            ///< Public rotation contract.
   using vve::Scale;               ///< Public scale contract.
   using vve::TextureChannelCount; ///< Public texture-channel-count contract.
   using vve::TextureBinding;      ///< Public texture binding contract.
   using vve::TextureDescriptor;   ///< Public texture descriptor contract.
   using vve::TextureSemantic;     ///< Public texture semantic contract.
   using vve::Transform;           ///< Public transform contract.
   using vve::VertexCount;         ///< Public vertex-count contract.
   using vve::CameraDescriptor;   ///< Public camera descriptor contract.
   using vve::LightDescriptor;    ///< Public light descriptor contract.
   using vve::MaterialDescriptor; ///< Public material descriptor contract.
   using vve::MeshDescriptor;     ///< Public mesh descriptor contract.
   using vve::MeshUse;            ///< Public mesh use contract.
   using vve::NodeDescriptor;     ///< Public node descriptor contract.

   /// @brief Scene descriptor stores handles to objects kept in the v4 object catalog.
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

      [[nodiscard]] std::expected<void, Error> add(TDescriptor descriptor) {
         if (!descriptor.handle.valid()) { return std::unexpected(Error::invalid_handle); }
         const auto [_, inserted] = descriptors_.emplace(descriptor.handle, std::move(descriptor));
         if (!inserted) { return std::unexpected(Error::duplicate_object); }
         return {};
      }

      [[nodiscard]] const TDescriptor *find(HandleType handle) const {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      [[nodiscard]] TDescriptor *find(HandleType handle) {
         const auto it = descriptors_.find(handle);
         return it == descriptors_.end() ? nullptr : std::addressof(it->second);
      }

      [[nodiscard]] bool contains(HandleType handle) const { return descriptors_.contains(handle); }

      [[nodiscard]] std::expected<void, Error> remove(HandleType handle) {
         if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
         if (descriptors_.erase(handle) == 0) { return std::unexpected(Error::missing_object); }
         return {};
      }

      [[nodiscard]] std::size_t size() const { return descriptors_.size(); }
      [[nodiscard]] const std::map<HandleType, TDescriptor> &all() const { return descriptors_; }

   private:
      std::map<HandleType, TDescriptor> descriptors_{}; ///< Ordered descriptor storage.
   };

   /// @brief Central v4 imported-object catalog; every loaded object is found by typed handle.
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
