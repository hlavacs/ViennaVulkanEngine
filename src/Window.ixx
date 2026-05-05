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
      InputState() = default;
      InputState(const InputState &) = default;
      InputState(InputState &&) noexcept = default;
      InputState &operator=(const InputState &) = default;
      InputState &operator=(InputState &&) noexcept = default;

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
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;

      Impl impl_{};
   }; ///< Facade input snapshot.

   using WindowDesc      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc;      ///< Facade window descriptor.
   using WindowFrameData = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData; ///< Per-frame window snapshot.
   using WindowInfo      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowInfo;      ///< Runtime window state.
   using Windows         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;         ///< Startup window collection.

} // namespace vve
