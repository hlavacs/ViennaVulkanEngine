module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine:World;
import std;
import :ECS;
import :Error;
import :Handle;

export namespace vve {

   class VVE_API World {
   public:
      explicit World(ECS<> &ecs) noexcept;

      [[nodiscard]] ECS<> &ecs() noexcept;
      [[nodiscard]] const ECS<> &ecs() const noexcept;

      [[nodiscard]] std::expected<Handle, Error> createEntity();
      [[nodiscard]] std::expected<Handle, Error> createObject();
      [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const;
      [[nodiscard]] std::expected<void, Error> destroyEntity(Handle entity);
      [[nodiscard]] std::expected<void, Error> destroyObject(Handle entity);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> addComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
      getComponent(Handle entity) const;

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> setComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> hasComponent(Handle entity) const;

      template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> removeComponent(Handle entity);

      template <NotHandle... TComponents> [[nodiscard]] std::expected<Handle, Error> spawn(TComponents &&...components);

      template <NotHandle TComponent, typename TMutator>
         requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
      [[nodiscard]] std::expected<void, Error> modifyComponent(Handle entity, TMutator &&mutator);

   private:
      ECS<> *ecs_;
   };

   inline World::World(ECS<> &ecs) noexcept : ecs_(&ecs) {}

   inline ECS<> &World::ecs() noexcept { return *ecs_; }

   inline const ECS<> &World::ecs() const noexcept { return *ecs_; }

   inline std::expected<Handle, Error> World::createEntity() { return ecs().create(); }

   inline std::expected<Handle, Error> World::createObject() { return createEntity(); }

   inline std::expected<bool, Error> World::exists(Handle entity) const { return ecs().exists(entity); }

   inline std::expected<void, Error> World::destroyEntity(Handle entity) { return ecs().erase(entity); }

   inline std::expected<void, Error> World::destroyObject(Handle entity) { return destroyEntity(entity); }

   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> World::addComponent(Handle entity, TComponent &&component) {
      return ecs().addComponent(entity, std::forward<TComponent>(component));
   }

   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
   World::getComponent(Handle entity) const {
      return ecs().template get<TComponent>(entity);
   }

   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> World::setComponent(Handle entity, TComponent &&component) {
      return ecs().put(entity, std::forward<TComponent>(component));
   }

   template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> World::hasComponent(Handle entity) const {
      return ecs().template hasComponent<TComponent>(entity);
   }

   template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> World::removeComponent(Handle entity) {
      return ecs().template eraseComponent<TComponent>(entity);
   }

   template <NotHandle... TComponents> [[nodiscard]] std::expected<Handle, Error> World::spawn(TComponents &&...components) {
      const auto entity_result = createEntity();
      if (!entity_result) {
         return std::unexpected(entity_result.error());
      }

      const auto entity = *entity_result;
      std::expected<void, Error> add_result{};
      (
          [&] {
             if (add_result) {
                add_result = addComponent(entity, std::forward<TComponents>(components));
             }
          }(),
          ...);

      if (!add_result) {
         static_cast<void>(destroyEntity(entity));
         return std::unexpected(add_result.error());
      }

      return entity;
   }

   template <NotHandle TComponent, typename TMutator>
      requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
   [[nodiscard]] std::expected<void, Error> World::modifyComponent(Handle entity, TMutator &&mutator) {
      auto component_result = getComponent<TComponent>(entity);
      if (!component_result) {
         return std::unexpected(component_result.error());
      }

      if (!component_result->has_value()) {
         return std::unexpected(Error::invalid_argument);
      }

      auto component = **component_result;
      std::forward<TMutator>(mutator)(component);
      return setComponent(entity, std::move(component));
   }

} // namespace vve
