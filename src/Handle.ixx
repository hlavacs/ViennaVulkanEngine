export module VEEngine:Handle;
import std;

export namespace vve {

   class Handle {
   public:
      using value_type = std::uint64_t;
      using part_type = std::uint32_t;

      constexpr Handle() noexcept = default;
      constexpr explicit Handle(value_type value) noexcept : value_(value) {}

      constexpr Handle(part_type low, part_type high) noexcept
          : value_(join(low, high)) {}

      template <typename THashable>
         requires(!std::same_as<std::remove_cvref_t<THashable>, Handle> &&
                  requires(const std::remove_cvref_t<THashable> &value) {
                     {
                        std::hash<std::remove_cvref_t<THashable>>{}(value)
                     } -> std::convertible_to<std::size_t>;
                  })
      explicit Handle(const THashable &value) noexcept
          : value_(static_cast<value_type>(
                std::hash<std::remove_cvref_t<THashable>>{}(value))) {}

      [[nodiscard]] static constexpr Handle fromParts(part_type low,
                                                      part_type high) noexcept {
         return Handle(low, high);
      }

      template <typename THashable>
         requires(!std::same_as<std::remove_cvref_t<THashable>, Handle> &&
                  requires(const std::remove_cvref_t<THashable> &value) {
                     {
                        std::hash<std::remove_cvref_t<THashable>>{}(value)
                     } -> std::convertible_to<std::size_t>;
                  })
      [[nodiscard]] static Handle fromHash(const THashable &value) noexcept {
         return Handle(value);
      }

      [[nodiscard]] constexpr value_type value() const noexcept {
         return value_;
      }

      [[nodiscard]] constexpr part_type low() const noexcept {
         return static_cast<part_type>(value_ & 0xFFFFFFFFull);
      }

      [[nodiscard]] constexpr part_type high() const noexcept {
         return static_cast<part_type>(value_ >> 32);
      }

      [[nodiscard]] constexpr std::array<part_type, 2> parts() const noexcept {
         return {low(), high()};
      }

      [[nodiscard]] constexpr explicit operator value_type() const noexcept {
         return value_;
      }

      [[nodiscard]] friend constexpr bool
      operator==(Handle lhs, Handle rhs) noexcept = default;
      [[nodiscard]] friend constexpr auto
      operator<=>(Handle lhs, Handle rhs) noexcept = default;

   private:
      [[nodiscard]] static constexpr value_type join(part_type low,
                                                     part_type high) noexcept {
         return static_cast<value_type>(low) |
                (static_cast<value_type>(high) << 32);
      }

      value_type value_{0};
   };

} // namespace vve
