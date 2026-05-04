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
   using World           = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::World;           ///< Facade world type.

   template <typename... TSystems>
   using UserSystems = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystems<TSystems...>; ///< User systems bundle.

   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeUserSystems; ///< Facade user-system bundle factory.

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
