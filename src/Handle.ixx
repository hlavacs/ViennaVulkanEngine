export module VEEngine:Handle;
import std;
import VEEngine.V4;

/**
 * @file
 * @brief Public typed-handle facade backed by the selected engine implementation.
 */
export namespace vve {

   template <typename TTag>
   using TypedHandle = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::TypedHandle<TTag>; ///< Facade typed handle.

   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeCounterHandle;        ///< Facade counter-handle builder.
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeHandleForTest;        ///< Facade deterministic handle builder.
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeSlotMapHandleForTest; ///< Facade slot-map test handle builder.

   template <typename THandle> concept TypedHandleLike =
      requires(THandle handle) {
         { THandle::counter_bit } -> std::convertible_to<std::uint64_t>;
         { THandle::id_mask } -> std::convertible_to<std::uint64_t>;
         { handle.value } -> std::convertible_to<std::uint64_t>;
         { handle.valid() } -> std::same_as<bool>;
         { handle.isCounter() } -> std::same_as<bool>;
         { handle.isSlotMapIndex() } -> std::same_as<bool>;
         { handle.generation() } -> std::convertible_to<std::uint64_t>;
         { handle.id() } -> std::convertible_to<std::uint64_t>;
         { handle.slotIndex() } -> std::convertible_to<std::uint64_t>;
      }; ///< Contract for all public 64-bit typed handle aliases.

   template <typename THandle> concept CounterHandleFactoryLike = TypedHandleLike<THandle> && requires {
      { makeCounterHandle<THandle>() } -> std::same_as<THandle>;
   }; ///< Contract for makeCounterHandle<THandle>().

   template <typename THandle> concept TestHandleFactoryLike = TypedHandleLike<THandle> && requires {
      { makeHandleForTest<THandle>(std::uint64_t{1}) } -> std::same_as<THandle>;
   }; ///< Contract for makeHandleForTest<THandle>(id).

   template <typename THandle> concept SlotMapHandleFactoryLike = TypedHandleLike<THandle> && requires {
      { makeSlotMapHandleForTest<THandle>(std::uint32_t{1}, std::uint32_t{1}) } -> std::same_as<THandle>;
   }; ///< Contract for makeSlotMapHandleForTest<THandle>(slot, generation).

   static_assert(TypedHandleLike<TypedHandle<decltype([] {})>>);

} // namespace vve
