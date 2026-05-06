export module VEEngine:World;
import std;
import VEEngine.V4;
import :ECS;
import :Window;
import :Assets;
import :Gui;

/**
 * @file
 * @brief Public world contract backed by the selected engine implementation.
 */
export namespace vve {

   class World {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::World;

   public:
      explicit World(Impl &implementation) : impl_{implementation} {}
      World(const World &) = delete;
      World(World &&) noexcept = default;
      World &operator=(const World &) = delete;
      World &operator=(World &&) = delete;

      [[nodiscard]] ECS ecs() { return ECS{impl_.ecs()}; }
      [[nodiscard]] ECS ecs() const { return ECS{const_cast<Impl &>(impl_).ecs()}; }
      [[nodiscard]] AssetSystem assets() { return AssetSystem{impl_.assets()}; }
      [[nodiscard]] GuiSystem gui() { return GuiSystem{impl_.gui()}; }
      [[nodiscard]] WindowSystem windowSystem() { return WindowSystem{impl_.windowSystem()}; }
      [[nodiscard]] WindowSystem windowSystem() const { return WindowSystem{const_cast<Impl &>(impl_).windowSystem()}; }

   private:
      Impl &impl_;
   }; ///< Facade world type.

   template <typename... TSystems>
   using UserSystems = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystems<TSystems...>; ///< User systems bundle.

   struct MakeUserSystems {
      template <typename... TSystems> [[nodiscard]] auto operator()(TSystems &&...systems) const {
         return UserSystems<std::remove_cvref_t<TSystems>...>{
            .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
      }
   }; ///< Callable facade user-system bundle factory.

   inline constexpr MakeUserSystems makeUserSystems{}; ///< Facade user-system bundle factory.

} // namespace vve
