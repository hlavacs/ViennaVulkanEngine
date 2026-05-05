export module VEEngine:World;
import std;
import VEEngine.V4;
import :ECS;
import VEEngine.Error;
import VEEngine.Types;
import :Window;
import :WindowSystem;
import :Assets;
import :Gui;

/**
 * @file
 * @brief Public world contract backed by the selected engine implementation.
 */
export namespace vve {

   class WindowSetup {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc;

   public:
      WindowSetup() = default;

      [[nodiscard]] WindowSetup &id(std::string value) {
         impl_.id = std::move(value);
         return *this;
      }
      [[nodiscard]] WindowSetup &title(std::string value) {
         impl_.title = std::move(value);
         return *this;
      }
      [[nodiscard]] WindowSetup &extent(PixelExtent value) {
         impl_.extent = value;
         return *this;
      }
      [[nodiscard]] WindowSetup &position(int x, int y) {
         impl_.x = x;
         impl_.y = y;
         return *this;
      }
      [[nodiscard]] WindowSetup &renderer(RendererId value) {
         impl_.renderer_id = std::move(value);
         return *this;
      }
      [[nodiscard]] WindowSetup &resizable(bool value) {
         impl_.resizable = value;
         return *this;
      }
      [[nodiscard]] WindowSetup &visible(bool value) {
         impl_.visible = value;
         return *this;
      }

      [[nodiscard]] operator Impl() const { return impl_; }

   private:
      Impl impl_{};
   }; ///< Facade startup window option.

   class WindowSetups {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;

   public:
      WindowSetups() = default;
      WindowSetups(std::initializer_list<WindowSetup> windows) {
         impl_.value.clear();
         impl_.value.reserve(windows.size());
         for (const auto &window : windows) { impl_.value.push_back(window); }
      }

      void add(WindowSetup window) { impl_.value.push_back(std::move(window)); }

      [[nodiscard]] operator Impl() const { return impl_; }

   private:
      Impl impl_{};
   }; ///< Facade startup window collection option.

   class World {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::World;

   public:
      explicit World(Impl &implementation) : impl_{implementation} {}
      World(const World &) = delete;
      World(World &&) noexcept = default;
      World &operator=(const World &) = delete;
      World &operator=(World &&) = delete;

      [[nodiscard]] decltype(auto) ecs() { return impl_.ecs(); }
      [[nodiscard]] decltype(auto) ecs() const { return impl_.ecs(); }
      [[nodiscard]] AssetSystem assets() { return AssetSystem{impl_.assets()}; }
      [[nodiscard]] GuiSystem gui() { return GuiSystem{impl_.gui()}; }
      [[nodiscard]] WindowSystem windowSystem() { return WindowSystem{impl_.windowSystem()}; }
      [[nodiscard]] WindowSystem windowSystem() const { return WindowSystem{const_cast<Impl &>(impl_).windowSystem()}; }
      [[nodiscard]] InputState input() { return InputState{impl_.input()}; }
      [[nodiscard]] InputState input() const { return InputState{const_cast<Impl &>(impl_).input()}; }

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
