export module VEEngine:Types;
import std;
export import :Error;
export import :Math;
export import :Graph;

/**
 * @file
 * @brief Upper-layer semantic engine types built from the thin math facade.
 */
export namespace vve {

   template <typename T> using Vector = std::vector<T>; ///< Facade dynamic array type.

   /// @brief Strong wrapper for world or local position values.
   struct Position {
      math::Vec3 value{math::zeroVec3()}; ///< Wrapped coordinate.
   };

   /// @brief Strong wrapper for vectors that should be interpreted as directions.
   struct Direction {
      math::Vec3 value{math::Vec3(math::zero(), math::zero(), -math::one())}; ///< Wrapped direction.
   };

   /// @brief Strong wrapper for non-uniform scale factors.
   struct Scale {
      math::Vec3 value{math::oneVec3()}; ///< Wrapped scale vector.
   };

   /// @brief Strong wrapper for quaternion rotations.
   struct Rotation {
      math::Quat value{math::identityQuat()}; ///< Wrapped orientation.
   };

   /// @brief Strong wrapper for linear RGB color values.
   struct LinearColor {
      math::Vec3 value{math::oneVec3()}; ///< Wrapped linear RGB color.
   };

   /// @brief Strong wrapper for relative light intensity.
   struct LightIntensity {
      math::Scalar value{math::one()}; ///< Wrapped non-negative intensity scale.
   };

   /// @brief Strong wrapper for vertical field-of-view angles.
   struct FovY {
      math::Scalar radians{static_cast<math::Scalar>(1.0471975511965976)}; ///< Wrapped vertical FOV in radians.
   };

   /// @brief Strong wrapper for near and far clipping planes.
   struct ClipPlanes {
      math::Scalar near_plane{static_cast<math::Scalar>(0.1)};     ///< Near clip distance.
      math::Scalar far_plane{static_cast<math::Scalar>(10000.0)}; ///< Far clip distance.
   };

   /// @brief Strong wrapper for frame delta time.
   struct DeltaTime {
      double seconds{1.0 / 60.0}; ///< Elapsed seconds.
   };

   /// @brief Strong wrapper for pixel dimensions.
   struct PixelExtent {
      std::uint32_t width{0};  ///< Width in pixels.
      std::uint32_t height{0}; ///< Height in pixels.
   };

   /// @brief Strong wrapper for human-readable object names.
   struct ObjectName {
      std::string value{}; ///< Wrapped display or diagnostic name.
   };

   /// @brief Strong wrapper for renderer selection identifiers.
   struct RendererId {
      std::string value{}; ///< Wrapped renderer identifier.
   };

   /// @brief Strong wrapper for frame counts and frame indices.
   struct FrameCount {
      std::uint64_t value{0}; ///< Wrapped frame count.
   };

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

   /// @brief Type-safe handle wrapper; categories share 64-bit storage but not the same C++ type.
   template <typename TTag> struct TypedHandle {
      static constexpr std::uint32_t generation_bits{16};                 ///< Future generation bit count.
      static constexpr std::uint32_t id_bits{64 - generation_bits - 1};    ///< Counter/id bit count.
      static constexpr std::uint64_t counter_bit{1ULL << 63U};             ///< High bit marks counter handles.
      static constexpr std::uint64_t id_mask{(1ULL << id_bits) - 1ULL};    ///< Low id/index bits.
      static constexpr std::uint64_t generation_mask{~counter_bit & ~id_mask}; ///< Middle generation bits.

      std::uint64_t value{0}; ///< Raw 64-bit handle value; zero is invalid.

      /// @brief Returns true when this handle is not the invalid zero value.
      [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }

      /// @brief Returns true when the handle stores an upward-counted id.
      [[nodiscard]] constexpr bool isCounter() const noexcept { return (value & counter_bit) != 0; }

      /// @brief Returns true when the handle is shaped as a future slot-map index.
      [[nodiscard]] constexpr bool isSlotMapIndex() const noexcept { return valid() && !isCounter(); }

      /// @brief Extracts the future slot-map generation counter.
      [[nodiscard]] constexpr std::uint64_t generation() const noexcept { return (value & generation_mask) >> id_bits; }

      /// @brief Extracts the low id bits used by both counter and slot-map handles.
      [[nodiscard]] constexpr std::uint64_t id() const noexcept { return value & id_mask; }

      /// @brief Names the id bits as a slot index for future slot-map users.
      [[nodiscard]] constexpr std::uint64_t slotIndex() const noexcept { return id(); }

      [[nodiscard]] friend constexpr bool operator==(TypedHandle, TypedHandle) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(TypedHandle, TypedHandle) noexcept = default;
   };

   static_assert(sizeof(TypedHandle<decltype([] {})>) == sizeof(std::uint64_t));

   namespace detail {

      /// @brief Returns the next process-local id used by all typed counter handles.
      [[nodiscard]] inline std::uint64_t nextCounterHandleId() {
         static std::atomic_uint64_t next_id{1};
         return next_id.fetch_add(1, std::memory_order_relaxed);
      }

   } // namespace detail

   /// @brief Builds a typed upward-counted non-slot-map handle from the module-global counter.
   template <typename THandle> [[nodiscard]] inline THandle makeCounterHandle() {
      const auto id = detail::nextCounterHandleId();
      return THandle{.value = THandle::counter_bit | (id & THandle::id_mask)};
   }

   /// @brief Builds a deterministic typed counter handle for tests and examples that need stable ids.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeHandleForTest(std::uint64_t id) noexcept {
      return THandle{.value = THandle::counter_bit | (id & THandle::id_mask)};
   }

   /// @brief Builds a deterministic future slot-map handle for tests of the prepared bit layout.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeSlotMapHandleForTest(std::uint32_t slot_index,
                                                            std::uint32_t generation) noexcept {
      const auto generation_bits = (static_cast<std::uint64_t>(generation) << THandle::id_bits) &
                                   THandle::generation_mask;
      const auto index_bits = static_cast<std::uint64_t>(slot_index) & THandle::id_mask;
      return THandle{.value = generation_bits | index_bits};
   }

   using Entity           = TypedHandle<decltype([] {})>; ///< Strong handle for ECS entities.
   using SceneHandle      = TypedHandle<decltype([] {})>; ///< Strong handle for scene descriptors.
   using NodeHandle       = TypedHandle<decltype([] {})>; ///< Strong handle for scene-node descriptors.
   using MeshHandle       = TypedHandle<decltype([] {})>; ///< Strong handle for mesh descriptors.
   using MaterialHandle   = TypedHandle<decltype([] {})>; ///< Strong handle for material descriptors.
   using TextureHandle    = TypedHandle<decltype([] {})>; ///< Strong handle for texture descriptors.
   using LightHandle      = TypedHandle<decltype([] {})>; ///< Strong handle for light descriptors.
   using CameraHandle     = TypedHandle<decltype([] {})>; ///< Strong handle for camera descriptors.
   using WindowHandle     = TypedHandle<decltype([] {})>; ///< Strong handle for runtime windows.
   using Tree             = BasicTree<NodeHandle>; ///< Scene-tree topology uses node handles.

   /// @brief Standard transform component shared by all active engine layers.
   struct Transform {
      Position translation{}; ///< Local or world-space translation.
      Rotation rotation{};    ///< Local or world-space orientation.
      Scale scale{};          ///< Local or world-space non-uniform scale.
   };

   /// @brief Axis-aligned bounds described by minimum and maximum positions.
   struct Bounds {
      Position minimum{}; ///< Minimum corner.
      Position maximum{}; ///< Maximum corner.
      bool valid{false};  ///< False until at least one point has been included.
   };

   /// @brief Public camera description used by game code and renderers.
   struct Camera {
      /// @brief World-space camera position.
      Position position{.value = math::Vec3(math::zero(), static_cast<math::Scalar>(1.5),
                                            static_cast<math::Scalar>(6.0))};
      /// @brief View direction.
      Direction forward{.value = math::Vec3(math::zero(), math::zero(), -math::one())};
      /// @brief World-to-view transform.
      math::Mat4 view_transform{math::translate(
          math::identityMat4(),
          math::Vec3(math::zero(), static_cast<math::Scalar>(-1.5),
                     static_cast<math::Scalar>(-6.0)))};
      FovY fov_y{};      ///< Vertical field of view.
      ClipPlanes clip{}; ///< Near/far clip planes.

      /// @brief Builds a camera from an eye position and target point.
      [[nodiscard]] static Camera lookAt(Position position, Position target,
                                         Direction up = Direction{
                                            .value = math::Vec3(math::zero(), math::one(), math::zero())},
                                         FovY fov_y = {}, ClipPlanes clip = {}) {
         Camera camera{};
         camera.position = position;
         camera.forward = Direction{.value = math::subtract(target.value, position.value)};
         camera.view_transform = math::lookAt(position.value, target.value, up.value);
         camera.fov_y = fov_y;
         camera.clip = clip;
         return camera;
      }

   };

   /// @brief Material texture slot meaning visible to apps inspecting imported materials.
   enum class TextureSemantic {
      unknown,    ///< Unclassified texture use.
      base_color, ///< Color/albedo texture.
      normal,     ///< Tangent-space normal texture.
      roughness,  ///< Roughness texture.
      metallic,   ///< Metallic texture.
      emissive,   ///< Emissive texture.
      occlusion   ///< Ambient-occlusion texture.
   };

   /// @brief High-level light shape visible to apps creating or inspecting lights.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

   /// @brief A material reference to one texture descriptor.
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

   /// @brief Scene graph node descriptor stored by handle in ObjectCatalog.
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

   /// @brief Light descriptor used by apps and renderers.
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

   /// @brief Camera descriptor used to create runtime cameras.
   struct CameraDescriptor {
      using HandleType = CameraHandle; ///< Descriptor handle type.
      CameraHandle handle{};           ///< Stable camera handle.
      ObjectName name{};               ///< Human-readable camera name.
      Position position{};             ///< Camera position.
      Direction forward{};             ///< Camera forward direction.
      FovY fov_y{};                    ///< Vertical field of view.
      ClipPlanes clip{};               ///< Near and far clipping planes.
   };

   /// @brief Scene descriptor stores only handles to objects kept in descriptor maps.
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

   /// @brief Simple descriptor table keyed by each descriptor's typed handle.
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

} // namespace vve
