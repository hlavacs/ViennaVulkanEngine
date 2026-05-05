export module VEEngine.V4.Handle;
import std;

/// @file
/// @brief v4 typed handle primitive, hash, and factories shared by all handle categories.

export namespace vve::v4 {

   /// @brief Type-safe handle wrapper; categories share 64-bit storage but not the same C++ type.
   template <typename TTag> struct TypedHandle {
      using tag_type = TTag;                                             ///< Handle category tag type.
      static constexpr std::uint32_t generation_bits{16};                 ///< Future generation bit count.
      static constexpr std::uint32_t id_bits{64 - generation_bits - 1};    ///< Counter/id bit count.
      static constexpr std::uint64_t counter_bit{1ULL << 63U};             ///< High bit marks counter handles.
      static constexpr std::uint64_t id_mask{(1ULL << id_bits) - 1ULL};    ///< Low id/index bits.
      static constexpr std::uint64_t generation_mask{~counter_bit & ~id_mask}; ///< Middle generation bits.

      std::uint64_t value{0}; ///< Raw 64-bit handle value; zero is invalid.

      [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }
      [[nodiscard]] constexpr bool isCounter() const noexcept { return (value & counter_bit) != 0; }
      [[nodiscard]] constexpr bool isSlotMapIndex() const noexcept { return valid() && !isCounter(); }
      [[nodiscard]] constexpr std::uint64_t generation() const noexcept { return (value & generation_mask) >> id_bits; }
      [[nodiscard]] constexpr std::uint64_t id() const noexcept { return value & id_mask; }
      [[nodiscard]] constexpr std::uint64_t slotIndex() const noexcept { return id(); }

      [[nodiscard]] friend constexpr bool operator==(TypedHandle, TypedHandle) noexcept = default;
      [[nodiscard]] friend constexpr auto operator<=>(TypedHandle, TypedHandle) noexcept = default;
   };

   static_assert(sizeof(TypedHandle<decltype([] {})>) == sizeof(std::uint64_t));

   /// @brief Hashes typed handles by their 64-bit payload for unordered topology side tables.
   template <typename THandle> struct HandleHash {
      [[nodiscard]] std::size_t operator()(THandle handle) const noexcept {
         if constexpr (requires { handle.value; }) {
            return std::hash<std::uint64_t>{}(handle.value);
         } else {
            return std::hash<std::uint64_t>{}(handle.value());
         }
      }
   };

   namespace detail {

      /// @brief Returns the next process-local id used by all typed counter handles.
      [[nodiscard]] inline std::uint64_t nextCounterHandleId() {
         static std::atomic_uint64_t next_id{1};
         return next_id.fetch_add(1, std::memory_order_relaxed);
      }

   } // namespace detail

   /// @brief Builds a typed upward-counted non-slot-map handle from the module-global counter.
   template <typename THandle> [[nodiscard]] inline THandle makeCounterHandle() {
      if constexpr (requires { typename THandle::implementation_type; }) {
         return THandle{makeCounterHandle<typename THandle::implementation_type>()};
      } else {
         const auto id = detail::nextCounterHandleId();
         return THandle{THandle::counter_bit | (id & THandle::id_mask)};
      }
   }

   /// @brief Builds a deterministic typed counter handle for tests and examples that need stable ids.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeHandleForTest(std::uint64_t id) noexcept {
      if constexpr (requires { typename THandle::implementation_type; }) {
         return THandle{makeHandleForTest<typename THandle::implementation_type>(id)};
      } else {
         return THandle{THandle::counter_bit | (id & THandle::id_mask)};
      }
   }

   /// @brief Builds a deterministic future slot-map handle for tests of the prepared bit layout.
   template <typename THandle>
   [[nodiscard]] constexpr THandle makeSlotMapHandleForTest(std::uint32_t slot_index,
                                                            std::uint32_t generation) noexcept {
      if constexpr (requires { typename THandle::implementation_type; }) {
         return THandle{makeSlotMapHandleForTest<typename THandle::implementation_type>(slot_index, generation)};
      } else {
         const auto generation_bits = (static_cast<std::uint64_t>(generation) << THandle::id_bits) &
                                      THandle::generation_mask;
         const auto index_bits = static_cast<std::uint64_t>(slot_index) & THandle::id_mask;
         return THandle{generation_bits | index_bits};
      }
   }

} // namespace vve::v4
