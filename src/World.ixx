module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine:World;
import std;
import :ECS;
import :Error;
import :Handle;
import :Math;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

/**
 * @file
 * @brief Game-facing world facade over ECS storage and runtime services.
 *
 * `World` provides the narrow API that gameplay-facing code uses to create
 * entities, attach components, read transient input, inspect windows, and
 * trigger scene loading without depending on lower-level engine internals.
 */
export namespace vve {

   class InputState;

   namespace detail {
      struct WorldRuntimeAccess;
      void beginInputFrame(InputState &);
      void setMousePosition(InputState &, Handle, math::Vec2);
      void addMouseDelta(InputState &, Handle, math::Vec2);
      void addMouseWheelDelta(InputState &, Handle, math::Vec2);
      void holdKey(InputState &, std::int32_t);
      void pressKey(InputState &, std::int32_t);
      void releaseKey(InputState &, std::int32_t);
      [[nodiscard]] const InputState &emptyInputState();
   } // namespace detail

   /// @brief Runtime-visible description of an engine window.
   struct WindowInfo {
      Handle handle{};          ///< Opaque handle of the runtime window.
      std::string id{};         ///< Stable string id of the window.
      std::string title{};      ///< Human-readable window title.
      std::uint32_t width{0};   ///< Current window width in pixels.
      std::uint32_t height{0};  ///< Current window height in pixels.
      std::string renderer_id{}; ///< Requested renderer identifier for this window.
      bool focused{false};      ///< Whether the window currently has focus.
      bool minimized{false};    ///< Whether the window is minimized.
      bool should_close{false}; ///< Whether the window has requested closure.
   };

   /// @brief Standard transform component used by world helper methods.
   struct Transform {
      /// @brief World-space translation.
      math::Vec3 translation{math::zeroVec3()};
      /// @brief World-space orientation.
      math::Quat rotation{math::identityQuat()};
      /// @brief Non-uniform scale.
      math::Vec3 scale{math::one(), math::one(), math::one()};
   };

   /// @brief Public camera description used by game code to drive rendering.
   struct Camera {
      math::Vec3 position{math::Vec3(math::zero(), static_cast<math::Scalar>(1.5),
                                     static_cast<math::Scalar>(6.0))}; ///< World-space camera position.
      math::Mat4 view_transform{math::translate(
          math::identityMat4(),
          math::Vec3(math::zero(), static_cast<math::Scalar>(-1.5),
                     static_cast<math::Scalar>(-6.0)))}; ///< World-to-view transform.
      math::Scalar vertical_fov_radians{static_cast<math::Scalar>(1.0471975511965976)}; ///< Vertical field of view.
      math::Scalar near_plane{static_cast<math::Scalar>(0.1)}; ///< Near clip plane.
      math::Scalar far_plane{static_cast<math::Scalar>(10000.0)}; ///< Far clip plane.

      /// @brief Builds a camera from an eye position and target point.
      [[nodiscard]] static Camera lookAt(const math::Vec3 &position, const math::Vec3 &target,
                                         const math::Vec3 &up = math::Vec3(math::zero(), math::one(), math::zero()),
                                         math::Scalar vertical_fov_radians =
                                             static_cast<math::Scalar>(1.0471975511965976),
                                         math::Scalar near_plane = static_cast<math::Scalar>(0.1),
                                         math::Scalar far_plane = static_cast<math::Scalar>(10000.0)) {
         Camera camera{};
         camera.position = position;
         camera.view_transform = math::lookAt(position, target, up);
         camera.vertical_fov_radians = vertical_fov_radians;
         camera.near_plane = near_plane;
         camera.far_plane = far_plane;
         return camera;
      }
   };

   /// @brief Camera component that can be attached to an entity and selected as the active view.
   struct CameraComponent {
      Camera camera{};       ///< Camera data stored on the entity.
      std::string window_id{}; ///< Optional future per-window routing id; empty means the runtime-wide camera.
   };

   /**
    * @brief Per-frame input snapshot exposed through `World`.
    *
    * The runtime mutates this structure at frame boundaries. Consumers observe
    * it as read-only state.
    */
   class VVE_API InputState {
   public:
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const;
      [[nodiscard]] std::optional<math::Vec2> mousePosition(Handle window) const;
      [[nodiscard]] math::Vec2 mouseDelta(Handle window) const;
      [[nodiscard]] math::Vec2 mouseWheelDelta(Handle window) const;

   private:
      std::unordered_set<std::int32_t> keys_down_{};    ///< Keys that are currently held down.
      std::unordered_set<std::int32_t> keys_pressed_{}; ///< Keys that transitioned to pressed during the current frame.
      /// @brief Keys that transitioned to released during the current frame.
      std::unordered_set<std::int32_t> keys_released_{};
      /// @brief Latest known mouse positions per window handle value.
      std::unordered_map<Handle::value_type, math::Vec2> mouse_positions_{};
      /// @brief Accumulated mouse movement delta per window handle value for the current frame.
      std::unordered_map<Handle::value_type, math::Vec2> mouse_delta_{};
      /// @brief Accumulated mouse wheel delta per window handle value for the current frame.
      std::unordered_map<Handle::value_type, math::Vec2> mouse_wheel_delta_{};

      friend struct detail::WorldRuntimeAccess;
      friend void detail::beginInputFrame(InputState &);
      friend void detail::setMousePosition(InputState &, Handle, math::Vec2);
      friend void detail::addMouseDelta(InputState &, Handle, math::Vec2);
      friend void detail::addMouseWheelDelta(InputState &, Handle, math::Vec2);
      friend void detail::holdKey(InputState &, std::int32_t);
      friend void detail::pressKey(InputState &, std::int32_t);
      friend void detail::releaseKey(InputState &, std::int32_t);
   };

   namespace detail {

      /// @brief Runtime-only bridge used to expose window, input, and scene-loading services to `World`.
      struct WorldRuntimeAccess {
         std::vector<WindowInfo>::const_iterator windows_begin{}; ///< Begin iterator for the runtime window cache.
         std::vector<WindowInfo>::const_iterator windows_end{};   ///< End iterator for the runtime window cache.
         const InputState *input{nullptr};                        ///< Pointer to the current input snapshot.
         /// @brief Runtime callback used to request scene loading.
         std::expected<void, Error> (*load_scene)(void *context, const std::filesystem::path &path){nullptr};
         void *load_scene_context{nullptr};                       ///< Opaque callback context passed back to `load_scene`.
         /// @brief Runtime callback used to activate an already imported scene.
         std::expected<void, Error> (*load_imported_scene)(void *context, const void *imported_scene){nullptr};
         void *load_imported_scene_context{nullptr}; ///< Opaque callback context passed back to `load_imported_scene`.
         /// @brief Runtime callback used to update the active render camera.
         std::expected<void, Error> (*set_camera)(void *context, const Camera &camera){nullptr};
         void *set_camera_context{nullptr};                       ///< Opaque callback context passed back to `set_camera`.
         /// @brief Runtime callback used to update the active render camera for one window id.
         std::expected<void, Error> (*set_window_camera)(void *context, std::string_view window_id,
                                                         const Camera &camera){nullptr};
         void *set_window_camera_context{nullptr}; ///< Opaque callback context passed back to `set_window_camera`.
      };

      /**
       * @brief Clears per-frame transient input state before new events are applied.
       * @param input Input snapshot mutated for the next frame.
       */
      inline void beginInputFrame(InputState &input) {
         input.keys_down_.clear();
         input.keys_pressed_.clear();
         input.keys_released_.clear();
         input.mouse_delta_.clear();
         input.mouse_wheel_delta_.clear();
      }

      /**
       * @brief Stores the latest mouse position for a window.
       * @param input Input snapshot receiving the update.
       * @param window Window handle associated with the mouse event.
       * @param position Latest mouse position in window coordinates.
       */
      inline void setMousePosition(InputState &input, Handle window, math::Vec2 position) {
         input.mouse_positions_[window.value()] = position;
      }

      /**
       * @brief Accumulates mouse movement delta for a window during the current frame.
       * @param input Input snapshot receiving the update.
       * @param window Window handle associated with the mouse event.
       * @param delta Mouse movement delta to accumulate.
       */
      inline void addMouseDelta(InputState &input, Handle window, math::Vec2 delta) {
         auto [it, inserted] = input.mouse_delta_.try_emplace(window.value(), math::Vec2(math::zero(), math::zero()));
         auto &value = it->second;
         value += delta;
      }

      /**
       * @brief Accumulates mouse wheel delta for a window during the current frame.
       * @param input Input snapshot receiving the update.
       * @param window Window handle associated with the mouse-wheel event.
       * @param delta Mouse wheel delta to accumulate.
       */
      inline void addMouseWheelDelta(InputState &input, Handle window, math::Vec2 delta) {
         auto [it, inserted] =
             input.mouse_wheel_delta_.try_emplace(window.value(), math::Vec2(math::zero(), math::zero()));
         auto &value = it->second;
         value += delta;
      }

      /**
       * @brief Marks a key as held without adding a pressed-this-frame edge.
       * @param input Input snapshot receiving the held-key update.
       * @param keycode Platform keycode that is currently down.
       */
      inline void holdKey(InputState &input, std::int32_t keycode) {
         input.keys_down_.insert(keycode);
      }

      /**
       * @brief Marks a key as pressed in the current frame snapshot.
       * @param input Input snapshot receiving the update.
       * @param keycode Platform keycode that became pressed.
       */
      inline void pressKey(InputState &input, std::int32_t keycode) {
         input.keys_down_.insert(keycode);
         input.keys_pressed_.insert(keycode);
      }

      /**
       * @brief Marks a key as released in the current frame snapshot.
       * @param input Input snapshot receiving the update.
       * @param keycode Platform keycode that became released.
       */
      inline void releaseKey(InputState &input, std::int32_t keycode) {
         input.keys_down_.erase(keycode);
         input.keys_released_.insert(keycode);
      }

      /**
       * @brief Returns a shared empty input snapshot used when no runtime is bound.
       * @return Immutable empty input snapshot.
       */
      [[nodiscard]] inline const InputState &emptyInputState() {
         static const InputState input{};
         return input;
      }

   } // namespace detail

} // namespace vve

#include "versions/v3/World.ixx"

export namespace vve {

   namespace detail {

      using DefaultWorldImplementation = vve::VVE_DEFAULT_ENGINE_NAMESPACE::WorldImplementation;

   } // namespace detail

   /**
    * @brief Game-facing facade that combines ECS access with runtime services.
    *
    * `World` intentionally exposes engine concepts rather than raw runtime
    * implementation details. Entity storage still lives in ECS; `World`
    * provides the higher-level convenience boundary used by systems.
    */
   template <typename TImplementation> class VVE_API WorldFacade {
   public:
      explicit WorldFacade(ECS<> &ecs) noexcept;
      explicit WorldFacade(ECS<> &ecs, const detail::WorldRuntimeAccess &runtime_access) noexcept;

      [[nodiscard]] ECS<> &ecs() noexcept;
      [[nodiscard]] const ECS<> &ecs() const noexcept; 

      [[nodiscard]] std::expected<Handle, Error> createEntity();
      [[nodiscard]] std::expected<Handle, Error> createObject();
      [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const;
      [[nodiscard]] std::expected<void, Error> destroyEntity(Handle entity);
      [[nodiscard]] std::expected<void, Error> destroyObject(Handle entity);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> addComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
      getComponent(Handle entity) const;

      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> setComponent(Handle entity, TComponent &&component);

      template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> hasComponent(Handle entity) const;

      template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> removeComponent(Handle entity);

      template <NotHandle... TComponents> [[nodiscard]] std::expected<Handle, Error> spawn(TComponents &&...components);

      [[nodiscard]] std::ranges::subrange<std::vector<WindowInfo>::const_iterator> windows() const;
      [[nodiscard]] std::optional<WindowInfo> findWindow(Handle window) const;
      [[nodiscard]] std::optional<WindowInfo> findWindow(std::string_view window_id) const;
      [[nodiscard]] const InputState &input() const;
      [[nodiscard]] std::expected<void, Error> loadScene(const std::filesystem::path &path);
      template <typename TImportedScene>
      [[nodiscard]] std::expected<void, Error> loadImportedScene(const TImportedScene &scene);
      [[nodiscard]] std::expected<void, Error> setCamera(const Camera &camera);
      [[nodiscard]] std::expected<void, Error> setCamera(std::string_view window_id, const Camera &camera);
      [[nodiscard]] std::expected<void, Error> setActiveCamera(Handle camera_entity);

      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Handle entity) const;
      [[nodiscard]] std::expected<void, Error> setTransform(Handle entity, const Transform &transform);
      [[nodiscard]] std::expected<void, Error> translate(Handle entity, const math::Vec3 &offset);
      [[nodiscard]] std::expected<void, Error> rotate(Handle entity, const math::Quat &rotation);
      [[nodiscard]] std::expected<void, Error> setScale(Handle entity, const math::Vec3 &scale);

      template <NotHandle TComponent, typename TMutator>
         requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
      [[nodiscard]] std::expected<void, Error> modifyComponent(Handle entity, TMutator &&mutator);

   private:
      TImplementation implementation_;
   };

   /// @brief Default world facade alias for the selected engine namespace.
   using World = WorldFacade<detail::DefaultWorldImplementation>;

   /**
    * @brief Creates a world facade backed only by ECS storage.
    * @param ecs ECS facade exposed through the world boundary.
    */
   template <typename TImplementation> WorldFacade<TImplementation>::WorldFacade(ECS<> &ecs) noexcept : implementation_(ecs) {}

   /**
    * @brief Creates a world facade with ECS storage and runtime service access.
    * @param ecs ECS facade exposed through the world boundary.
    * @param runtime_access Runtime bridge used for windows, input, and scene loading.
    */
   template <typename TImplementation>
   WorldFacade<TImplementation>::WorldFacade(ECS<> &ecs, const detail::WorldRuntimeAccess &runtime_access) noexcept
       : implementation_(ecs, runtime_access) {}

   /// @brief Returns whether a key is currently held down.
   inline bool InputState::isKeyDown(std::int32_t keycode) const { return keys_down_.contains(keycode); }

   /// @brief Returns whether a key transitioned to pressed during the current frame.
   inline bool InputState::wasKeyPressed(std::int32_t keycode) const { return keys_pressed_.contains(keycode); }

   /// @brief Returns whether a key transitioned to released during the current frame.
   inline bool InputState::wasKeyReleased(std::int32_t keycode) const { return keys_released_.contains(keycode); }

   /**
    * @brief Returns the latest known mouse position for a window, if any.
    * @param window Window handle to inspect.
    * @return Mouse position when known for the current frame.
    */
   inline std::optional<math::Vec2> InputState::mousePosition(Handle window) const {
      const auto it = mouse_positions_.find(window.value());
      if (it == mouse_positions_.end()) {
         return std::nullopt;
      }

      return it->second;
   }

   /**
    * @brief Returns the accumulated mouse movement delta for a window this frame.
    * @param window Window handle to inspect.
    * @return Mouse delta accumulated during the current frame.
    */
   inline math::Vec2 InputState::mouseDelta(Handle window) const {
      const auto it = mouse_delta_.find(window.value());
      return it == mouse_delta_.end() ? math::Vec2(math::zero(), math::zero()) : it->second;
   }

   /**
    * @brief Returns the accumulated mouse wheel delta for a window this frame.
    * @param window Window handle to inspect.
    * @return Mouse wheel delta accumulated during the current frame.
    */
   inline math::Vec2 InputState::mouseWheelDelta(Handle window) const {
      const auto it = mouse_wheel_delta_.find(window.value());
      return it == mouse_wheel_delta_.end() ? math::Vec2(math::zero(), math::zero()) : it->second;
   }

   /// @brief Returns mutable access to the underlying ECS facade.
   template <typename TImplementation> inline ECS<> &WorldFacade<TImplementation>::ecs() noexcept {
      return implementation_.ecs();
   }

   /// @brief Returns read-only access to the underlying ECS facade.
   template <typename TImplementation> inline const ECS<> &WorldFacade<TImplementation>::ecs() const noexcept {
      return implementation_.ecs();
   }

   /// @brief Creates a new entity.
   template <typename TImplementation>
   inline std::expected<Handle, Error> WorldFacade<TImplementation>::createEntity() {
      return implementation_.createEntity();
   }

   /// @brief Creates a new object entity. Currently equivalent to `createEntity()`.
   template <typename TImplementation>
   inline std::expected<Handle, Error> WorldFacade<TImplementation>::createObject() {
      return implementation_.createObject();
   }

   /// @brief Returns whether an entity currently exists.
   template <typename TImplementation>
   inline std::expected<bool, Error> WorldFacade<TImplementation>::exists(Handle entity) const {
      return implementation_.exists(entity);
   }

   /// @brief Destroys an entity and all of its components.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::destroyEntity(Handle entity) {
      return implementation_.destroyEntity(entity);
   }

   /// @brief Destroys an object entity. Currently equivalent to `destroyEntity()`.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::destroyObject(Handle entity) {
      return implementation_.destroyObject(entity);
   }

   /// @brief Returns the currently visible runtime window range.
   template <typename TImplementation>
   inline std::ranges::subrange<std::vector<WindowInfo>::const_iterator> WorldFacade<TImplementation>::windows() const {
      return implementation_.windows();
   }

   /// @brief Finds a window by handle.
   template <typename TImplementation> inline std::optional<WindowInfo> WorldFacade<TImplementation>::findWindow(Handle window) const {
      return implementation_.findWindow(window);
   }

   /// @brief Finds a window by its stable string id.
   template <typename TImplementation>
   inline std::optional<WindowInfo> WorldFacade<TImplementation>::findWindow(std::string_view window_id) const {
      return implementation_.findWindow(window_id);
   }

   /// @brief Returns the current frame's input snapshot.
   template <typename TImplementation> inline const InputState &WorldFacade<TImplementation>::input() const {
      return implementation_.input();
   }

   /// @brief Requests scene loading through the runtime scene-loading seam.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::loadScene(const std::filesystem::path &path) {
      return implementation_.loadScene(path);
   }

   /// @brief Activates a scene that was already imported by the selected engine version.
   template <typename TImplementation>
   template <typename TImportedScene>
   inline std::expected<void, Error> WorldFacade<TImplementation>::loadImportedScene(const TImportedScene &scene) {
      return implementation_.loadImportedScene(scene);
   }

   /// @brief Updates the active render camera through the runtime bridge.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setCamera(const Camera &camera) {
      return implementation_.setCamera(camera);
   }

   /// @brief Updates the active render camera for one runtime window id.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setCamera(std::string_view window_id,
                                                                             const Camera &camera) {
      return implementation_.setCamera(window_id, camera);
   }

   /// @brief Selects an entity camera component as the active render camera.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setActiveCamera(Handle camera_entity) {
      const auto camera_result = getComponent<CameraComponent>(camera_entity);
      if (!camera_result) {
         return std::unexpected(camera_result.error());
      }
      if (!camera_result->has_value()) {
         return std::unexpected(Error::invalid_argument);
      }

      const auto &camera_component = camera_result->value();
      if (camera_component.window_id.empty()) {
         return setCamera(camera_component.camera);
      }

      return setCamera(camera_component.window_id, camera_component.camera);
   }

   /// @brief Returns an entity transform component if present.
   template <typename TImplementation>
   inline std::expected<std::optional<Transform>, Error> WorldFacade<TImplementation>::getTransform(Handle entity) const {
      return implementation_.getTransform(entity);
   }

   /// @brief Replaces an entity transform component.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setTransform(Handle entity, const Transform &transform) {
      return implementation_.setTransform(entity, transform);
   }

   /// @brief Adds `offset` to an entity transform's translation.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::translate(Handle entity, const math::Vec3 &offset) {
      return implementation_.translate(entity, offset);
   }

   /// @brief Premultiplies an entity transform by `rotation`.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::rotate(Handle entity, const math::Quat &rotation) {
      return implementation_.rotate(entity, rotation);
   }

   /// @brief Replaces an entity transform scale.
   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setScale(Handle entity, const math::Vec3 &scale) {
      return implementation_.setScale(entity, scale);
   }

   /// @brief Adds a component to an entity through the world facade.
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::addComponent(Handle entity,
                                                                                      TComponent &&component) {
      return implementation_.addComponent(entity, std::forward<TComponent>(component));
   }

   /// @brief Returns a copy of a component if the entity has one.
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
   WorldFacade<TImplementation>::getComponent(Handle entity) const {
      return implementation_.template getComponent<TComponent>(entity);
   }

   /// @brief Replaces or inserts a component on an entity.
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::setComponent(Handle entity,
                                                                                       TComponent &&component) {
      return implementation_.setComponent(entity, std::forward<TComponent>(component));
   }

   /// @brief Returns whether an entity owns a component of type `TComponent`.
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<bool, Error> WorldFacade<TImplementation>::hasComponent(Handle entity) const {
      return implementation_.template hasComponent<TComponent>(entity);
   }

   /// @brief Removes a component of type `TComponent` from an entity.
   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::removeComponent(Handle entity) {
      return implementation_.template removeComponent<TComponent>(entity);
   }

   /// @brief Creates an entity and attaches all provided components.
   template <typename TImplementation>
   template <NotHandle... TComponents>
   [[nodiscard]] std::expected<Handle, Error> WorldFacade<TImplementation>::spawn(TComponents &&...components) {
      return implementation_.spawn(std::forward<TComponents>(components)...);
   }

   /**
    * @brief Mutates a component by value and writes the result back.
    * @tparam TComponent Component type to read, mutate, and write back.
    * @tparam TMutator Callable that mutates the temporary component copy.
    * @param entity Entity handle to mutate.
    * @param mutator Mutation callable applied to the copied component value.
    * @return Empty success result, or an error when the component mutation fails.
    */
   template <typename TImplementation>
   template <NotHandle TComponent, typename TMutator>
      requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::modifyComponent(Handle entity,
                                                                                          TMutator &&mutator) {
      return implementation_.template modifyComponent<TComponent>(entity, std::forward<TMutator>(mutator));
   }

} // namespace vve
