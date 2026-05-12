export module VEEngine.Handle;
import std;
import VEEngine.V4.Handle;

/**
 * @file
 * @brief Public typed-handle facade backed by the selected engine implementation.
 */
export namespace vve {

   template <typename TTag> class TypedHandle {
   public:
      using tag_type = TTag;
      using implementation_type = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TypedHandle<TTag>;

      constexpr TypedHandle() noexcept = default;
      constexpr explicit TypedHandle(std::uint64_t value) noexcept : impl_{.value = value} {}
      constexpr TypedHandle(implementation_type implementation) noexcept : impl_{implementation} {}

      [[nodiscard]] constexpr std::uint64_t value() const noexcept { return impl_.value; }
      [[nodiscard]] constexpr bool valid() const noexcept { return impl_.valid(); }
      [[nodiscard]] constexpr bool isCounter() const noexcept { return impl_.isCounter(); }
      [[nodiscard]] constexpr bool isSlotMapIndex() const noexcept { return impl_.isSlotMapIndex(); }
      [[nodiscard]] constexpr std::uint64_t generation() const noexcept { return impl_.generation(); }
      [[nodiscard]] constexpr std::uint64_t id() const noexcept { return impl_.id(); }
      [[nodiscard]] constexpr std::uint64_t slotIndex() const noexcept { return impl_.slotIndex(); }

      [[nodiscard]] constexpr implementation_type implementation() const noexcept { return impl_; }
      [[nodiscard]] constexpr operator implementation_type() const noexcept { return impl_; }

      [[nodiscard]] friend constexpr bool operator==(TypedHandle, TypedHandle) noexcept = default;
      [[nodiscard]] friend constexpr bool operator==(TypedHandle lhs, implementation_type rhs) noexcept {
         return lhs.impl_ == rhs;
      }
      [[nodiscard]] friend constexpr bool operator==(implementation_type lhs, TypedHandle rhs) noexcept {
         return lhs == rhs.impl_;
      }
      [[nodiscard]] friend constexpr bool operator<(TypedHandle lhs, TypedHandle rhs) noexcept {
         return lhs.impl_ < rhs.impl_;
      }
      [[nodiscard]] friend constexpr bool operator<(TypedHandle lhs, implementation_type rhs) noexcept {
         return lhs.impl_ < rhs;
      }
      [[nodiscard]] friend constexpr bool operator<(implementation_type lhs, TypedHandle rhs) noexcept {
         return lhs < rhs.impl_;
      }

   private:
      implementation_type impl_{};
   }; ///< Facade typed handle.

   template <typename THandle> [[nodiscard]] inline THandle makeCounterHandle() {
      return THandle{VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeCounterHandle<typename THandle::implementation_type>()};
   } ///< Facade counter-handle builder.

   template <typename THandle> [[nodiscard]] constexpr THandle makeHandleForTest(std::uint64_t id) noexcept {
      return THandle{VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeHandleForTest<typename THandle::implementation_type>(id)};
   } ///< Facade deterministic handle builder.

   template <typename THandle>
   [[nodiscard]] constexpr THandle makeSlotMapHandleForTest(std::uint32_t slot_index,
                                                            std::uint32_t generation) noexcept {
      return THandle{VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeSlotMapHandleForTest<
         typename THandle::implementation_type>(slot_index, generation)};
   } ///< Facade slot-map test handle builder.

} // namespace vve
