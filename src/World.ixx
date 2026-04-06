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
      void pressKey(InputState &, std::int32_t);
      void releaseKey(InputState &, std::int32_t);
      [[nodiscard]] const InputState &emptyInputState();
   } // namespace detail

   /// @brief Runtime-visible description of an engine window.
   struct WindowInfo {
      Handle handle{};				///< Opaque handle of the runtime window.
      std::string id{};				///< Stable string id of the window.
      std::string title{};			///< Human-readable window title.
      std::uint32_t width{0};		///< Current window width in pixels.
      std::uint32_t height{0};	///< Current window height in pixels.
      bool focused{false};			///< Whether the window currently has focus.
      bool minimized{false};		///< Whether the window is minimized.
      bool should_close{false};	///< Whether the window has requested closure.
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

   /**
    * @brief Per-frame input snapshot exposed through `World`.
    *
    * The runtime mutates this structure at frame boundaries. Consumers observe
    * it as read-only state.
    */
   class VVE_API InputState {
   public:
      /// @brief Returns whether a key is currently held down.
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const;
      /// @brief Returns whether a key transitioned to pressed during the current frame.
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const;
      /// @brief Returns whether a key transitioned to released during the current frame.
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const;
      /// @brief Returns the latest known mouse position for a window, if any.
      [[nodiscard]] std::optional<math::Vec2> mousePosition(Handle window) const;
      /// @brief Returns the accumulated mouse movement delta for a window this frame.
      [[nodiscard]] math::Vec2 mouseDelta(Handle window) const;
      /// @brief Returns the accumulated mouse wheel delta for a window this frame.
      [[nodiscard]] math::Vec2 mouseWheelDelta(Handle window) const;

   private:
      std::unordered_set<std::int32_t> keys_down_{};		///< Keys that are currently held down.
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
      friend void detail::pressKey(InputState &, std::int32_t);
      friend void detail::releaseKey(InputState &, std::int32_t);
   };

   namespace detail {

      /// @brief Runtime-only bridge used to expose window, input, and scene-loading services to `World`.
      struct WorldRuntimeAccess {
         std::vector<WindowInfo>::const_iterator windows_begin{};	///< Begin iterator for the runtime window cache.
         std::vector<WindowInfo>::const_iterator windows_end{};	///< End iterator for the runtime window cache.
         const InputState *input{nullptr};								///< Pointer to the current input snapshot.
         /// @brief Runtime callback used to request scene loading.
         std::expected<void, Error> (*load_scene)(void *context, const std::filesystem::path &path){nullptr};
         void *load_scene_context{nullptr};	///< Opaque callback context passed back to `load_scene`.
      };

      inline void beginInputFrame(InputState &input) {
         input.keys_pressed_.clear();
         input.keys_released_.clear();
         input.mouse_delta_.clear();
         input.mouse_wheel_delta_.clear();
      }

      inline void setMousePosition(InputState &input, Handle window, math::Vec2 position) {
         input.mouse_positions_[window.value()] = position;
      }

      inline void addMouseDelta(InputState &input, Handle window, math::Vec2 delta) {
         auto [it, inserted] = input.mouse_delta_.try_emplace(window.value(), math::Vec2(math::zero(), math::zero()));
         auto &value = it->second;
         value += delta;
      }

      inline void addMouseWheelDelta(InputState &input, Handle window, math::Vec2 delta) {
         auto [it, inserted] =
             input.mouse_wheel_delta_.try_emplace(window.value(), math::Vec2(math::zero(), math::zero()));
         auto &value = it->second;
         value += delta;
      }

      inline void pressKey(InputState &input, std::int32_t keycode) {
         input.keys_down_.insert(keycode);
         input.keys_pressed_.insert(keycode);
      }

      inline void releaseKey(InputState &input, std::int32_t keycode) {
         input.keys_down_.erase(keycode);
         input.keys_released_.insert(keycode);
      }

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
      /// @brief Creates a world facade backed only by ECS storage.
      explicit WorldFacade(ECS<> &ecs) noexcept;
      /// @brief Creates a world facade with ECS storage and runtime service access.
      explicit WorldFacade(ECS<> &ecs, const detail::WorldRuntimeAccess &runtime_access) noexcept;

      /// @brief Returns mutable access to the underlying ECS facade.
      [[nodiscard]] ECS<> &ecs() noexcept;
      /// @brief Returns read-only access to the underlying ECS facade.
      [[nodiscard]] const ECS<> &ecs() const noexcept; 

      /// @brief Creates a new entity.
      [[nodiscard]] std::expected<Handle, Error> createEntity();
      /// @brief Creates a new object entity. Currently equivalent to `createEntity()`.
      [[nodiscard]] std::expected<Handle, Error> createObject();
      /// @brief Returns whether an entity currently exists.
      [[nodiscard]] std::expected<bool, Error> exists(Handle entity) const;
      /// @brief Destroys an entity and all of its components.
      [[nodiscard]] std::expected<void, Error> destroyEntity(Handle entity);
      /// @brief Destroys an object entity. Currently equivalent to `destroyEntity()`.
      [[nodiscard]] std::expected<void, Error> destroyObject(Handle entity);

      /// @brief Adds a component to an entity through the world facade.
      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> addComponent(Handle entity, TComponent &&component);

      /// @brief Returns a copy of a component if the entity has one.
      template <NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
      getComponent(Handle entity) const;

      /// @brief Replaces or inserts a component on an entity.
      template <NotHandle TComponent>
      [[nodiscard]] std::expected<void, Error> setComponent(Handle entity, TComponent &&component);

      /// @brief Returns whether an entity owns a component of type `TComponent`.
      template <NotHandle TComponent> [[nodiscard]] std::expected<bool, Error> hasComponent(Handle entity) const;

      /// @brief Removes a component of type `TComponent` from an entity.
      template <NotHandle TComponent> [[nodiscard]] std::expected<void, Error> removeComponent(Handle entity);

      /// @brief Creates an entity and attaches all provided components.
      template <NotHandle... TComponents> [[nodiscard]] std::expected<Handle, Error> spawn(TComponents &&...components);

      /// @brief Returns the currently visible runtime window range.
      [[nodiscard]] std::ranges::subrange<std::vector<WindowInfo>::const_iterator> windows() const;
      /// @brief Finds a window by handle.
      [[nodiscard]] std::optional<WindowInfo> findWindow(Handle window) const;
      /// @brief Finds a window by its stable string id.
      [[nodiscard]] std::optional<WindowInfo> findWindow(std::string_view window_id) const;
      /// @brief Returns the current frame's input snapshot.
      [[nodiscard]] const InputState &input() const;
      /// @brief Requests scene loading through the runtime scene-loading seam.
      [[nodiscard]] std::expected<void, Error> loadScene(const std::filesystem::path &path);

      /// @brief Returns an entity transform component if present.
      [[nodiscard]] std::expected<std::optional<Transform>, Error> getTransform(Handle entity) const;
      /// @brief Replaces an entity transform component.
      [[nodiscard]] std::expected<void, Error> setTransform(Handle entity, const Transform &transform);
      /// @brief Adds `offset` to an entity transform's translation.
      [[nodiscard]] std::expected<void, Error> translate(Handle entity, const math::Vec3 &offset);
      /// @brief Premultiplies an entity transform by `rotation`.
      [[nodiscard]] std::expected<void, Error> rotate(Handle entity, const math::Quat &rotation);
      /// @brief Replaces an entity transform scale.
      [[nodiscard]] std::expected<void, Error> setScale(Handle entity, const math::Vec3 &scale);

      /**
       * @brief Mutates a component by value and writes the result back.
       *
       * The mutator operates on a temporary component copy. The updated value
       * is committed through `setComponent()` if the component exists.
       */
      template <NotHandle TComponent, typename TMutator>
         requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
      [[nodiscard]] std::expected<void, Error> modifyComponent(Handle entity, TMutator &&mutator);

   private:
      TImplementation implementation_;
   };

   /// @brief Default world facade alias for the selected engine namespace.
   using World = WorldFacade<detail::DefaultWorldImplementation>;

   template <typename TImplementation> WorldFacade<TImplementation>::WorldFacade(ECS<> &ecs) noexcept : implementation_(ecs) {}

   template <typename TImplementation>
   WorldFacade<TImplementation>::WorldFacade(ECS<> &ecs, const detail::WorldRuntimeAccess &runtime_access) noexcept
       : implementation_(ecs, runtime_access) {}

   inline bool InputState::isKeyDown(std::int32_t keycode) const { return keys_down_.contains(keycode); }

   inline bool InputState::wasKeyPressed(std::int32_t keycode) const { return keys_pressed_.contains(keycode); }

   inline bool InputState::wasKeyReleased(std::int32_t keycode) const { return keys_released_.contains(keycode); }

   inline std::optional<math::Vec2> InputState::mousePosition(Handle window) const {
      const auto it = mouse_positions_.find(window.value());
      if (it == mouse_positions_.end()) {
         return std::nullopt;
      }

      return it->second;
   }

   inline math::Vec2 InputState::mouseDelta(Handle window) const {
      const auto it = mouse_delta_.find(window.value());
      return it == mouse_delta_.end() ? math::Vec2(math::zero(), math::zero()) : it->second;
   }

   inline math::Vec2 InputState::mouseWheelDelta(Handle window) const {
      const auto it = mouse_wheel_delta_.find(window.value());
      return it == mouse_wheel_delta_.end() ? math::Vec2(math::zero(), math::zero()) : it->second;
   }

   template <typename TImplementation> inline ECS<> &WorldFacade<TImplementation>::ecs() noexcept {
      return implementation_.ecs();
   }

   template <typename TImplementation> inline const ECS<> &WorldFacade<TImplementation>::ecs() const noexcept {
      return implementation_.ecs();
   }

   template <typename TImplementation>
   inline std::expected<Handle, Error> WorldFacade<TImplementation>::createEntity() {
      return implementation_.createEntity();
   }

   template <typename TImplementation>
   inline std::expected<Handle, Error> WorldFacade<TImplementation>::createObject() {
      return implementation_.createObject();
   }

   template <typename TImplementation>
   inline std::expected<bool, Error> WorldFacade<TImplementation>::exists(Handle entity) const {
      return implementation_.exists(entity);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::destroyEntity(Handle entity) {
      return implementation_.destroyEntity(entity);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::destroyObject(Handle entity) {
      return implementation_.destroyObject(entity);
   }

   template <typename TImplementation>
   inline std::ranges::subrange<std::vector<WindowInfo>::const_iterator> WorldFacade<TImplementation>::windows() const {
      return implementation_.windows();
   }

   template <typename TImplementation> inline std::optional<WindowInfo> WorldFacade<TImplementation>::findWindow(Handle window) const {
      return implementation_.findWindow(window);
   }

   template <typename TImplementation>
   inline std::optional<WindowInfo> WorldFacade<TImplementation>::findWindow(std::string_view window_id) const {
      return implementation_.findWindow(window_id);
   }

   template <typename TImplementation> inline const InputState &WorldFacade<TImplementation>::input() const {
      return implementation_.input();
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::loadScene(const std::filesystem::path &path) {
      return implementation_.loadScene(path);
   }

   template <typename TImplementation>
   inline std::expected<std::optional<Transform>, Error> WorldFacade<TImplementation>::getTransform(Handle entity) const {
      return implementation_.getTransform(entity);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setTransform(Handle entity, const Transform &transform) {
      return implementation_.setTransform(entity, transform);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::translate(Handle entity, const math::Vec3 &offset) {
      return implementation_.translate(entity, offset);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::rotate(Handle entity, const math::Quat &rotation) {
      return implementation_.rotate(entity, rotation);
   }

   template <typename TImplementation>
   inline std::expected<void, Error> WorldFacade<TImplementation>::setScale(Handle entity, const math::Vec3 &scale) {
      return implementation_.setScale(entity, scale);
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::addComponent(Handle entity,
                                                                                       TComponent &&component) {
      return implementation_.addComponent(entity, std::forward<TComponent>(component));
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, Error>
   WorldFacade<TImplementation>::getComponent(Handle entity) const {
      return implementation_.template getComponent<TComponent>(entity);
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::setComponent(Handle entity,
                                                                                       TComponent &&component) {
      return implementation_.setComponent(entity, std::forward<TComponent>(component));
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<bool, Error> WorldFacade<TImplementation>::hasComponent(Handle entity) const {
      return implementation_.template hasComponent<TComponent>(entity);
   }

   template <typename TImplementation>
   template <NotHandle TComponent>
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::removeComponent(Handle entity) {
      return implementation_.template removeComponent<TComponent>(entity);
   }

   template <typename TImplementation>
   template <NotHandle... TComponents>
   [[nodiscard]] std::expected<Handle, Error> WorldFacade<TImplementation>::spawn(TComponents &&...components) {
      return implementation_.spawn(std::forward<TComponents>(components)...);
   }

   template <typename TImplementation>
   template <NotHandle TComponent, typename TMutator>
      requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
   [[nodiscard]] std::expected<void, Error> WorldFacade<TImplementation>::modifyComponent(Handle entity,
                                                                                          TMutator &&mutator) {
      return implementation_.template modifyComponent<TComponent>(entity, std::forward<TMutator>(mutator));
   }

} // namespace vve
