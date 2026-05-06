export module VEEngine:Window;
import std;
import VEEngine.V4;
import VEEngine.Types;
import VEEngine.Vector;

/**
 * @file
 * @brief Public window/input contract backed by the selected engine implementation.
 */
export namespace vve {

   class InputState {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;

   public:
      explicit InputState(Impl &implementation) : impl_{implementation} {}
      InputState(const InputState &) = default;
      InputState(InputState &&) noexcept = default;
      InputState &operator=(const InputState &) = delete;
      InputState &operator=(InputState &&) noexcept = delete;

      void beginFrame() { impl_.beginFrame(); }
      void holdKey(std::int32_t keycode) { impl_.holdKey(keycode); }
      void pressKey(std::int32_t keycode) { impl_.pressKey(keycode); }
      void releaseKey(std::int32_t keycode) { impl_.releaseKey(keycode); }
      void setMousePosition(WindowHandle window, Vec2 position) { impl_.setMousePosition(window, position); }
      void addMouseDelta(WindowHandle window, Vec2 delta) { impl_.addMouseDelta(window, delta); }
      void addMouseWheelDelta(WindowHandle window, Vec2 delta) { impl_.addMouseWheelDelta(window, delta); }

      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return impl_.isKeyDown(keycode); }
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const { return impl_.wasKeyPressed(keycode); }
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const { return impl_.wasKeyReleased(keycode); }
      [[nodiscard]] std::optional<Vec2> mousePosition(WindowHandle window) const {
         return impl_.mousePosition(window);
      }
      [[nodiscard]] Vec2 mouseDelta(WindowHandle window) const { return impl_.mouseDelta(window); }
      [[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const { return impl_.mouseWheelDelta(window); }

   private:
      Impl &impl_;
   }; ///< Facade input snapshot.

   class Window {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Window;

   public:
      explicit Window(Impl &implementation) : impl_{implementation} {}
      Window(const Window &) = default;
      Window(Window &&) noexcept = default;
      Window &operator=(const Window &) = delete;
      Window &operator=(Window &&) noexcept = delete;

      [[nodiscard]] WindowHandle handle() const { return impl_.info().handle; }
      [[nodiscard]] std::string_view id() const { return impl_.info().id; }
      [[nodiscard]] std::string_view title() const { return impl_.info().title; }
      [[nodiscard]] PixelExtent extent() const { return impl_.info().extent; }
      [[nodiscard]] RendererId rendererId() const { return impl_.info().renderer_id; }
      [[nodiscard]] std::optional<Entity> camera() const { return impl_.info().camera; }
      [[nodiscard]] bool focused() const { return impl_.info().focused; }
      [[nodiscard]] bool minimized() const { return impl_.info().minimized; }
      [[nodiscard]] bool shouldClose() const { return impl_.info().should_close; }

   private:
      Impl &impl_;
   }; ///< Read-only facade window view.

   class WindowSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem;

   public:
      explicit WindowSystem(Impl &implementation) : impl_{implementation} {}
      WindowSystem(const WindowSystem &) = default;
      WindowSystem(WindowSystem &&) noexcept = default;
      WindowSystem &operator=(const WindowSystem &) = delete;
      WindowSystem &operator=(WindowSystem &&) noexcept = delete;

      [[nodiscard]] std::string_view name() const noexcept { return impl_.name(); }
      [[nodiscard]] std::size_t windowCount() const { return impl_.windowCount(); }
      [[nodiscard]] Vector<Window> windows() const {
         Vector<Window> result{};
         const auto implementation_windows = impl_.windows();
         result.reserve(implementation_windows.size());
         for (const auto window : implementation_windows) { result.push_back(Window{window.get()}); }
         return result;
      }
      [[nodiscard]] std::optional<Window> findWindow(std::string_view id) const {
         auto *window = impl_.findWindow(id);
         return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
      }
      [[nodiscard]] std::optional<Window> findWindow(WindowHandle handle) const {
         auto *window = impl_.findWindow(handle);
         return window == nullptr ? std::optional<Window>{} : std::optional<Window>{Window{*window}};
      }

   private:
      Impl &impl_;
   }; ///< Public window-system wrapper.

} // namespace vve
