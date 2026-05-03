export module VEEngine:World;
import std;
export import :ECS;
export import :Window;

/**
 * @file
 * @brief World facade, frame status, and user-system option types.
 */
export namespace vve {

   /// @brief Human-readable application name used by the platform layer.
   struct ApplicationName {
      std::string value{"v4"}; ///< Name shown in diagnostics and default window titles.
   };

   /// @brief Optional frame cap; zero means the engine runs until a window asks to close.
   struct MaxFrames {
      FrameCount value{}; ///< Maximum number of step() calls before the engine stops.
   };

   /// @brief Per-step timing and frame index passed to user systems.
   struct FrameContext {
      FrameCount frame_index{}; ///< Zero-based frame index.
      DeltaTime delta_time{};   ///< Elapsed wall-clock time since the previous frame.
   };

   /// @brief User-visible state facade used by examples and systems.
   class World {
   public:
      /// @brief Creates a facade over ECS storage owned by a higher-level runtime.
      explicit World(ECS &ecs) noexcept : ecs_(ecs) {}

      /// @brief Returns the runtime ECS.
      [[nodiscard]] ECS &ecs() { return ecs_; }

      /// @brief Returns the runtime ECS.
      [[nodiscard]] const ECS &ecs() const { return ecs_; }

      /// @brief Returns the current input snapshot.
      [[nodiscard]] InputState &input() { return input_; }

      /// @brief Returns the current input snapshot.
      [[nodiscard]] const InputState &input() const { return input_; }

      /// @brief Returns mutable runtime window states for engine synchronization.
      [[nodiscard]] Vector<WindowInfo> &windows() { return windows_; }

      /// @brief Returns read-only runtime window states.
      [[nodiscard]] const Vector<WindowInfo> &windows() const { return windows_; }

      /// @brief Finds a mutable window by string id.
      [[nodiscard]] WindowInfo *findWindow(std::string_view id) {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Finds a read-only window by string id.
      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Creates an entity and attaches all provided components.
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

      /// @brief Adds one component to an existing entity.
      template <typename T> [[nodiscard]] std::expected<void, Error> addComponent(Entity entity, T component) {
         return ecs_.add(entity, std::move(component));
      }

      /// @brief Inserts or replaces one component on an existing entity.
      template <typename T> [[nodiscard]] std::expected<void, Error> setComponent(Entity entity, T component) {
         return ecs_.put(entity, std::move(component));
      }

      /// @brief Reads an optional component by value.
      template <typename T> [[nodiscard]] std::expected<std::optional<T>, Error> getComponent(Entity entity) const {
         return ecs_.tryGet<T>(entity);
      }

      /// @brief Destroys an entity and its components.
      [[nodiscard]] std::expected<void, Error> destroy(Entity entity) { return ecs_.erase(entity); }

      /// @brief Convenience wrapper for setting the Transform component.
      [[nodiscard]] std::expected<void, Error> setTransform(Entity entity, Transform transform) {
         return setComponent(entity, std::move(transform));
      }

      /// @brief Convenience wrapper for reading the Transform component.
      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Entity entity) const {
         return getComponent<Transform>(entity);
      }

      /// @brief Stores the active camera handle; renderers can interpret it later.
      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         active_camera_ = camera;
         return {};
      }

      /// @brief Returns the current active camera handle when one has been selected.
      [[nodiscard]] std::optional<Entity> activeCamera() const { return active_camera_; }

      /// @brief Installs the runtime scene loader used by loadScene().
      void setSceneLoader(std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> loader) {
         scene_loader_ = std::move(loader);
      }

      /// @brief Installs a read-only object-catalog provider for examples and diagnostics.
      void setCatalogProvider(std::function<const ObjectCatalog *()> provider) {
         catalog_provider_ = std::move(provider);
      }

      /// @brief Imports a scene through the runtime loader and returns the scene handle.
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &path) {
         if (!scene_loader_) { return std::unexpected(Error::missing_object); }
         return scene_loader_(path);
      }

      /// @brief Returns the runtime object catalog when an engine has connected one.
      [[nodiscard]] const ObjectCatalog *objectCatalog() const {
         return catalog_provider_ ? catalog_provider_() : nullptr;
      }

   private:
      ECS &ecs_;                          ///< Non-owning runtime entity/component storage.
      InputState input_{};                ///< Current input snapshot.
      Vector<WindowInfo> windows_{};      ///< Current platform windows.
      std::optional<Entity> active_camera_{}; ///< Optional camera selected by the application.
      /// @brief Runtime callback that imports a scene into the asset catalog.
      std::function<std::expected<SceneHandle, Error>(const std::filesystem::path &)> scene_loader_{};
      std::function<const ObjectCatalog *()> catalog_provider_{}; ///< Runtime catalog access hook.
   };

   /// @brief Backward-compatible compact config; typed options are preferred for new examples.
   struct EngineConfig {
      std::string application_name{"v4"}; ///< Human-readable application name.
      FrameCount max_frames{};            ///< Zero means no frame limit.
   };

   /// @brief Result of one engine frame.
   enum class FrameStatus {
      running,                    ///< Engine can continue stepping.
      stopped,                    ///< Engine stopped because a close request or frame cap was reached.
      continue_running = running, ///< Alias kept close to v3-style wording.
      should_close     = stopped  ///< Alias kept close to v3-style wording.
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

} // namespace vve
