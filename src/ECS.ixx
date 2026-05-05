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

   template <typename... TSystems> class Engine;
   class World;

   template <typename TTraits = DefaultECSTraits> class BasicECS {
   public:
      BasicECS() = default;
      BasicECS(const BasicECS &) = delete;
      BasicECS(BasicECS &&) = delete;
      BasicECS &operator=(const BasicECS &) = delete;
      BasicECS &operator=(BasicECS &&) = delete;

      [[nodiscard]] Entity create() { return impl().create(); }
      [[nodiscard]] bool exists(Entity entity) const { return impl().exists(entity); }
      [[nodiscard]] std::expected<void, Error> erase(Entity entity) { return impl().erase(entity); }

      template <typename T> [[nodiscard]] std::expected<void, Error> add(Entity entity, T component) {
         return impl().template add<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<T, Error> get(Entity entity) const {
         return impl().template get<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const {
         return impl().template tryGet<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> put(Entity entity, T component) {
         return impl().template put<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<bool, Error> has(Entity entity) const {
         return impl().template has<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> remove(Entity entity) {
         return impl().template remove<T>(entity);
      }

      template <typename... T> [[nodiscard]] Vector<Entity> view() const { return impl().template view<T...>(); }

   protected:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicECS<TTraits>;

      explicit BasicECS(Impl &implementation) noexcept : impl_{std::addressof(implementation)} {}

   private:
      [[nodiscard]] Impl &impl() { return *impl_; }
      [[nodiscard]] const Impl &impl() const { return *impl_; }

      Impl owned_{};
      Impl *impl_{std::addressof(owned_)};

      template <typename... TSystems> friend class Engine;
      friend class World;
   }; ///< Facade ECS template.

   class ECS : public BasicECS<> {
   public:
      ECS() = default;
      ECS(const ECS &) = delete;
      ECS(ECS &&) = delete;
      ECS &operator=(const ECS &) = delete;
      ECS &operator=(ECS &&) = delete;

   private:
      explicit ECS(VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ECS &implementation) noexcept : BasicECS<>(implementation) {}

      template <typename... TSystems> friend class Engine;
      friend class World;
   }; ///< Default facade ECS.

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

   static_assert(ECSLike<BasicECS<>>);
   static_assert(ECSLike<ECS>);
   static_assert(ECSTraitsLike<DefaultECSTraits>);

} // namespace vve
