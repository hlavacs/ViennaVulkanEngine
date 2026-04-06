export module VEEngine:Handle;
import std;

/**
 * @file
 * @brief Stable 64-bit handle type used across engine-facing APIs.
 *
 * This module centralizes the engine's generic identity type so subsystem
 * facades can exchange ids without exposing storage layout details.
 */

export namespace vve {

   /**
    * @brief Opaque value type used to identify engine objects and resources.
    *
    * A handle is a trivially copyable 64-bit token. It can be assembled from
    * two 32-bit parts or derived from a stable hash seed when deterministic
    * naming is preferable to sequential allocation.
    */
   class Handle {
   public:
      using value_type = std::uint64_t;
      using part_type = std::uint32_t;
      static constexpr value_type invalid_value = std::numeric_limits<value_type>::max();

      /// @brief Creates an invalid handle.
      constexpr Handle() noexcept = default; 
      /// @brief Wraps an already constructed raw handle value.
      constexpr explicit Handle(value_type value) noexcept : value_(value) {}

      /// @brief Builds a handle from low and high 32-bit parts.
      constexpr Handle(part_type low, part_type high) noexcept : value_(join(low, high)) {}

      template <typename THashable>
         requires(!std::same_as<std::remove_cvref_t<THashable>, Handle> &&
                  requires(const std::remove_cvref_t<THashable> &value) {
                     { std::hash<std::remove_cvref_t<THashable>>{}(value) } -> std::convertible_to<std::size_t>;
                  })
      /// @brief Builds a handle from the hash of a stable seed value.
      explicit Handle(const THashable &value) noexcept
          : value_(static_cast<value_type>(std::hash<std::remove_cvref_t<THashable>>{}(value))) {}

      /// @brief Creates a handle from explicit low and high 32-bit parts.
      [[nodiscard]] static constexpr Handle fromParts(part_type low, part_type high) noexcept {
         return Handle(low, high);
      }

      /// @brief Returns the sentinel invalid handle.
      [[nodiscard]] static constexpr Handle invalid() noexcept { return Handle(invalid_value); }

      template <typename THashable>
         requires(!std::same_as<std::remove_cvref_t<THashable>, Handle> &&
                  requires(const std::remove_cvref_t<THashable> &value) {
                     { std::hash<std::remove_cvref_t<THashable>>{}(value) } -> std::convertible_to<std::size_t>;
                  })
      /// @brief Creates a handle from the hash of a seed value.
      [[nodiscard]] static Handle fromHash(const THashable &value) noexcept {
         return Handle(value);
      }

      /// @brief Returns the raw 64-bit representation.
      [[nodiscard]] constexpr value_type value() const noexcept { return value_; }
      /// @brief Returns whether this handle is not the invalid sentinel.
      [[nodiscard]] constexpr bool isValid() const noexcept { return value_ != invalid_value; }

      /// @brief Returns the low 32 bits of the handle.
      [[nodiscard]] constexpr part_type low() const noexcept { return static_cast<part_type>(value_ & 0xFFFFFFFFull); }

      /// @brief Returns the high 32 bits of the handle.
      [[nodiscard]] constexpr part_type high() const noexcept { return static_cast<part_type>(value_ >> 32); }

      /// @brief Returns both 32-bit parts as `{low, high}`.
      [[nodiscard]] constexpr std::array<part_type, 2> parts() const noexcept { return {low(), high()}; }

      /// @brief Explicitly converts the handle to its raw 64-bit value.
      [[nodiscard]] constexpr explicit operator value_type() const noexcept { return value_; }

      [[nodiscard]] friend constexpr bool operator==(Handle lhs, Handle rhs) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(Handle lhs, Handle rhs) noexcept = default;

   private:
      [[nodiscard]] static constexpr value_type join(part_type low, part_type high) noexcept {
         return static_cast<value_type>(low) | (static_cast<value_type>(high) << 32);
      }

      value_type value_{invalid_value};
   };

   /// @brief Handles are expected to remain storage-compatible with their raw value.
   static_assert(sizeof(Handle) == sizeof(Handle::value_type));

} // namespace vve
