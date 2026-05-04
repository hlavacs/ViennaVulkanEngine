export module VEEngine:ECS;
import std;
import VEEngine.V4;
import :Error;
import :Types;

/**
 * @file
 * @brief Public ECS contract backed by the selected engine implementation.
 */
export namespace vve {

   using DefaultECSTraits = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::DefaultECSTraits; ///< Facade ECS traits.

   template <typename TTraits = DefaultECSTraits>
   using BasicECS = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicECS<TTraits>; ///< Facade ECS template.

   using ECS = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ECS; ///< Default facade ECS.

   template <typename T> concept ECSTraitsLike =
      requires { { T::use_slot_map_handles } -> std::convertible_to<bool>; }; ///< Contract for ECS traits.

   template <typename TECS> concept ECSLike =
      requires(TECS ecs, Entity entity, Transform transform) {
         { ecs.create() } -> std::same_as<Entity>;
         { ecs.exists(entity) } -> std::same_as<bool>;
         { ecs.erase(entity) } -> std::same_as<std::expected<void, Error>>;
         { ecs.add(entity, transform) } -> std::same_as<std::expected<void, Error>>;
         { ecs.template get<Transform>(entity) } -> std::same_as<std::expected<Transform, Error>>;
         { ecs.template tryGet<Transform>(entity) } -> std::same_as<std::expected<std::optional<Transform>, Error>>;
         { ecs.put(entity, transform) } -> std::same_as<std::expected<void, Error>>;
         { ecs.template has<Transform>(entity) } -> std::same_as<std::expected<bool, Error>>;
         { ecs.template remove<Transform>(entity) } -> std::same_as<std::expected<void, Error>>;
         { ecs.template view<Transform>() } -> std::same_as<Vector<Entity>>;
      }; ///< Contract for the public ECS class template.

   static_assert(ECSTraitsLike<DefaultECSTraits>);
   static_assert(ECSLike<BasicECS<>>);
   static_assert(ECSLike<ECS>);

} // namespace vve
