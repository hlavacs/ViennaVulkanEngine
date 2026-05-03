export module VEEngine:Types;
import std;
export import :Math;

/**
 * @file
 * @brief Upper-layer semantic engine types built from the thin math facade.
 */
export namespace vve {

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

   /// @brief Opaque 64-bit id prepared for counter handles and future slot-map handles.
   struct Handle {
      static constexpr std::uint32_t generation_bits{16};                 ///< Future generation bit count.
      static constexpr std::uint32_t id_bits{64 - generation_bits - 1};    ///< Counter/id bit count.
      static constexpr std::uint64_t counter_bit{1ULL << 63U};             ///< High bit marks counter handles.
      static constexpr std::uint64_t id_mask{(1ULL << id_bits) - 1ULL};    ///< Low id/index bits.
      static constexpr std::uint64_t generation_mask{~counter_bit & ~id_mask}; ///< Middle generation bits.

      std::uint64_t value{0}; ///< Raw handle value; zero is invalid.

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

      [[nodiscard]] friend constexpr bool operator==(Handle, Handle) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(Handle, Handle) noexcept = default;
   };

   static_assert(sizeof(Handle) == sizeof(std::uint64_t));

   /// @brief Type-safe handle wrapper; categories share 64-bit storage but not the same C++ type.
   template <typename TTag> struct TypedHandle {
      Handle value{}; ///< Wrapped raw handle.

      constexpr TypedHandle() noexcept = default;
      /// @brief Wraps an existing raw handle explicitly.
      explicit constexpr TypedHandle(Handle raw) noexcept : value(raw) {}

      /// @brief Returns true when this handle is not the invalid zero value.
      [[nodiscard]] constexpr bool valid() const noexcept { return value.valid(); }

      /// @brief Returns true when the handle stores an upward-counted id.
      [[nodiscard]] constexpr bool isCounter() const noexcept { return value.isCounter(); }

      /// @brief Returns true when the handle is shaped as a future slot-map index.
      [[nodiscard]] constexpr bool isSlotMapIndex() const noexcept { return value.isSlotMapIndex(); }

      /// @brief Extracts the future slot-map generation counter.
      [[nodiscard]] constexpr std::uint64_t generation() const noexcept { return value.generation(); }

      /// @brief Extracts the low id bits used by both counter and slot-map handles.
      [[nodiscard]] constexpr std::uint64_t id() const noexcept { return value.id(); }

      /// @brief Names the id bits as a slot index for future slot-map users.
      [[nodiscard]] constexpr std::uint64_t slotIndex() const noexcept { return value.slotIndex(); }

      /// @brief Returns the wrapped raw handle for diagnostics or low-level APIs.
      [[nodiscard]] constexpr Handle raw() const noexcept { return value; }

      [[nodiscard]] friend constexpr bool operator==(TypedHandle, TypedHandle) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(TypedHandle, TypedHandle) noexcept = default;
   };

   static_assert(sizeof(TypedHandle<struct SizeCheckTag>) == sizeof(std::uint64_t));

   /// @brief Builds a future slot-map handle from slot index and generation.
   [[nodiscard]] constexpr Handle makeSlotMapHandle(std::uint32_t slot_index, std::uint32_t generation) noexcept {
      const auto generation_bits = (static_cast<std::uint64_t>(generation) << Handle::id_bits) &
                                   Handle::generation_mask;
      const auto index_bits = static_cast<std::uint64_t>(slot_index) & Handle::id_mask;
      return Handle{generation_bits | index_bits};
   }

   /// @brief Builds an upward-counted non-slot-map handle from an explicit id.
   [[nodiscard]] constexpr Handle makeCounterHandle(std::uint64_t id) noexcept {
      return Handle{Handle::counter_bit | (id & Handle::id_mask)};
   }

   /// @brief Builds an upward-counted non-slot-map handle from the module-global counter.
   [[nodiscard]] inline Handle makeCounterHandle() {
      static std::atomic_uint64_t next_id{1};
      return makeCounterHandle(next_id.fetch_add(1, std::memory_order_relaxed));
   }

   /// @brief Builds a typed future slot-map handle from slot index and generation.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeTypedSlotMapHandle(std::uint32_t slot_index,
                                                          std::uint32_t generation) noexcept {
      return THandle{makeSlotMapHandle(slot_index, generation)};
   }

   /// @brief Builds a typed upward-counted non-slot-map handle from an explicit id.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeTypedCounterHandle(std::uint64_t id) noexcept {
      return THandle{makeCounterHandle(id)};
   }

   /// @brief Builds a typed upward-counted non-slot-map handle from the module-global counter.
   template <typename THandle> [[nodiscard]] inline THandle makeTypedCounterHandle() {
      return THandle{makeCounterHandle()};
   }

   struct EntityTag;     ///< Tag for ECS entity handles.
   struct SceneTag;      ///< Tag for scene handles.
   struct NodeTag;       ///< Tag for scene-node handles.
   struct MeshTag;       ///< Tag for mesh handles.
   struct MaterialTag;   ///< Tag for material handles.
   struct TextureTag;    ///< Tag for texture handles.
   struct LightTag;      ///< Tag for light handles.
   struct CameraTag;     ///< Tag for camera descriptor handles.
   struct WindowTag;     ///< Tag for runtime window handles.
   struct ResourceTag;   ///< Tag for resource handles.
   struct ShaderTag;     ///< Tag for shader handles.
   struct TaskTag;       ///< Tag for task handles.
   struct RenderPassTag; ///< Tag for render-pass handles.
   struct RendererTag;   ///< Tag for renderer descriptor handles.
   struct GuiWidgetTag;  ///< Tag for GUI widget handles.

   using Entity           = TypedHandle<EntityTag>;     ///< Strong handle for ECS entities.
   using SceneHandle      = TypedHandle<SceneTag>;      ///< Strong handle for scene descriptors.
   using NodeHandle       = TypedHandle<NodeTag>;       ///< Strong handle for scene-node descriptors.
   using MeshHandle       = TypedHandle<MeshTag>;       ///< Strong handle for mesh descriptors.
   using MaterialHandle   = TypedHandle<MaterialTag>;   ///< Strong handle for material descriptors.
   using TextureHandle    = TypedHandle<TextureTag>;    ///< Strong handle for texture descriptors.
   using LightHandle      = TypedHandle<LightTag>;      ///< Strong handle for light descriptors.
   using CameraHandle     = TypedHandle<CameraTag>;     ///< Strong handle for camera descriptors.
   using WindowHandle     = TypedHandle<WindowTag>;     ///< Strong handle for runtime windows.
   using ResourceHandle   = TypedHandle<ResourceTag>;   ///< Strong handle for resources.
   using ShaderHandle     = TypedHandle<ShaderTag>;     ///< Strong handle for shader descriptors.
   using TaskHandle       = TypedHandle<TaskTag>;       ///< Strong handle for task descriptors.
   using RenderPassHandle = TypedHandle<RenderPassTag>; ///< Strong handle for render passes.
   using RendererHandle   = TypedHandle<RendererTag>;   ///< Strong handle for renderer descriptors.
   using GuiWidgetHandle  = TypedHandle<GuiWidgetTag>;  ///< Strong handle for GUI widgets.

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

} // namespace vve
