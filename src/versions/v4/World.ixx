export module VEEngine.V4:World;
import std;
export import :ECS;
export import :Window;

/// @file
/// @brief v4 world state and frame-option implementation types.

export namespace vve::v4 {

   class AssetSystem;
   class GuiSystem;
   class WindowSystem;

   /// @brief User-visible state facade used by examples and systems.
   class World {
   public:
      explicit World(ECS &ecs) noexcept : ecs_(ecs) {}

      void bindSubsystems(AssetSystem &assets, GuiSystem &gui, WindowSystem &window_system) noexcept {
         assets_ = std::addressof(assets);
         gui_ = std::addressof(gui);
         window_system_ = std::addressof(window_system);
      }

      [[nodiscard]] ECS &ecs() { return ecs_; }
      [[nodiscard]] const ECS &ecs() const { return ecs_; }
      [[nodiscard]] AssetSystem &assets() { return *assets_; }
      [[nodiscard]] GuiSystem &gui() { return *gui_; }
      [[nodiscard]] WindowSystem &windowSystem() { return *window_system_; }
      [[nodiscard]] const WindowSystem &windowSystem() const { return *window_system_; }

   private:
      ECS &ecs_;
      AssetSystem *assets_{};
      GuiSystem *gui_{};
      WindowSystem *window_system_{};
   };

   /// @brief Heterogeneous user-system storage used by makeEngine().
   template <typename... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{}; ///< User systems stored by value.
   };

   /// @brief Builds a user-system bundle while preserving concrete system types.
   template <typename... TSystems> [[nodiscard]] auto makeUserSystems(TSystems &&...systems) {
      return UserSystems<std::remove_cvref_t<TSystems>...>{
         .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
   }

} // namespace vve::v4
