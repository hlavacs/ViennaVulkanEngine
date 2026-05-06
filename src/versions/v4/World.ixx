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
      [[nodiscard]] InputState &input() { return input_; }
      [[nodiscard]] const InputState &input() const { return input_; }

      template <typename... TComponents>
      [[nodiscard]] std::expected<Entity, Error> spawn(TComponents &&...components) {
         const Entity entity = ecs_.create();
         if constexpr (sizeof...(TComponents) > 0) {
            if ((!ecs_.add(entity, std::forward<TComponents>(components)) || ...)) {
               (void)ecs_.erase(entity);
               return std::unexpected(Error::duplicate_component);
            }
         }
         return entity;
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> addComponent(Entity entity, T component) {
         return ecs_.add(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> setComponent(Entity entity, T component) {
         return ecs_.put(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> getComponent(Entity entity) const {
         return ecs_.tryGet<T>(entity);
      }

      [[nodiscard]] std::expected<void, Error> destroy(Entity entity) { return ecs_.erase(entity); }

      [[nodiscard]] std::expected<void, Error> setTransform(Entity entity, Transform transform) {
         return setComponent(entity, std::move(transform));
      }

      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Entity entity) const {
         return getComponent<Transform>(entity);
      }

      [[nodiscard]] std::expected<void, Error> setWindowCamera(WindowHandle window, Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         return window_system_->setWindowCamera(window, camera);
      }

      [[nodiscard]] std::expected<void, Error> setWindowCamera(std::string_view window_id, Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         return window_system_->setWindowCamera(window_id, camera);
      }

      [[nodiscard]] std::expected<void, Error> clearWindowCamera(WindowHandle window) {
         return window_system_->clearWindowCamera(window);
      }

      [[nodiscard]] std::expected<void, Error> clearWindowCamera(std::string_view window_id) {
         return window_system_->clearWindowCamera(window_id);
      }

      [[nodiscard]] std::optional<Entity> windowCamera(WindowHandle window) const {
         return window_system_->windowCamera(window);
      }

      [[nodiscard]] std::optional<Entity> windowCamera(std::string_view window_id) const {
         return window_system_->windowCamera(window_id);
      }

      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         return window_system_->setActiveCamera(camera);
      }

      [[nodiscard]] std::optional<Entity> activeCamera() const { return window_system_->activeCamera(); }

      void setSceneLoader(std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         scene_loader_ = std::move(loader);
      }

      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &path) {
         if (!scene_loader_) { return std::unexpected(Error::missing_object); }
         return scene_loader_(path);
      }

   private:
      ECS &ecs_;
      AssetSystem *assets_{};
      GuiSystem *gui_{};
      WindowSystem *window_system_{};
      InputState input_{};
      std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> scene_loader_{};
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
