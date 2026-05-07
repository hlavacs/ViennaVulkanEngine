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
      World(ECS &ecs, AssetSystem &assets, GuiSystem &gui, WindowSystem &window_system) noexcept;

      [[nodiscard]] ECS &ecs();
      [[nodiscard]] const ECS &ecs() const;
      [[nodiscard]] AssetSystem &assets();
      [[nodiscard]] GuiSystem &gui();
      [[nodiscard]] WindowSystem &windowSystem();
      [[nodiscard]] const WindowSystem &windowSystem() const;

   private:
      ECS &ecs_;
      AssetSystem &assets_;
      GuiSystem &gui_;
      WindowSystem &window_system_;
   };

   /// @brief Creates a world view over engine-owned subsystems.
   inline World::World(ECS &ecs, AssetSystem &assets, GuiSystem &gui, WindowSystem &window_system) noexcept
       : ecs_{ecs}, assets_{assets}, gui_{gui}, window_system_{window_system} {}

   /// @brief Returns the ECS implementation owned by the engine.
   inline ECS &World::ecs() { return ecs_; }

   /// @brief Returns the ECS implementation owned by the engine.
   inline const ECS &World::ecs() const { return ecs_; }

   /// @brief Returns the asset-system implementation bound by the engine.
   inline AssetSystem &World::assets() { return assets_; }

   /// @brief Returns the GUI-system implementation bound by the engine.
   inline GuiSystem &World::gui() { return gui_; }

   /// @brief Returns the window-system implementation bound by the engine.
   inline WindowSystem &World::windowSystem() { return window_system_; }

   /// @brief Returns the window-system implementation bound by the engine.
   inline const WindowSystem &World::windowSystem() const { return window_system_; }

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
