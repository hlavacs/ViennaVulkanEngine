export module VEEngine.V4:World;
import std;
export import :Types;

/// @file
/// @brief Example-facing world, input, and window data for the educational v4 runtime.

export namespace vve::v4 {

   /// @brief Human-readable application name used by the platform layer.
   struct ApplicationName {
      std::string value{"v4"}; ///< Name shown in diagnostics and default window titles.
   };

   /// @brief Optional frame cap; zero means the engine runs until a window asks to close.
   struct MaxFrames {
      FrameCount value{}; ///< Maximum number of step() calls before the engine stops.
   };

   /// @brief Window creation descriptor kept deliberately close to v3's public shape.
   struct WindowDesc {
      std::string id{"main"};       ///< Stable application-local window id.
      std::string title{"VVE v4"};  ///< Platform window title.
      PixelExtent extent{.width = 960, .height = 540}; ///< Initial pixel dimensions.
      std::optional<int> x{};       ///< Optional initial screen x coordinate.
      std::optional<int> y{};       ///< Optional initial screen y coordinate.
      RendererId renderer_id{};     ///< Renderer id selected for this window.
      bool resizable{true};         ///< Enables platform resizing.
      bool visible{true};           ///< Shows the window after creation.
   };

   /// @brief Collection wrapper for all windows created during engine init().
   struct Windows {
      Vector<WindowDesc> value{WindowDesc{}}; ///< Startup windows; defaults to one main window.
   };

   /// @brief Runtime window state exposed through World.
   struct WindowInfo {
      Handle handle{};             ///< 64-bit runtime window handle.
      std::string id{};            ///< Stable id copied from WindowDesc.
      std::string title{};         ///< Current platform title.
      PixelExtent extent{};        ///< Current pixel dimensions.
      RendererId renderer_id{};    ///< Renderer id selected for this window.
      bool focused{false};         ///< True while the window has keyboard focus.
      bool minimized{false};       ///< True while the platform reports a minimized window.
      bool should_close{false};    ///< True after a close request.
   };

   /// @brief Per-step timing and frame index passed to user systems.
   struct FrameContext {
      FrameCount frame_index{};       ///< Zero-based frame index.
      DeltaTime delta_time{};         ///< Elapsed wall-clock time since the previous frame.
   };

   /// @brief Snapshot passed to user systems that want window data for the current frame.
   struct WindowFrameData {
      Vector<WindowInfo> windows{}; ///< Window states after event polling.
   };

   /// @brief Keyboard and mouse snapshot; held keys are independent of OS key-repeat speed.
   class InputState {
   public:
      /// @brief Starts a new input frame while preserving held-key state.
      void beginFrame() {
         keys_pressed_.clear();
         keys_released_.clear();
         mouse_delta_.clear();
         mouse_wheel_delta_.clear();
      }

      /// @brief Records a key as currently held without generating a fresh press edge.
      void holdKey(std::int32_t keycode) { keys_down_.insert(keycode); }

      /// @brief Records a key-down edge and held state.
      void pressKey(std::int32_t keycode) {
         if (!keys_down_.contains(keycode)) { keys_pressed_.insert(keycode); }
         keys_down_.insert(keycode);
      }

      /// @brief Records a key-up edge and clears held state.
      void releaseKey(std::int32_t keycode) {
         keys_down_.erase(keycode);
         keys_released_.insert(keycode);
      }

      /// @brief Stores the latest mouse position for one window.
      void setMousePosition(Handle window, Vec2 position) { mouse_position_[window] = position; }

      /// @brief Accumulates mouse movement for the current frame.
      void addMouseDelta(Handle window, Vec2 delta) {
         const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Accumulates mouse-wheel movement for the current frame.
      void addMouseWheelDelta(Handle window, Vec2 delta) {
         const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Returns whether a key is currently held down.
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return keys_down_.contains(keycode); }

      /// @brief Returns whether a key was pressed during the current frame.
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const { return keys_pressed_.contains(keycode); }

      /// @brief Returns whether a key was released during the current frame.
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const { return keys_released_.contains(keycode); }

      /// @brief Returns the latest mouse position for a window, if any motion event was seen.
      [[nodiscard]] std::optional<Vec2> mousePosition(Handle window) const {
         const auto it = mouse_position_.find(window);
         return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
      }

      /// @brief Returns accumulated mouse delta for a window in the current frame.
      [[nodiscard]] Vec2 mouseDelta(Handle window) const {
         const auto it = mouse_delta_.find(window);
         return it == mouse_delta_.end() ? Vec2{} : it->second;
      }

      /// @brief Returns accumulated mouse-wheel delta for a window in the current frame.
      [[nodiscard]] Vec2 mouseWheelDelta(Handle window) const {
         const auto it = mouse_wheel_delta_.find(window);
         return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
      }

   private:
      std::set<std::int32_t> keys_down_{};             ///< Keys currently held down.
      std::set<std::int32_t> keys_pressed_{};          ///< Keys pressed this frame.
      std::set<std::int32_t> keys_released_{};         ///< Keys released this frame.
      std::map<Handle, Vec2> mouse_position_{};        ///< Last mouse position by window.
      std::map<Handle, Vec2> mouse_delta_{};           ///< Frame-local mouse delta by window.
      std::map<Handle, Vec2> mouse_wheel_delta_{};     ///< Frame-local wheel delta by window.
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
      void setSceneLoader(std::function<std::expected<Handle, Error>(const std::filesystem::path &)> loader) {
         scene_loader_ = std::move(loader);
      }

      /// @brief Installs a read-only object-catalog provider for examples and diagnostics.
      void setCatalogProvider(std::function<const ObjectCatalog *()> provider) {
         catalog_provider_ = std::move(provider);
      }

      /// @brief Imports a scene through the runtime loader and returns the scene handle.
      [[nodiscard]] std::expected<Handle, Error> loadScene(const std::filesystem::path &path) {
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
      std::function<std::expected<Handle, Error>(const std::filesystem::path &)> scene_loader_{};
      std::function<const ObjectCatalog *()> catalog_provider_{}; ///< Runtime catalog access hook.
   };

} // namespace vve::v4
