module;

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_V4_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef VVE_V4_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_V4_DEFINED_SDL_MAIN_HANDLED
#endif

export module VEEngine.V4:Window;
import std;
export import :Types;

/// @file
/// @brief v4 window descriptors, input state, and owned platform window object.

export namespace vve::v4 {

   /// @brief Window creation descriptor consumed by the v4 platform layer.
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

   /// @brief Runtime window state exposed through the facade window wrapper.
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
      void beginFrame();
      void holdKey(std::int32_t keycode);
      void pressKey(std::int32_t keycode);
      void releaseKey(std::int32_t keycode);
      void setMousePosition(WindowHandle window, Vec2 position);
      void addMouseDelta(WindowHandle window, Vec2 delta);
      void addMouseWheelDelta(WindowHandle window, Vec2 delta);

      [[nodiscard]] bool isKeyDown(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyPressed(std::int32_t keycode) const;
      [[nodiscard]] bool wasKeyReleased(std::int32_t keycode) const;
      [[nodiscard]] std::optional<Vec2> mousePosition(WindowHandle window) const;
      [[nodiscard]] Vec2 mouseDelta(WindowHandle window) const;
      [[nodiscard]] Vec2 mouseWheelDelta(WindowHandle window) const;

   private:
      [[nodiscard]] static std::int32_t normalizeKey(std::int32_t keycode);

      std::set<std::int32_t> keys_down_{};                      ///< Keys currently held down.
      std::set<std::int32_t> keys_pressed_{};                   ///< Keys pressed this frame.
      std::set<std::int32_t> keys_released_{};                  ///< Keys released this frame.
      std::map<WindowHandle, Vec2> mouse_position_{};           ///< Mouse positions by window.
      std::map<WindowHandle, Vec2> mouse_delta_{};              ///< Mouse motion by window.
      std::map<WindowHandle, Vec2> mouse_wheel_delta_{};        ///< Mouse wheel motion by window.
   };

   /// @brief Owned v4 platform window implementation.
   class Window {
   public:
      Window(SDL_Window *window, SDL_WindowID sdl_id, WindowInfo info) noexcept;
      ~Window();
      Window(Window &&other) noexcept;
      Window &operator=(Window &&other) noexcept;
      Window(const Window &) = delete;
      Window &operator=(const Window &) = delete;

      [[nodiscard]] SDL_Window *native() const noexcept;
      [[nodiscard]] SDL_WindowID sdlId() const noexcept;
      [[nodiscard]] WindowInfo &info() noexcept;
      [[nodiscard]] const WindowInfo &info() const noexcept;
      [[nodiscard]] WindowHandle handle() const noexcept;
      [[nodiscard]] std::string_view id() const noexcept;
      [[nodiscard]] std::string_view title() const noexcept;
      [[nodiscard]] PixelExtent extent() const noexcept;
      [[nodiscard]] RendererId rendererId() const;
      [[nodiscard]] std::optional<Entity> camera() const;
      [[nodiscard]] bool focused() const noexcept;
      [[nodiscard]] bool minimized() const noexcept;
      [[nodiscard]] bool shouldClose() const noexcept;

   private:
      void reset() noexcept;

      SDL_Window *window_{}; ///< Owned SDL window.
      SDL_WindowID sdl_id_{}; ///< SDL-local window id.
      WindowInfo info_{};     ///< Cached public window state.
   };

} // namespace vve::v4

namespace vve::v4 {

   void InputState::beginFrame() {
      keys_pressed_.clear();
      keys_released_.clear();
      mouse_delta_.clear();
      mouse_wheel_delta_.clear();
   }

   void InputState::holdKey(std::int32_t keycode) { keys_down_.insert(normalizeKey(keycode)); }

   void InputState::pressKey(std::int32_t keycode) {
      const auto key = normalizeKey(keycode);
      if (!keys_down_.contains(key)) { keys_pressed_.insert(key); }
      keys_down_.insert(key);
   }

   void InputState::releaseKey(std::int32_t keycode) {
      const auto key = normalizeKey(keycode);
      keys_down_.erase(key);
      keys_pressed_.erase(key);
      keys_released_.insert(key);
   }

   void InputState::setMousePosition(WindowHandle window, Vec2 position) { mouse_position_[window] = position; }

   void InputState::addMouseDelta(WindowHandle window, Vec2 delta) {
      const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
      it->second = math::add(it->second, delta);
   }

   void InputState::addMouseWheelDelta(WindowHandle window, Vec2 delta) {
      const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
      it->second = math::add(it->second, delta);
   }

   bool InputState::isKeyDown(std::int32_t keycode) const { return keys_down_.contains(normalizeKey(keycode)); }

   bool InputState::wasKeyPressed(std::int32_t keycode) const {
      return keys_pressed_.contains(normalizeKey(keycode));
   }

   bool InputState::wasKeyReleased(std::int32_t keycode) const {
      return keys_released_.contains(normalizeKey(keycode));
   }

   std::optional<Vec2> InputState::mousePosition(WindowHandle window) const {
      const auto it = mouse_position_.find(window);
      return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
   }

   Vec2 InputState::mouseDelta(WindowHandle window) const {
      const auto it = mouse_delta_.find(window);
      return it == mouse_delta_.end() ? Vec2{} : it->second;
   }

   Vec2 InputState::mouseWheelDelta(WindowHandle window) const {
      const auto it = mouse_wheel_delta_.find(window);
      return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
   }

   std::int32_t InputState::normalizeKey(std::int32_t keycode) {
      if (keycode >= static_cast<std::int32_t>('A') && keycode <= static_cast<std::int32_t>('Z')) {
         return keycode - static_cast<std::int32_t>('A') + static_cast<std::int32_t>('a');
      }
      return keycode;
   }

   Window::Window(SDL_Window *window, SDL_WindowID sdl_id, WindowInfo info) noexcept
       : window_{window}, sdl_id_{sdl_id}, info_{std::move(info)} {}

   Window::~Window() { reset(); }

   Window::Window(Window &&other) noexcept
       : window_{std::exchange(other.window_, nullptr)},
         sdl_id_{std::exchange(other.sdl_id_, 0)},
         info_{std::move(other.info_)} {}

   Window &Window::operator=(Window &&other) noexcept {
      if (this != std::addressof(other)) {
         reset();
         window_ = std::exchange(other.window_, nullptr);
         sdl_id_ = std::exchange(other.sdl_id_, 0);
         info_ = std::move(other.info_);
      }
      return *this;
   }

   SDL_Window *Window::native() const noexcept { return window_; }

   SDL_WindowID Window::sdlId() const noexcept { return sdl_id_; }

   WindowInfo &Window::info() noexcept { return info_; }

   const WindowInfo &Window::info() const noexcept { return info_; }

   WindowHandle Window::handle() const noexcept { return info_.handle; }

   std::string_view Window::id() const noexcept { return info_.id; }

   std::string_view Window::title() const noexcept { return info_.title; }

   PixelExtent Window::extent() const noexcept { return info_.extent; }

   RendererId Window::rendererId() const { return info_.renderer_id; }

   std::optional<Entity> Window::camera() const { return info_.camera; }

   bool Window::focused() const noexcept { return info_.focused; }

   bool Window::minimized() const noexcept { return info_.minimized; }

   bool Window::shouldClose() const noexcept { return info_.should_close; }

   void Window::reset() noexcept {
      if (window_ != nullptr) {
         SDL_DestroyWindow(window_);
         window_ = nullptr;
      }
   }

} // namespace vve::v4
