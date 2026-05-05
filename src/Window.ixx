export module VEEngine:Window;
import std;
import VEEngine.V4;
import VEEngine.Types;

/**
 * @file
 * @brief Public window/input contract backed by the selected engine implementation.
 */
export namespace vve {

   class InputState {
   public:
      InputState() : impl_{std::make_shared<Impl>()} {}
      explicit InputState(VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState &implementation)
         : impl_{std::shared_ptr<Impl>(std::addressof(implementation), [](Impl *) {})} {}
      InputState(const InputState &) = default;
      InputState(InputState &&) noexcept = default;
      InputState &operator=(const InputState &) = default;
      InputState &operator=(InputState &&) noexcept = default;

      void beginFrame() { impl_->beginFrame(); }
      void holdKey(std::int32_t keycode) { impl_->holdKey(keycode); }
      void pressKey(std::int32_t keycode) { impl_->pressKey(keycode); }
      void releaseKey(std::int32_t keycode) { impl_->releaseKey(keycode); }
      void setMousePosition(WindowHandle window, Vec2 position) { impl_->setMousePosition(window, position); }
      void addMouseDelta(WindowHandle window, Vec2 delta) { impl_->addMouseDelta(window, delta); }
      void addMouseWheelDelta(WindowHandle window, Vec2 delta) { impl_->addMouseWheelDelta(window, delta); }

      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return impl_->isKeyDown(keycode); }
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const { return impl_->wasKeyPressed(keycode); }
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const { return impl_->wasKeyReleased(keycode); }
      [[nodiscard]] std::optional<Vec2> mousePosition(WindowHandle window) const {
         return impl_->mousePosition(window);
      }
      [[nodiscard]] Vec2 mouseDelta(WindowHandle window) const { return impl_->mouseDelta(window); }
      [[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const { return impl_->mouseWheelDelta(window); }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;

      std::shared_ptr<Impl> impl_{};
   }; ///< Facade input snapshot.

   class Window {
   public:
      explicit Window(const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowInfo &implementation)
         : impl_{std::addressof(implementation)} {}

      [[nodiscard]] WindowHandle handle() const { return impl_->handle; }
      [[nodiscard]] std::string_view id() const { return impl_->id; }
      [[nodiscard]] std::string_view title() const { return impl_->title; }
      [[nodiscard]] PixelExtent extent() const { return impl_->extent; }
      [[nodiscard]] RendererId rendererId() const { return impl_->renderer_id; }
      [[nodiscard]] std::optional<Entity> camera() const { return impl_->camera; }
      [[nodiscard]] bool focused() const { return impl_->focused; }
      [[nodiscard]] bool minimized() const { return impl_->minimized; }
      [[nodiscard]] bool shouldClose() const { return impl_->should_close; }

   private:
      const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowInfo *impl_{};
   }; ///< Read-only facade window view.

} // namespace vve
