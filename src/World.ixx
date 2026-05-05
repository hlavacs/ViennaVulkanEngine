export module VEEngine:World;
import std;
import VEEngine.V4;
import :ECS;
import :Error;
import :Types;
import :Window;

/**
 * @file
 * @brief Public world contract backed by the selected engine implementation.
 */
export namespace vve {

   using ApplicationName = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::ApplicationName; ///< Facade app-name option.
   using EngineConfig    = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::EngineConfig;    ///< Compact engine config.
   using FrameContext    = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FrameContext;    ///< Per-frame timing context.
   using FrameStatus     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::FrameStatus;     ///< One-frame loop result.
   using MaxFrames       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::MaxFrames;       ///< Facade frame-cap option.

   class World {
   public:
      explicit World(ECS &ecs) : impl_{ecs.impl()} {}
      World(const World &) = delete;
      World(World &&) noexcept = default;
      World &operator=(const World &) = delete;
      World &operator=(World &&) = delete;

      [[nodiscard]] decltype(auto) ecs() { return impl_.ecs(); }
      [[nodiscard]] decltype(auto) ecs() const { return impl_.ecs(); }
      [[nodiscard]] decltype(auto) input() { return impl_.input(); }
      [[nodiscard]] decltype(auto) input() const { return impl_.input(); }
      [[nodiscard]] decltype(auto) windows() { return impl_.windows(); }
      [[nodiscard]] decltype(auto) windows() const { return impl_.windows(); }

      [[nodiscard]] WindowInfo *findWindow(std::string_view id) { return impl_.findWindow(id); }
      [[nodiscard]] WindowInfo *findWindow(WindowHandle handle) { return impl_.findWindow(handle); }
      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const { return impl_.findWindow(id); }
      [[nodiscard]] const WindowInfo *findWindow(WindowHandle handle) const { return impl_.findWindow(handle); }

      template <typename... TComponents>
      [[nodiscard]] std::expected<Entity, Error> spawn(TComponents &&...components) {
         return impl_.spawn(std::forward<TComponents>(components)...);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> addComponent(Entity entity, T component) {
         return impl_.template addComponent<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> setComponent(Entity entity, T component) {
         return impl_.template setComponent<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> getComponent(Entity entity) const {
         return impl_.template getComponent<T>(entity);
      }

      [[nodiscard]] std::expected<void, Error> destroy(Entity entity) { return impl_.destroy(entity); }
      [[nodiscard]] std::expected<void, Error> setTransform(Entity entity, Transform transform) {
         return impl_.setTransform(std::move(entity), std::move(transform));
      }
      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Entity entity) const {
         return impl_.getTransform(std::move(entity));
      }
      [[nodiscard]] std::expected<void, Error> setWindowCamera(WindowHandle window, Entity camera) {
         return impl_.setWindowCamera(window, camera);
      }
      [[nodiscard]] std::expected<void, Error> setWindowCamera(std::string_view window_id, Entity camera) {
         return impl_.setWindowCamera(window_id, camera);
      }
      [[nodiscard]] std::expected<void, Error> clearWindowCamera(WindowHandle window) {
         return impl_.clearWindowCamera(window);
      }
      [[nodiscard]] std::expected<void, Error> clearWindowCamera(std::string_view window_id) {
         return impl_.clearWindowCamera(window_id);
      }
      [[nodiscard]] std::optional<Entity> windowCamera(WindowHandle window) const {
         return impl_.windowCamera(window);
      }
      [[nodiscard]] std::optional<Entity> windowCamera(std::string_view window_id) const {
         return impl_.windowCamera(window_id);
      }
      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) { return impl_.setActiveCamera(camera); }
      [[nodiscard]] std::optional<Entity> activeCamera() const { return impl_.activeCamera(); }
      void setSceneLoader(std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         impl_.setSceneLoader(std::move(loader));
      }
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &path) {
         return impl_.loadScene(path);
      }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::World;

      Impl impl_;
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
