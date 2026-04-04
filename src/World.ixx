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

   struct WindowInfo {
      Handle handle{};
      std::string id{};
      std::string title{};
      std::uint32_t width{0};
      std::uint32_t height{0};
      bool focused{false};
      bool minimized{false};
      bool should_close{false};
   };

   struct Transform {
      math::Vec3 translation{math::zeroVec3()};
      math::Quat rotation{math::identityQuat()};
      math::Vec3 scale{math::one(), math::one(), math::one()};
   };

   class VVE_API InputState {
   public:
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const;
      [[nodiscard]] std::optional<math::Vec2> mousePosition(Handle window) const;
      [[nodiscard]] math::Vec2 mouseDelta(Handle window) const;
      [[nodiscard]] math::Vec2 mouseWheelDelta(Handle window) const;

   private:
      std::unordered_set<std::int32_t> keys_down_{};
      std::unordered_set<std::int32_t> keys_pressed_{};
      std::unordered_set<std::int32_t> keys_released_{};
      std::unordered_map<Handle::value_type, math::Vec2> mouse_positions_{};
      std::unordered_map<Handle::value_type, math::Vec2> mouse_delta_{};
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

      struct WorldRuntimeAccess {
         const WindowInfo *windows_begin{nullptr};
         const WindowInfo *windows_end{nullptr};
         const InputState *input{nullptr};
         std::expected<void, Error> (*load_scene)(void *context, const std::filesystem::path &path){nullptr};
         void *load_scene_context{nullptr};
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

      [[nodiscard]] std::ranges::subrange<const WindowInfo *> windows() const;
      [[nodiscard]] std::optional<WindowInfo> findWindow(Handle window) const;
      [[nodiscard]] std::optional<WindowInfo> findWindow(std::string_view window_id) const;
      [[nodiscard]] const InputState &input() const;
      [[nodiscard]] std::expected<void, Error> loadScene(const std::filesystem::path &path);

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
   inline std::ranges::subrange<const WindowInfo *> WorldFacade<TImplementation>::windows() const {
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
