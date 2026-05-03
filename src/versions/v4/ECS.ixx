export module VEEngine.V4:ECS;
import std;
export import :Vector;
export import VEEngine;

/// @file
/// @brief v4 compatibility aliases for facade-owned ECS and error types.

export namespace vve::v4 {

   using ::vve::DefaultECSTraits;         ///< Facade ECS trait.
   using ::vve::ECS;                      ///< Facade ECS type.
   using ::vve::Entity;                   ///< Facade ECS entity id.
   using ::vve::Error;                    ///< Facade error vocabulary.
   using ::vve::errorName;                ///< Facade error-name helper.
   using ::vve::makeCounterHandle;        ///< Facade counter-handle builder.
   using ::vve::makeHandleForTest;        ///< Facade deterministic test-handle builder.
   using ::vve::makeSlotMapHandleForTest; ///< Facade deterministic slot-map test builder.

   template <typename TTag> using TypedHandle = ::vve::TypedHandle<TTag>; ///< Facade typed handle.

   template <typename TTraits = DefaultECSTraits>
   using BasicECS = ::vve::BasicECS<TTraits>; ///< Facade ECS template.

} // namespace vve::v4
