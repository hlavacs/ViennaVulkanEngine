module;

#include <SDL3/SDL.h>

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
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
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
      };

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

         Uint64 flags = SDL_WINDOW_VULKAN;
         if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
         }
         if (!desc.visible) {
            flags |= SDL_WINDOW_HIDDEN;
         }

         SDL_Window *const window =
             SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.width), static_cast<int>(desc.height), flags);
         if (window == nullptr) {
            return std::unexpected(vve::Error::internal_error);
         }

         // Window identity is derived from the caller-supplied id so it stays
         // stable across runtime rebuilds.
         const WindowHandle handle{vve::Handle::fromHash(std::string_view(desc.id))};
         const auto window_id = SDL_GetWindowID(window);

         WindowRecord record{.handle = handle,
                             .id = desc.id,
                             .window = window,
                             .state = WindowState{.handle = handle,
                                                  .id = desc.id,
                                                  .title = desc.title,
                                                  .width = desc.width,
                                                  .height = desc.height,
                                                  .focused = SDL_GetKeyboardFocus() == window,
                                                  .minimized = false,
                                                  .should_close = false}};

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

      bool video_initialized_{false};                            ///< Tracks whether SDL video has been initialized.
      Vector<WindowRecord> windows_{};                           ///< Owned window records.
      std::unordered_map<Uint32, std::size_t> window_indices_{}; ///< Maps SDL window ids to record indices.
      Vector<WindowState> states_{};                             ///< Contiguous window-state cache exposed to the runtime.
      Vector<WindowEvent> events_{};                             ///< Frame-local translated event list.
      std::shared_ptr<WindowFrameData> frame_data_sink_{};       ///< Shared snapshot sink consumed by the runtime.
   };

   /// @brief Constructs the public window-system facade around the concrete SDL3 implementation.
   template <>
   WindowSystemFacade<SDL3WindowSystemImplementation>::WindowSystemFacade()
       : implementation_(new SDL3WindowSystemImplementation(),
                         [](SDL3WindowSystemImplementation *implementation) { delete implementation; }) {}

   /// @brief Returns the window-system name for the public facade.
   std::string_view WindowSystemFacade<SDL3WindowSystemImplementation>::name() const noexcept {
      return implementation_->name();
   }

   /// @brief Creates runtime windows through the public window-system facade.
   template <>
   std::expected<void, vve::Error>
   WindowSystemFacade<SDL3WindowSystemImplementation>::init(VectorConstRange<vve::WindowDesc> windows) {
      return implementation_->init(windows);
   }

   /// @brief Polls events through the public window-system facade.
   template <>
   std::expected<void, vve::Error>
   WindowSystemFacade<SDL3WindowSystemImplementation>::pollEvents(const FrameContext &frame_context) {
      return implementation_->pollEvents(frame_context);
   }

   /// @brief Returns frame data through the public window-system facade.
   template <> WindowFrameData WindowSystemFacade<SDL3WindowSystemImplementation>::frameData() const {
      return implementation_->frameData();
   }

   /// @brief Returns runtime windows through the public window-system facade.
   template <>
   VectorConstRange<WindowState> WindowSystemFacade<SDL3WindowSystemImplementation>::windows() const {
      return implementation_->windows();
   }

   /// @brief Installs the shared frame-data sink through the public window-system facade.
   template <>
   void
   WindowSystemFacade<SDL3WindowSystemImplementation>::setFrameDataSink(std::shared_ptr<WindowFrameData> frame_data) {
      implementation_->setFrameDataSink(std::move(frame_data));
   }

   /// @brief Registers window tasks through the public window-system facade.
   template <> void WindowSystemFacade<SDL3WindowSystemImplementation>::registerTasks(TaskGraphBuilder &builder) {
      implementation_->registerTasks(builder);
   }

   /// @brief Emits the explicit window-system facade instantiation for v3.
   template class WindowSystemFacade<SDL3WindowSystemImplementation>;

} // namespace vve::v3
