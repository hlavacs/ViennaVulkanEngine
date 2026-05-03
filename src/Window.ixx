export module VEEngine:Window;
import std;
export import :Types;

/**
 * @file
 * @brief Window descriptors, runtime window state, and frame-local input snapshots.
 */
export namespace vve {

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
      void setMousePosition(WindowHandle window, math::Vec2 position) { mouse_position_[window] = position; }

      /// @brief Accumulates mouse movement for the current frame.
      void addMouseDelta(WindowHandle window, math::Vec2 delta) {
         const auto [it, _] = mouse_delta_.try_emplace(window, math::Vec2{math::zero(), math::zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Accumulates mouse-wheel movement for the current frame.
      void addMouseWheelDelta(WindowHandle window, math::Vec2 delta) {
         const auto [it, _] = mouse_wheel_delta_.try_emplace(window, math::Vec2{math::zero(), math::zero()});
         it->second = math::add(it->second, delta);
      }

      /// @brief Returns whether a key is currently held down.
      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const { return keys_down_.contains(keycode); }

      /// @brief Returns whether a key was pressed during the current frame.
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const { return keys_pressed_.contains(keycode); }

      /// @brief Returns whether a key was released during the current frame.
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const { return keys_released_.contains(keycode); }

      /// @brief Returns the latest mouse position for a window, if any motion event was seen.
      [[nodiscard]] std::optional<math::Vec2> mousePosition(WindowHandle window) const {
         const auto it = mouse_position_.find(window);
         return it == mouse_position_.end() ? std::optional<math::Vec2>{} : std::optional<math::Vec2>{it->second};
      }

      /// @brief Returns accumulated mouse delta for a window in the current frame.
      [[nodiscard]] math::Vec2 mouseDelta(WindowHandle window) const {
         const auto it = mouse_delta_.find(window);
         return it == mouse_delta_.end() ? math::Vec2{} : it->second;
      }

      /// @brief Returns accumulated mouse-wheel delta for a window in the current frame.
      [[nodiscard]] math::Vec2 mouseWheelDelta(WindowHandle window) const {
         const auto it = mouse_wheel_delta_.find(window);
         return it == mouse_wheel_delta_.end() ? math::Vec2{} : it->second;
      }

   private:
      std::set<std::int32_t> keys_down_{};               ///< Keys currently held down.
      std::set<std::int32_t> keys_pressed_{};            ///< Keys pressed this frame.
      std::set<std::int32_t> keys_released_{};           ///< Keys released this frame.
      std::map<WindowHandle, math::Vec2> mouse_position_{};    ///< Last mouse position by window.
      std::map<WindowHandle, math::Vec2> mouse_delta_{};       ///< Frame-local mouse delta by window.
      std::map<WindowHandle, math::Vec2> mouse_wheel_delta_{}; ///< Frame-local wheel delta by window.
   };

} // namespace vve
