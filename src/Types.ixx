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

   using Entity       = TypedHandle<decltype([] {})>; ///< Strong handle for ECS entities.
   using SceneHandle  = TypedHandle<decltype([] {})>; ///< Strong handle returned by scene-loading calls.
   using WindowHandle = TypedHandle<decltype([] {})>; ///< Strong handle for runtime windows.

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

   /// @brief High-level light shape visible to apps creating or inspecting lights.
   enum class LightKind {
      unknown,     ///< Unclassified light.
      directional, ///< Direction-only light such as the sun.
      point,       ///< Point light with position.
      spot         ///< Spot light with position and direction.
   };

} // namespace vve
