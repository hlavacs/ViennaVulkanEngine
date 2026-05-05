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

export module VEEngine.V4:WindowSystem;
import std;
export import :Window;

/// @file
/// @brief SDL-backed platform window system for v4.

export namespace vve::v4 {

   /// @brief Owns SDL windows and translates platform events into input and window state.
   class WindowSystem {
   public:
      WindowSystem();             ///< Creates an empty window system.
      ~WindowSystem();            ///< Destroys owned SDL windows and shuts down the video subsystem.
      WindowSystem(WindowSystem &&) noexcept;            ///< Moves the window system and owned implementation.
      WindowSystem &operator=(WindowSystem &&) noexcept; ///< Moves the window system and owned implementation.
      WindowSystem(const WindowSystem &) = delete;       ///< SDL windows cannot be copied safely.
      WindowSystem &operator=(const WindowSystem &) = delete; ///< SDL windows cannot be copied safely.

      /// @brief Returns the implementation name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept;
      /// @brief Creates all startup windows from descriptors.
      [[nodiscard]] std::expected<void, Error> init(const Windows &windows);
      /// @brief Polls SDL events and mutates the supplied input snapshot.
      [[nodiscard]] std::expected<void, Error> poll(InputState &input);
      /// @brief Returns a copy of the current window-state snapshot.
      [[nodiscard]] Vector<WindowInfo> snapshot() const;
      /// @brief Returns read-only references to owned window states.
      [[nodiscard]] Vector<std::reference_wrapper<const WindowInfo>> windows() const;
      /// @brief Returns the number of owned windows.
      [[nodiscard]] std::size_t windowCount() const;
      /// @brief Finds a read-only window by application id.
      [[nodiscard]] const WindowInfo *findWindow(std::string_view id) const;
      /// @brief Finds a read-only window by runtime handle.
      [[nodiscard]] const WindowInfo *findWindow(WindowHandle handle) const;
      /// @brief Returns true when any window has requested closing.
      [[nodiscard]] bool anyShouldClose() const;

   private:
      struct Impl;                 ///< SDL-owning implementation hidden from module importers.
      std::unique_ptr<Impl> impl_; ///< Pimpl keeps SDL headers out of the public v4 module.
   };

} // namespace vve::v4

namespace vve::v4 {

   /// @brief Hidden implementation that stores owned v4 window implementations.
   struct WindowSystem::Impl {
      /// @brief Destroys SDL windows and tears down video if this object initialized it.
      ~Impl() {
         windows.clear();
         if (video_initialized) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
      }

      /// @brief Finds a mutable window implementation by SDL window id.
      [[nodiscard]] Window *find(SDL_WindowID id) {
         const auto it = indices.find(id);
         return it == indices.end() ? nullptr : std::addressof(windows[it->second]);
      }

      /// @brief Finds a read-only window implementation by SDL window id.
      [[nodiscard]] const Window *find(SDL_WindowID id) const {
         const auto it = indices.find(id);
         return it == indices.end() ? nullptr : std::addressof(windows[it->second]);
      }

      /// @brief Finds a mutable window implementation by public window handle.
      [[nodiscard]] Window *find(WindowHandle handle) {
         const auto it = std::ranges::find_if(windows, [handle](const Window &window) {
            return window.info().handle == handle;
         });
         return it == windows.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Finds a read-only window implementation by public window handle.
      [[nodiscard]] const Window *find(WindowHandle handle) const {
         const auto it = std::ranges::find_if(windows, [handle](const Window &window) {
            return window.info().handle == handle;
         });
         return it == windows.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Finds a mutable window implementation by application-local id.
      [[nodiscard]] Window *find(std::string_view id) {
         const auto it = std::ranges::find_if(windows, [id](const Window &window) {
            return window.info().id == id;
         });
         return it == windows.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Finds a read-only window implementation by application-local id.
      [[nodiscard]] const Window *find(std::string_view id) const {
         const auto it = std::ranges::find_if(windows, [id](const Window &window) {
            return window.info().id == id;
         });
         return it == windows.end() ? nullptr : std::addressof(*it);
      }

      /// @brief Marks every window as closing after SDL emits a process-wide quit event.
      void closeAll() {
         for (auto &window : windows) { window.info().should_close = true; }
      }

      bool video_initialized{false};                 ///< True after SDL video init succeeds.
      Vector<Window> windows{};                      ///< Owned window implementations.
      std::map<SDL_WindowID, std::size_t> indices{}; ///< SDL id to window index.
   };

   WindowSystem::WindowSystem() : impl_{std::make_unique<Impl>()} {}

   WindowSystem::~WindowSystem() = default;

   WindowSystem::WindowSystem(WindowSystem &&) noexcept = default;

   WindowSystem &WindowSystem::operator=(WindowSystem &&) noexcept = default;

   std::string_view WindowSystem::name() const noexcept { return "SDL3WindowSystem"; }

   std::expected<void, Error> WindowSystem::init(const Windows &windows) {
      SDL_SetMainReady();
      if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) { return std::unexpected(Error::platform_error); }
      impl_->video_initialized = true;

      for (const auto &desc : windows.value) {
         SDL_WindowFlags flags = 0;
         if (desc.resizable) { flags |= SDL_WINDOW_RESIZABLE; }
         if (!desc.visible) { flags |= SDL_WINDOW_HIDDEN; }

         SDL_Window *const window = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.extent.width),
                                                     static_cast<int>(desc.extent.height), flags);
         if (window == nullptr) { return std::unexpected(Error::platform_error); }

         if (desc.x.has_value() || desc.y.has_value()) {
            int x = 0;
            int y = 0;
            SDL_GetWindowPosition(window, &x, &y);
            SDL_SetWindowPosition(window, desc.x.value_or(x), desc.y.value_or(y));
         }

         int width = 0;
         int height = 0;
         SDL_GetWindowSize(window, &width, &height);

         auto info = WindowInfo{.handle = makeCounterHandle<WindowHandle>(),
                                .id = desc.id,
                                .title = desc.title,
                                .extent = PixelExtent{.width = static_cast<std::uint32_t>(std::max(width, 0)),
                                                      .height = static_cast<std::uint32_t>(std::max(height, 0))},
                                .renderer_id = desc.renderer_id,
                                .focused = SDL_GetKeyboardFocus() == window,
                                .minimized = false,
                                .should_close = false};
         const SDL_WindowID id = SDL_GetWindowID(window);
         impl_->indices[id] = impl_->windows.size();
         impl_->windows.emplace_back(window, id, std::move(info));
      }

      return {};
   }

   std::expected<void, Error> WindowSystem::poll(InputState &input) {
      input.beginFrame();
      SDL_Event event{};
      while (SDL_PollEvent(&event)) {
         switch (event.type) {
         case SDL_EVENT_QUIT:
            impl_->closeAll();
            break;
         case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (auto *window = impl_->find(event.window.windowID)) { window->info().should_close = true; }
            break;
         case SDL_EVENT_WINDOW_RESIZED:
            if (auto *window = impl_->find(event.window.windowID)) {
               window->info().extent = PixelExtent{
                  .width = static_cast<std::uint32_t>(std::max(event.window.data1, 0)),
                  .height = static_cast<std::uint32_t>(std::max(event.window.data2, 0))};
            }
            break;
         case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (auto *window = impl_->find(event.window.windowID)) { window->info().focused = true; }
            break;
         case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (auto *window = impl_->find(event.window.windowID)) { window->info().focused = false; }
            break;
         case SDL_EVENT_WINDOW_MINIMIZED:
            if (auto *window = impl_->find(event.window.windowID)) { window->info().minimized = true; }
            break;
         case SDL_EVENT_WINDOW_RESTORED:
            if (auto *window = impl_->find(event.window.windowID)) { window->info().minimized = false; }
            break;
         case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat) { input.pressKey(static_cast<std::int32_t>(event.key.key)); }
            break;
         case SDL_EVENT_KEY_UP:
            input.releaseKey(static_cast<std::int32_t>(event.key.key));
            break;
         case SDL_EVENT_MOUSE_MOTION:
            if (const auto *window = impl_->find(event.motion.windowID)) {
               input.setMousePosition(window->info().handle, Vec2{event.motion.x, event.motion.y});
               input.addMouseDelta(window->info().handle, Vec2{event.motion.xrel, event.motion.yrel});
            }
            break;
         case SDL_EVENT_MOUSE_WHEEL:
            if (const auto *window = impl_->find(event.wheel.windowID)) {
               input.addMouseWheelDelta(window->info().handle, Vec2{event.wheel.x, event.wheel.y});
            }
            break;
         default:
            break;
         }
      }

      return {};
   }

   Vector<WindowInfo> WindowSystem::snapshot() const {
      Vector<WindowInfo> result{};
      result.reserve(impl_->windows.size());
      for (const auto &window : impl_->windows) { result.push_back(window.info()); }
      return result;
   }

   Vector<std::reference_wrapper<const WindowInfo>> WindowSystem::windows() const {
      Vector<std::reference_wrapper<const WindowInfo>> result{};
      result.reserve(impl_->windows.size());
      for (const auto &window : impl_->windows) { result.push_back(std::cref(window.info())); }
      return result;
   }

   std::size_t WindowSystem::windowCount() const { return impl_->windows.size(); }

   const WindowInfo *WindowSystem::findWindow(std::string_view id) const {
      const auto *record = impl_->find(id);
      return record == nullptr ? nullptr : std::addressof(record->info());
   }

   const WindowInfo *WindowSystem::findWindow(WindowHandle handle) const {
      const auto *record = impl_->find(handle);
      return record == nullptr ? nullptr : std::addressof(record->info());
   }

   bool WindowSystem::anyShouldClose() const {
      return std::ranges::any_of(impl_->windows, [](const Window &window) {
         return window.info().should_close;
      });
   }

} // namespace vve::v4
