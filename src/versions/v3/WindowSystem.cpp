module;

#include "FacadeMacros.hpp"
#include <SDL3/SDL.h>
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_DEFINED_SDL_MAIN_HANDLED
#endif

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 SDL3-backed window-system implementation.
 *
 * The window system owns platform window lifetime, event translation, and the
 * frame-local window snapshot consumed by the rest of the runtime.
 */
namespace vve::v3 {

   /**
    * @brief Concrete SDL3-backed window-system implementation used by v3.
    *
    * The implementation owns SDL window lifetime, translates SDL events into
    * engine window events, and keeps a frame-local snapshot shared with the
    * rest of the runtime.
    */
   class SDL3WindowSystemImplementation {
   public:
      /// @brief Destroys owned SDL windows and releases the SDL video subsystem.
      ~SDL3WindowSystemImplementation() {
         for (auto &record : windows_) {
            if (record.window != nullptr) {
               SDL_DestroyWindow(record.window);
               record.window = nullptr;
            }
         }

         if (video_initialized_) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
         }
      }

      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "SDL3WindowSystem"; }

      /**
       * @brief Creates the configured runtime windows.
       * @param windows Window descriptors supplied by the engine configuration.
       * @return Empty success result, or an SDL/configuration error.
       */
      [[nodiscard]] std::expected<void, vve::Error>
      init(VectorConstRange<vve::WindowDesc> windows) {
         if (!video_initialized_) {
            configureVulkanLoader();
            SDL_SetMainReady();
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
               std::cerr << "[SDL3WindowSystem] Failed to initialize SDL video subsystem: " << SDL_GetError() << '\n';
               return std::unexpected(vve::Error::internal_error);
            }

            video_initialized_ = true;
         }

         // Reinitialization tears down previously created windows so the
         // runtime snapshot always matches the new descriptor set exactly.
         for (auto &record : windows_) {
            if (record.window != nullptr) {
               SDL_DestroyWindow(record.window);
               record.window = nullptr;
            }
         }

         windows_.clear();
         states_.clear();
         window_indices_.clear();
         events_.clear();

         for (const auto &desc : windows) {
            // Each descriptor becomes one owned SDL window plus one cached
            // engine-facing `WindowState` record.
            auto create_result = createWindow(desc);
            if (!create_result) {
               return std::unexpected(create_result.error());
            }
         }

         syncFrameData();
         return {};
      }

      /// @brief Returns Vulkan instance extensions required by SDL for presentation.
      [[nodiscard]] std::expected<std::vector<std::string>, vve::Error> vulkanInstanceExtensions() const {
         if (!video_initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         Uint32 extension_count = 0;
         const char *const *const extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
         if (extensions == nullptr) {
            std::cerr << "[SDL3WindowSystem] Failed to query Vulkan instance extensions: " << SDL_GetError() << '\n';
            return std::unexpected(vve::Error::internal_error);
         }

         std::vector<std::string> required_extensions{};
         required_extensions.reserve(extension_count);
         for (Uint32 extension_index = 0; extension_index < extension_count; ++extension_index) {
            if (extensions[extension_index] != nullptr) {
               required_extensions.emplace_back(extensions[extension_index]);
            }
         }

         return required_extensions;
      }

      /// @brief Returns opaque native window handles used by the Vulkan backend.
      [[nodiscard]] std::vector<NativeWindowHandle> nativeWindowHandles() const {
         std::vector<NativeWindowHandle> handles{};
         handles.reserve(windows_.size());
         for (const auto &record : windows_) {
            handles.push_back(NativeWindowHandle{.window = record.handle,
                                                .window_id = record.id,
                                                .native_window = record.window,
                                                .width = record.state.width,
                                                .height = record.state.height,
                                                .vulkan_capable = record.vulkan_capable});
         }

         return handles;
      }

      /**
       * @brief Polls SDL events and refreshes the frame-local event snapshot.
       * @param frame_context Current frame timing data.
       * @return Empty success result, or an error when the video subsystem is unavailable.
       */
      [[nodiscard]] std::expected<void, vve::Error> pollEvents(const FrameContext &) {
         if (!video_initialized_) {
            return std::unexpected(vve::Error::not_initialized);
         }

         // Event storage is frame-local and rebuilt every poll.
         events_.clear();
         SDL_PumpEvents();

         SDL_Event event{};
         while (SDL_PollEvent(&event)) {
            translateEvent(event);
         }

         appendHeldKeyEvents();
         syncFrameData();
         return {};
      }

      /// @brief Returns the current frame-local window and event snapshot.
      [[nodiscard]] WindowFrameData frameData() const {
         return WindowFrameData{.windows = makeRange(states_), .events = makeRange(events_)};
      }

      /// @brief Returns the current runtime window-state range.
      [[nodiscard]] VectorConstRange<WindowState> windows() const { return makeRange(states_); }

      /// @brief Installs the shared frame-data sink consumed by other runtime systems.
      void setFrameDataSink(std::shared_ptr<WindowFrameData> frame_data) {
         frame_data_sink_ = std::move(frame_data);
         syncFrameData();
      }

      /// @brief Registers the built-in window-event polling task.
      void registerTasks(TaskGraphBuilder &builder) {
         [[maybe_unused]] const auto poll_window_events_task = builder.addTask(
             "task.poll_window_events", TaskKernelId::poll_window_events,
             detail::requireFrame([this](const FrameContext &frame_context) { return pollEvents(frame_context); }),
             {TaskGraphBuilder::taskHandleFor("task.begin_frame")}, {}, "Poll Window Events", TaskPhase::input);
      }

   private:
      /// @brief Owned SDL window plus cached engine-facing state.
      struct WindowRecord {
         WindowHandle handle{};       ///< Stable engine handle for the window.
         std::string id{};            ///< Stable string id configured by the caller.
         SDL_Window *window{nullptr}; ///< Owned SDL window pointer.
         WindowState state{};         ///< Cached engine-facing window state.
         bool vulkan_capable{false};  ///< Whether this window was created with SDL_WINDOW_VULKAN.
      };

      /// @brief Builds the SDL flag set for one engine window descriptor.
      [[nodiscard]] static Uint64 buildWindowFlags(const vve::WindowDesc &desc) {
         Uint64 flags = SDL_WINDOW_VULKAN;
         if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
         }
         if (!desc.visible) {
            flags |= SDL_WINDOW_HIDDEN;
         }
         return flags;
      }

      /// @brief Keeps SDL's Vulkan window support on the same loader selected by CMake.
      void configureVulkanLoader() const {
#ifdef VVE_SDL_VULKAN_LIBRARY
         if (!SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY)) {
            std::clog << "[SDL3WindowSystem] Failed to set SDL Vulkan loader hint: " << SDL_GetError() << '\n';
         }
#endif
      }

      /// @brief Chooses a visible position for the next created window.
      [[nodiscard]] std::pair<int, int> nextWindowPosition(SDL_Window *window) const {
         if (window == nullptr) {
            return {0, 0};
         }

         int width = 0;
         int height = 0;
         SDL_GetWindowSize(window, &width, &height);

         constexpr int margin = 32;
         constexpr int gap = 24;
         constexpr int cascade_step = 48;

         SDL_Rect usable_bounds{};
         const SDL_DisplayID display = SDL_GetPrimaryDisplay();
         if (display == 0 || !SDL_GetDisplayUsableBounds(display, &usable_bounds)) {
            const int offset = static_cast<int>(std::min<std::size_t>(windows_.size(), 8U)) * cascade_step;
            return {margin + offset, margin + offset};
         }

         if (windows_.empty()) {
            return {usable_bounds.x + margin, usable_bounds.y + margin};
         }

         int previous_x = usable_bounds.x + margin;
         int previous_y = usable_bounds.y + margin;
         if (windows_.back().window != nullptr) {
            SDL_GetWindowPosition(windows_.back().window, &previous_x, &previous_y);
         }

         const int usable_right = usable_bounds.x + usable_bounds.w - margin;
         const int usable_bottom = usable_bounds.y + usable_bounds.h - margin;
         const int previous_right = previous_x + static_cast<int>(windows_.back().state.width);
         const int previous_bottom = previous_y + static_cast<int>(windows_.back().state.height);

         if (previous_right + gap + width <= usable_right) {
            return {previous_right + gap, previous_y};
         }

         if (previous_bottom + gap + height <= usable_bottom) {
            return {usable_bounds.x + margin, previous_bottom + gap};
         }

         const int offset = static_cast<int>(std::min<std::size_t>(windows_.size(), 8U)) * cascade_step;
         return {usable_bounds.x + margin + offset, usable_bounds.y + margin + offset};
      }

      /// @brief Places newly created windows where they stay discoverable on screen.
      void positionWindow(SDL_Window *window) const {
         const auto [x, y] = nextWindowPosition(window);
         SDL_SetWindowPosition(window, x, y);
      }

      /**
       * @brief Creates one SDL window from an engine window descriptor.
       * @param desc Window descriptor from engine configuration.
       * @return Empty success result, or a configuration/SDL creation error.
       */
      [[nodiscard]] std::expected<void, vve::Error> createWindow(const vve::WindowDesc &desc) {
         if (desc.id.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }
         // Stable string ids must remain unique because other subsystems use
         // them when naming per-window tasks and diagnostics.
         if (std::ranges::any_of(windows_, [&desc](const WindowRecord &record) { return record.id == desc.id; })) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const Uint64 flags = buildWindowFlags(desc);

         SDL_Window *const window =
             SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.width), static_cast<int>(desc.height), flags);
         SDL_Window *resolved_window = window;
         bool vulkan_capable = true;
         if (resolved_window == nullptr) {
            const std::string vulkan_error = SDL_GetError();
            resolved_window = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.width), static_cast<int>(desc.height),
                                               flags & ~SDL_WINDOW_VULKAN);
            if (resolved_window == nullptr) {
               std::cerr << "[SDL3WindowSystem] Failed to create window '" << desc.id << "': "
                         << SDL_GetError() << '\n';
               return std::unexpected(vve::Error::internal_error);
            }

            std::clog << "[SDL3WindowSystem] Vulkan-capable window creation is unavailable for '" << desc.id << "': "
                      << vulkan_error << ". Falling back to a standard SDL window.\n";
            vulkan_capable = false;
         }

         positionWindow(resolved_window);

         int actual_width = 0;
         int actual_height = 0;
         SDL_GetWindowSize(resolved_window, &actual_width, &actual_height);

         // Window identity is derived from the caller-supplied id so it stays
         // stable across runtime rebuilds.
         const WindowHandle handle{vve::Handle::fromHash(std::string_view(desc.id))};
         const auto window_id = SDL_GetWindowID(resolved_window);

         WindowRecord record{.handle = handle,
                             .id = desc.id,
                             .window = resolved_window,
                             .state = WindowState{.handle = handle,
                                                  .id = desc.id,
                                                  .title = desc.title,
                                                  .width = static_cast<std::uint32_t>(std::max(actual_width, 0)),
                                                  .height = static_cast<std::uint32_t>(std::max(actual_height, 0)),
                                                  .renderer_id = desc.renderer_id,
                                                  .focused = SDL_GetKeyboardFocus() == resolved_window,
                                                  .minimized = false,
                                                  .should_close = false},
                             .vulkan_capable = vulkan_capable};

         window_indices_[window_id] = windows_.size();
         windows_.push_back(std::move(record));
         rebuildStateCache();
         return {};
      }

      /**
       * @brief Translates one SDL event into engine-facing window state and event records.
       * @param event SDL event fetched from the platform queue.
       */
      void translateEvent(const SDL_Event &event) {
         switch (event.type) {
         case SDL_EVENT_QUIT:
            markAllWindowsClosing();
            break;
         case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            pushWindowEvent(event.window.windowID, WindowEventType::close_requested, 0, 0,
                            [](WindowState &state, std::int32_t, std::int32_t) { state.should_close = true; });
            break;
         case SDL_EVENT_WINDOW_RESIZED:
            pushWindowEvent(event.window.windowID, WindowEventType::resized, event.window.data1, event.window.data2,
                            [](WindowState &state, std::int32_t width, std::int32_t height) {
                               state.width = static_cast<std::uint32_t>(std::max(width, 0));
                               state.height = static_cast<std::uint32_t>(std::max(height, 0));
                            });
            break;
         case SDL_EVENT_WINDOW_MOVED:
            pushWindowEvent(event.window.windowID, WindowEventType::moved, event.window.data1, event.window.data2);
            break;
         case SDL_EVENT_WINDOW_FOCUS_GAINED:
            pushWindowEvent(event.window.windowID, WindowEventType::focus_gained, 0, 0,
                            [](WindowState &state, std::int32_t, std::int32_t) { state.focused = true; });
            break;
         case SDL_EVENT_WINDOW_FOCUS_LOST:
            pushWindowEvent(event.window.windowID, WindowEventType::focus_lost, 0, 0,
                            [](WindowState &state, std::int32_t, std::int32_t) { state.focused = false; });
            break;
         case SDL_EVENT_WINDOW_MINIMIZED:
            updateWindowState(event.window.windowID, [](WindowState &state) { state.minimized = true; });
            break;
         case SDL_EVENT_WINDOW_RESTORED:
            updateWindowState(event.window.windowID, [](WindowState &state) { state.minimized = false; });
            break;
         case SDL_EVENT_KEY_DOWN:
            pushWindowEvent(event.key.windowID, WindowEventType::key_down,
                            static_cast<std::int32_t>(event.key.scancode), static_cast<std::int32_t>(event.key.key));
            break;
         case SDL_EVENT_KEY_UP:
            pushWindowEvent(event.key.windowID, WindowEventType::key_up, static_cast<std::int32_t>(event.key.scancode),
                            static_cast<std::int32_t>(event.key.key));
            break;
         case SDL_EVENT_MOUSE_MOTION:
            pushWindowEvent(event.motion.windowID, WindowEventType::mouse_move,
                            static_cast<std::int32_t>(event.motion.x), static_cast<std::int32_t>(event.motion.y));
            break;
         case SDL_EVENT_MOUSE_BUTTON_DOWN:
            pushWindowEvent(event.button.windowID, WindowEventType::mouse_button_down,
                            static_cast<std::int32_t>(event.button.button),
                            static_cast<std::int32_t>(event.button.clicks));
            break;
         case SDL_EVENT_MOUSE_BUTTON_UP:
            pushWindowEvent(event.button.windowID, WindowEventType::mouse_button_up,
                            static_cast<std::int32_t>(event.button.button),
                            static_cast<std::int32_t>(event.button.clicks));
            break;
         case SDL_EVENT_MOUSE_WHEEL:
            pushWindowEvent(event.wheel.windowID, WindowEventType::mouse_wheel,
                            static_cast<std::int32_t>(event.wheel.x), static_cast<std::int32_t>(event.wheel.y));
            break;
         default:
            break;
         }
      }

      /// @brief Adds current keyboard-state events so held keys are independent of OS key repeat.
      void appendHeldKeyEvents() {
         SDL_Window *const keyboard_focus = SDL_GetKeyboardFocus();
         if (keyboard_focus == nullptr) {
            return;
         }

         const SDL_WindowID window_id = SDL_GetWindowID(keyboard_focus);
         if (!findWindowIndex(window_id).has_value()) {
            return;
         }

         int key_count = 0;
         const bool *const keyboard_state = SDL_GetKeyboardState(&key_count);
         if (keyboard_state == nullptr || key_count <= 0) {
            return;
         }

         const SDL_Keymod modifiers = SDL_GetModState();
         for (int scancode = 0; scancode < key_count; ++scancode) {
            if (!keyboard_state[scancode]) {
               continue;
            }

            const auto sdl_scancode = static_cast<SDL_Scancode>(scancode);
            const SDL_Keycode keycode = SDL_GetKeyFromScancode(sdl_scancode, modifiers, true);
            if (keycode == SDLK_UNKNOWN) {
               continue;
            }

            pushWindowEvent(window_id, WindowEventType::key_held, scancode, static_cast<std::int32_t>(keycode));
         }
      }

      /// @brief Marks all known windows as closing after a global SDL quit request.
      void markAllWindowsClosing() {
         for (auto &record : windows_) {
            record.state.should_close = true;
            events_.push_back(WindowEvent{.window = record.handle, .type = WindowEventType::close_requested});
         }
         rebuildStateCache();
      }

      /**
       * @brief Applies an event-specific state mutation and stores the translated event.
       * @tparam TMutator Callable that mutates `WindowState`.
       * @param window_id SDL window id associated with the event.
       * @param type Engine window-event type.
       * @param a First integer payload.
       * @param b Second integer payload.
       * @param mutator State mutator executed before caching the new state.
       */
      template <typename TMutator>
      void pushWindowEvent(Uint32 window_id, WindowEventType type, std::int32_t a, std::int32_t b, TMutator &&mutator) {
         const auto index = findWindowIndex(window_id);
         if (!index.has_value()) {
            return;
         }

         auto &record = windows_[*index];
         // The authoritative state lives on the owned record and is then
         // mirrored into the contiguous cache exposed to the rest of the engine.
         std::forward<TMutator>(mutator)(record.state, a, b);
         events_.push_back(WindowEvent{.window = record.handle, .type = type, .a = a, .b = b});
         states_[*index] = record.state;
      }

      /// @brief Convenience overload for events that do not mutate cached state.
      void pushWindowEvent(Uint32 window_id, WindowEventType type, std::int32_t a, std::int32_t b) {
         pushWindowEvent(window_id, type, a, b, [](WindowState &, std::int32_t, std::int32_t) {});
      }

      /// @brief Applies a direct state mutation for events that do not emit an explicit `WindowEvent`.
      template <typename TMutator> void updateWindowState(Uint32 window_id, TMutator &&mutator) {
         const auto index = findWindowIndex(window_id);
         if (!index.has_value()) {
            return;
         }

         auto &record = windows_[*index];
         std::forward<TMutator>(mutator)(record.state);
         states_[*index] = record.state;
      }

      /// @brief Returns the internal record index for an SDL window id when present.
      [[nodiscard]] std::optional<std::size_t> findWindowIndex(Uint32 window_id) const {
         const auto it = window_indices_.find(window_id);
         if (it == window_indices_.end()) {
            return std::nullopt;
         }

         return it->second;
      }

      /// @brief Rebuilds the contiguous `WindowState` cache from the owned records.
      void rebuildStateCache() {
         states_.clear();
         states_.reserve(windows_.size());
         for (const auto &record : windows_) {
            states_.push_back(record.state);
         }
      }

      /// @brief Copies the current caches into the shared frame-data sink when one is bound.
      void syncFrameData() const {
         if (frame_data_sink_ == nullptr) {
            return;
         }

         frame_data_sink_->windows = makeRange(states_);
         frame_data_sink_->events = makeRange(events_);
      }

      bool video_initialized_{false};                             ///< Tracks whether SDL video has been initialized.
      Vector<WindowRecord> windows_{};                            ///< Owned window records.
      std::unordered_map<Uint32, std::size_t> window_indices_{}; ///< Maps SDL window ids to record indices.
      Vector<WindowState> states_{};                              ///< Contiguous window-state cache exposed to the runtime.
      Vector<WindowEvent> events_{};                              ///< Frame-local translated event list.
      std::shared_ptr<WindowFrameData> frame_data_sink_{};       ///< Shared snapshot sink consumed by the runtime.
   };

   /// @brief Constructs the public window-system facade around the concrete SDL3 implementation.
   VVE_V3_DEFINE_FACADE_CTOR(WindowSystemFacade, SDL3WindowSystemImplementation, (), ())

   /// @brief Returns the window-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Creates runtime windows through the public window-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, init,
                               (VectorConstRange<vve::WindowDesc> windows), (windows), ,
                               std::expected<void, vve::Error>)

   /// @brief Returns SDL-required Vulkan instance extensions through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, vulkanInstanceExtensions, (), (),
                               const, std::expected<std::vector<std::string>, vve::Error>)

   /// @brief Returns native window handles through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, nativeWindowHandles, (), (), const,
                               std::vector<NativeWindowHandle>)

   /// @brief Polls events through the public window-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, pollEvents,
                               (const FrameContext &frame_context), (frame_context), ,
                               std::expected<void, vve::Error>)

   /// @brief Returns frame data through the public window-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, frameData, (), (), const,
                               WindowFrameData)

   /// @brief Returns runtime windows through the public window-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, windows, (), (), const,
                               VectorConstRange<WindowState>)

   /// @brief Installs the shared frame-data sink through the public window-system facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, setFrameDataSink,
                                    (std::shared_ptr<WindowFrameData> frame_data), (std::move(frame_data)), )

   /// @brief Registers window tasks through the public window-system facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(WindowSystemFacade, SDL3WindowSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder), (builder), )

   /// @brief Emits the explicit window-system facade instantiation for v3.
   template class WindowSystemFacade<SDL3WindowSystemImplementation>;

} // namespace vve::v3
