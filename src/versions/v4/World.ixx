export module VEEngine.V4:World;
import std;
export import :ECS;

/// @file
/// @brief v4 world, window/input data, and frame-option implementation types.

export namespace vve::v4 {

   class AssetSystem;
   class GuiSystem;
   class WindowSystem;

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
      WindowHandle handle{};    ///< 64-bit runtime window handle.
      std::string id{};         ///< Stable id copied from WindowDesc.
      std::string title{};      ///< Current platform title.
      PixelExtent extent{};     ///< Current pixel dimensions.
      RendererId renderer_id{}; ///< Renderer id selected for this window.
      std::optional<Entity> camera{}; ///< Camera entity rendered through this window, when selected.
      bool focused{false};      ///< True while the window has keyboard focus.
      bool minimized{false};    ///< True while the platform reports a minimized window.
      bool should_close{false}; ///< True after a close request.
   };

   /// @brief Snapshot passed to user systems that want window data for the current frame.
   struct WindowFrameData {
      Vector<WindowInfo> windows{}; ///< Window states after event polling.
   };

   /// @brief Keyboard and mouse snapshot; held keys are independent of OS key-repeat speed.
   class InputState {
   public:
      void beginFrame() {
         keys_pressed_.clear();
         keys_released_.clear();
         mouse_delta_.clear();
         mouse_wheel_delta_.clear();
      }

      void holdKey(std::int32_t keycode) { keys_down_.insert(normalizeKey(keycode)); }

      void pressKey(std::int32_t keycode) {
         const auto key = normalizeKey(keycode);
         if (!keys_down_.contains(key)) { keys_pressed_.insert(key); }
         keys_down_.insert(key);
      }

      void releaseKey(std::int32_t keycode) {
         const auto key = normalizeKey(keycode);
         keys_down_.erase(key);
         keys_pressed_.erase(key);
         keys_released_.insert(key);
      }

      void setMousePosition(WindowHandle window, Vec2 position) { mouse_position_[window] = position; }

      void addMouseDelta(WindowHandle window, Vec2 delta) {
         const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      void addMouseWheelDelta(WindowHandle window, Vec2 delta) {
         const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
         it->second = math::add(it->second, delta);
      }

      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return keys_down_.contains(normalizeKey(keycode)); }
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const {
         return keys_pressed_.contains(normalizeKey(keycode));
      }
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const {
         return keys_released_.contains(normalizeKey(keycode));
      }

      [[nodiscard]] std::optional<Vec2> mousePosition(WindowHandle window) const {
         const auto it = mouse_position_.find(window);
         return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
      }

      [[nodiscard]] Vec2 mouseDelta(WindowHandle window) const {
         const auto it = mouse_delta_.find(window);
         return it == mouse_delta_.end() ? Vec2{} : it->second;
      }

      [[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const {
         const auto it = mouse_wheel_delta_.find(window);
         return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
      }

   private:
      [[nodiscard]] static std::int32_t normalizeKey(std::int32_t keycode) {
         if (keycode >= static_cast<std::int32_t>('A') && keycode <= static_cast<std::int32_t>('Z')) {
            return keycode - static_cast<std::int32_t>('A') + static_cast<std::int32_t>('a');
         }
         return keycode;
      }

      std::set<std::int32_t> keys_down_{};
      std::set<std::int32_t> keys_pressed_{};
      std::set<std::int32_t> keys_released_{};
      std::map<WindowHandle, Vec2> mouse_position_{};
      std::map<WindowHandle, Vec2> mouse_delta_{};
      std::map<WindowHandle, Vec2> mouse_wheel_delta_{};
   };

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
      [[nodiscard]] InputState &input() { return input_; }
      [[nodiscard]] const InputState &input() const { return input_; }
      [[nodiscard]] Vector<WindowInfo> &windows() { return windows_; }
      [[nodiscard]] const Vector<WindowInfo> &windows() const { return windows_; }

      [[nodiscard]] WindowInfo *findWindow(std::string_view id) {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] WindowInfo *findWindow(WindowHandle handle) {
         const auto it = std::ranges::find_if(windows_, [handle](const WindowInfo &window) {
            return window.handle == handle;
         });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const {
         const auto it = std::ranges::find_if(windows_, [id](const WindowInfo &window) { return window.id == id; });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

      [[nodiscard]] const WindowInfo *findWindow(WindowHandle handle) const {
         const auto it = std::ranges::find_if(windows_, [handle](const WindowInfo &window) {
            return window.handle == handle;
         });
         return it == windows_.end() ? nullptr : std::addressof(*it);
      }

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
         auto *info = findWindow(window);
         if (info == nullptr) { return std::unexpected(Error::invalid_handle); }
         info->camera = camera;
         return {};
      }

      [[nodiscard]] std::expected<void, Error> setWindowCamera(std::string_view window_id, Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         auto *info = findWindow(window_id);
         if (info == nullptr) { return std::unexpected(Error::invalid_handle); }
         info->camera = camera;
         return {};
      }

      [[nodiscard]] std::expected<void, Error> clearWindowCamera(WindowHandle window) {
         auto *info = findWindow(window);
         if (info == nullptr) { return std::unexpected(Error::invalid_handle); }
         info->camera.reset();
         return {};
      }

      [[nodiscard]] std::expected<void, Error> clearWindowCamera(std::string_view window_id) {
         auto *info = findWindow(window_id);
         if (info == nullptr) { return std::unexpected(Error::invalid_handle); }
         info->camera.reset();
         return {};
      }

      [[nodiscard]] std::optional<Entity> windowCamera(WindowHandle window) const {
         const auto *info = findWindow(window);
         return info == nullptr ? std::optional<Entity>{} : info->camera;
      }

      [[nodiscard]] std::optional<Entity> windowCamera(std::string_view window_id) const {
         const auto *info = findWindow(window_id);
         return info == nullptr ? std::optional<Entity>{} : info->camera;
      }

      [[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera) {
         if (!ecs_.exists(camera)) { return std::unexpected(Error::invalid_handle); }
         if (windows_.empty()) { return std::unexpected(Error::missing_object); }
         for (auto &window : windows_) { window.camera = camera; }
         return {};
      }

      [[nodiscard]] std::optional<Entity> activeCamera() const {
         const auto it = std::ranges::find_if(windows_, [](const WindowInfo &window) {
            return window.camera.has_value();
         });
         return it == windows_.end() ? std::optional<Entity>{} : it->camera;
      }

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
      Vector<WindowInfo> windows_{};
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
