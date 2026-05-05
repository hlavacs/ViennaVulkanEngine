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
      explicit World(ECS &ecs)
         : owned_{std::make_unique<Impl>(ecs.impl())}, impl_{owned_.get()}, ecs_{std::addressof(ecs)},
           input_{impl().input()} {}
      World(const World &) = delete;
      World(World &&) = delete;
      World &operator=(const World &) = delete;
      World &operator=(World &&) = delete;

      [[nodiscard]] ECS &ecs() { return *ecs_; }
      [[nodiscard]] const ECS &ecs() const { return *ecs_; }
      [[nodiscard]] InputState &input() { return input_; }
      [[nodiscard]] const InputState &input() const { return input_; }
      [[nodiscard]] Vector<WindowInfo> &windows() { return impl().windows(); }
      [[nodiscard]] const Vector<WindowInfo> &windows() const { return impl().windows(); }

      [[nodiscard]] WindowInfo *findWindow(std::string_view id) { return impl().findWindow(id); }
      [[nodiscard]] WindowInfo *findWindow(WindowHandle handle) { return impl().findWindow(handle); }
      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const { return impl().findWindow(id); }
      [[nodiscard]] const WindowInfo *findWindow(WindowHandle handle) const { return impl().findWindow(handle); }

      template <typename... TComponents>
      [[nodiscard]] std::expected<Entity, Error> spawn(TComponents &&...components) {
         return impl().spawn(std::forward<TComponents>(components)...);
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> addComponent(Entity entity, T component) {
         return impl().template addComponent<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<void, Error> setComponent(Entity entity, T component) {
         return impl().template setComponent<T>(entity, std::move(component));
      }

      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> getComponent(Entity entity) const {
         return impl().template getComponent<T>(entity);
      }

      [[nodiscard]] std::expected<void, Error> destroy(Entity entity) { return impl().destroy(entity); }
      [[nodiscard]] std::expected<void, Error> setTransform(Entity entity, Transform transform) {
         return impl().setTransform(std::move(entity), std::move(transform));
      }
      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Entity entity) const {
         return impl().getTransform(std::move(entity));
      }
      [[nodiscard]] std::expected<void, Error> setWindowCamera(WindowHandle window, Entity camera) {
         return impl().setWindowCamera(window, camera);
      }
      [[nodiscard]] std::expected<void, Error> setWindowCamera(std::string_view window_id, Entity camera) {
         return impl().setWindowCamera(window_id, camera);
      }
      [[nodiscard]] std::expected<void, Error> clearWindowCamera(WindowHandle window) {
         return impl().clearWindowCamera(window);
      }
      [[nodiscard]] std::expected<void, Error> clearWindowCamera(std::string_view window_id) {
         return impl().clearWindowCamera(window_id);
      }
      [[nodiscard]] std::optional<Entity> windowCamera(WindowHandle window) const {
         return impl().windowCamera(window);
      }
      [[nodiscard]] std::optional<Entity> windowCamera(std::string_view window_id) const {
         return impl().windowCamera(window_id);
      }
      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) { return impl().setActiveCamera(camera); }
      [[nodiscard]] std::optional<Entity> activeCamera() const { return impl().activeCamera(); }
      void setSceneLoader(std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         impl().setSceneLoader(std::move(loader));
      }
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &path) {
         return impl().loadScene(path);
      }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::World;

      World(Impl &implementation, ECS &ecs)
         : impl_{std::addressof(implementation)}, ecs_{std::addressof(ecs)}, input_{impl().input()} {}

      [[nodiscard]] Impl &impl() { return *impl_; }
      [[nodiscard]] const Impl &impl() const { return *impl_; }

      std::unique_ptr<Impl> owned_{};
      Impl *impl_{nullptr};
      ECS *ecs_{nullptr};
      InputState input_;

      template <typename... TSystems> friend class Engine;
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

   template <typename T> concept ApplicationNameLike = requires(T value) {
      { value.value } -> std::same_as<std::string &>;
   }; ///< Contract for application-name options.

   template <typename T> concept MaxFramesLike = requires(T value) {
      { value.value } -> std::same_as<FrameCount &>;
   }; ///< Contract for frame-cap options.

   template <typename T> concept FrameContextLike = requires(T value) {
      { value.frame_index } -> std::same_as<FrameCount &>;
      { value.delta_time } -> std::same_as<DeltaTime &>;
   }; ///< Contract for per-frame timing contexts.

   template <typename T> concept FrameStatusLike =
      std::same_as<std::remove_cvref_t<T>, FrameStatus>; ///< Contract for frame loop status values.

   template <typename T> concept EngineConfigLike = requires(T value) {
      { value.application_name } -> std::same_as<std::string &>;
      { value.max_frames } -> std::same_as<FrameCount &>;
   }; ///< Contract for compact engine config structs.

   template <typename T, typename... TSystems> concept UserSystemsLike = requires(T value) {
      { value.value } -> std::same_as<std::tuple<TSystems...> &>;
   }; ///< Contract for user-system bundles.

   template <typename T> concept WorldLike =
      requires(T world, Entity entity, WindowHandle window, std::string_view id, Transform transform,
               std::filesystem::path path,
               std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         { world.ecs() } -> std::same_as<ECS &>;
         { world.input() } -> std::same_as<InputState &>;
         { world.windows() } -> std::same_as<Vector<WindowInfo> &>;
         { world.findWindow(id) } -> std::same_as<WindowInfo *>;
         { world.findWindow(window) } -> std::same_as<WindowInfo *>;
         { world.spawn(transform) } -> std::same_as<std::expected<Entity, Error>>;
         { world.addComponent(entity, transform) } -> std::same_as<std::expected<void, Error>>;
         { world.setComponent(entity, transform) } -> std::same_as<std::expected<void, Error>>;
         { world.template getComponent<Transform>(entity) } ->
            std::same_as<std::expected<std::optional<Transform>, Error>>;
         { world.destroy(entity) } -> std::same_as<std::expected<void, Error>>;
         { world.setTransform(entity, transform) } -> std::same_as<std::expected<void, Error>>;
         { world.getTransform(entity) } -> std::same_as<std::expected<std::optional<Transform>, Error>>;
         { world.setWindowCamera(window, entity) } -> std::same_as<std::expected<void, Error>>;
         { world.setWindowCamera(id, entity) } -> std::same_as<std::expected<void, Error>>;
         { world.clearWindowCamera(window) } -> std::same_as<std::expected<void, Error>>;
         { world.clearWindowCamera(id) } -> std::same_as<std::expected<void, Error>>;
         { world.windowCamera(window) } -> std::same_as<std::optional<Entity>>;
         { world.windowCamera(id) } -> std::same_as<std::optional<Entity>>;
         { world.setActiveCamera(entity) } -> std::same_as<std::expected<void, Error>>;
         { world.activeCamera() } -> std::same_as<std::optional<Entity>>;
         world.setSceneLoader(loader);
         { world.loadScene(path) } -> std::same_as<std::expected<SceneHandle, Error>>;
      }; ///< Contract for the public world facade.

   template <typename... TSystems> concept MakeUserSystemsFunctionLike = requires(TSystems... systems) {
      { makeUserSystems(systems...) } -> std::same_as<UserSystems<std::remove_cvref_t<TSystems>...>>;
   }; ///< Contract for makeUserSystems(...).

   static_assert(ApplicationNameLike<ApplicationName>);
   static_assert(EngineConfigLike<EngineConfig>);
   static_assert(FrameContextLike<FrameContext>);
   static_assert(FrameStatusLike<FrameStatus>);
   static_assert(MakeUserSystemsFunctionLike<>);
   static_assert(MaxFramesLike<MaxFrames>);
   static_assert(UserSystemsLike<UserSystems<>>);
   static_assert(WorldLike<World>);

} // namespace vve
