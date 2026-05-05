export module VEEngine:ECS;
import std;
import VEEngine.V4;
import VEEngine.Error;
import VEEngine.Types;

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
      BasicECS(const BasicECS &) = default;
      BasicECS(BasicECS &&) noexcept = default;
      BasicECS &operator=(const BasicECS &) = default;
      BasicECS &operator=(BasicECS &&) noexcept = default;

      [[nodiscard]] Entity create() { return impl_.create(); }
      [[nodiscard]] bool exists(Entity entity) const { return impl_.exists(entity); }
      [[nodiscard]] std::expected<void, Error> erase(Entity entity) { return impl_.erase(entity); }

      template <typename T> [[nodiscard]] std::expected<void, Error> add(Entity entity, T component) {
         return impl_.template add<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<T, Error> get(Entity entity) const {
         return impl_.template get<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> tryGet(Entity entity) const {
         return impl_.template tryGet<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> put(Entity entity, T component) {
         return impl_.template put<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<bool, Error> has(Entity entity) const {
         return impl_.template has<T>(entity);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> remove(Entity entity) {
         return impl_.template remove<T>(entity);
      }

      template <typename... T> [[nodiscard]] Vector<Entity> view() const {
         Vector<Entity> result{};
         for (const auto entity : impl_.template view<T...>()) {
            result.push_back(Entity{entity});
         }
         return result;
      }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicECS<TTraits>;

      [[nodiscard]] Impl &impl() { return impl_; }
      [[nodiscard]] const Impl &impl() const { return impl_; }

      Impl impl_{};

      friend class World;
   }; ///< Facade ECS template.

   class ECS : public BasicECS<> {
   public:
      ECS() = default;
      ECS(const ECS &) = default;
      ECS(ECS &&) noexcept = default;
      ECS &operator=(const ECS &) = default;
      ECS &operator=(ECS &&) noexcept = default;
   }; ///< Default facade ECS.

} // namespace vve
