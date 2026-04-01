module;

#include <SDL3/SDL.h>

module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class SDL3WindowSystem final : public IWindowSystem {
public:
    ~SDL3WindowSystem() override {
        for (auto& record : windows_) {
            if (record.window != nullptr) {
                SDL_DestroyWindow(record.window);
                record.window = nullptr;
            }
        }

        if (video_initialized_) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "SDL3WindowSystem";
    }

    [[nodiscard]] std::expected<void, vve::Result> init(
        std::span<const vve::WindowDesc> windows) override {
        if (!video_initialized_) {
            if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
                return std::unexpected(vve::Result::internal_error);
            }

            video_initialized_ = true;
        }

        for (auto& record : windows_) {
            if (record.window != nullptr) {
                SDL_DestroyWindow(record.window);
                record.window = nullptr;
            }
        }

        windows_.clear();
        states_.clear();
        window_indices_.clear();
        events_.clear();

        for (const auto& desc : windows) {
            auto create_result = createWindow(desc);
            if (!create_result) {
                return std::unexpected(create_result.error());
            }
        }

        syncFrameData();
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> pollEvents(
        const FrameContext&) override {
        if (!video_initialized_) {
            return std::unexpected(vve::Result::not_initialized);
        }

        events_.clear();

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            translateEvent(event);
        }

        syncFrameData();
        return {};
    }

    [[nodiscard]] WindowFrameData frameData() const override {
        return WindowFrameData{
            .windows = states_,
            .events = events_
        };
    }

    void setFrameDataSink(std::shared_ptr<WindowFrameData> frame_data) override {
        frame_data_sink_ = std::move(frame_data);
        syncFrameData();
    }

    void registerTasks(TaskGraphBuilder& builder) override {
        const auto poll_window_events_task = builder.addTask(
            "task.poll_window_events",
            TaskKernelId::poll_window_events,
            {},
            {TaskGraphBuilder::taskHandleFor("task.begin_frame")},
            {},
            "Poll Window Events");

        builder.setTaskCallback(
            poll_window_events_task,
            [this](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return pollEvents(*execution_context.frame_context);
            });
    }

private:
    struct WindowRecord {
        WindowHandle handle{};
        std::string id{};
        SDL_Window* window{nullptr};
        WindowState state{};
    };

    [[nodiscard]] std::expected<void, vve::Result> createWindow(const vve::WindowDesc& desc) {
        if (desc.id.empty()) {
            return std::unexpected(vve::Result::invalid_argument);
        }
        if (std::ranges::any_of(windows_, [&desc](const WindowRecord& record) {
                return record.id == desc.id;
            })) {
            return std::unexpected(vve::Result::invalid_argument);
        }

        Uint64 flags = SDL_WINDOW_VULKAN;
        if (desc.resizable) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        if (!desc.visible) {
            flags |= SDL_WINDOW_HIDDEN;
        }

        SDL_Window* const window = SDL_CreateWindow(
            desc.title.c_str(),
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            flags);
        if (window == nullptr) {
            return std::unexpected(vve::Result::internal_error);
        }

        const WindowHandle handle{vve::Handle::fromHash(std::string_view(desc.id))};
        const auto window_id = SDL_GetWindowID(window);

        WindowRecord record{
            .handle = handle,
            .id = desc.id,
            .window = window,
            .state = WindowState{
                .handle = handle,
                .id = desc.id,
                .title = desc.title,
                .width = desc.width,
                .height = desc.height,
                .focused = SDL_GetKeyboardFocus() == window,
                .minimized = false,
                .should_close = false
            }
        };

        window_indices_[window_id] = windows_.size();
        windows_.push_back(std::move(record));
        rebuildStateCache();
        return {};
    }

    void translateEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                markAllWindowsClosing();
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                pushWindowEvent(
                    event.window.windowID,
                    WindowEventType::close_requested,
                    0,
                    0,
                    [](WindowState& state, std::int32_t, std::int32_t) {
                        state.should_close = true;
                    });
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                pushWindowEvent(
                    event.window.windowID,
                    WindowEventType::resized,
                    event.window.data1,
                    event.window.data2,
                    [](WindowState& state, std::int32_t width, std::int32_t height) {
                        state.width = static_cast<std::uint32_t>(std::max(width, 0));
                        state.height = static_cast<std::uint32_t>(std::max(height, 0));
                    });
                break;
            case SDL_EVENT_WINDOW_MOVED:
                pushWindowEvent(
                    event.window.windowID,
                    WindowEventType::moved,
                    event.window.data1,
                    event.window.data2);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                pushWindowEvent(
                    event.window.windowID,
                    WindowEventType::focus_gained,
                    0,
                    0,
                    [](WindowState& state, std::int32_t, std::int32_t) {
                        state.focused = true;
                    });
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                pushWindowEvent(
                    event.window.windowID,
                    WindowEventType::focus_lost,
                    0,
                    0,
                    [](WindowState& state, std::int32_t, std::int32_t) {
                        state.focused = false;
                    });
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                updateWindowState(
                    event.window.windowID,
                    [](WindowState& state) {
                        state.minimized = true;
                    });
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                updateWindowState(
                    event.window.windowID,
                    [](WindowState& state) {
                        state.minimized = false;
                    });
                break;
            case SDL_EVENT_KEY_DOWN:
                pushWindowEvent(
                    event.key.windowID,
                    WindowEventType::key_down,
                    static_cast<std::int32_t>(event.key.scancode),
                    static_cast<std::int32_t>(event.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                pushWindowEvent(
                    event.key.windowID,
                    WindowEventType::key_up,
                    static_cast<std::int32_t>(event.key.scancode),
                    static_cast<std::int32_t>(event.key.key));
                break;
            case SDL_EVENT_MOUSE_MOTION:
                pushWindowEvent(
                    event.motion.windowID,
                    WindowEventType::mouse_move,
                    static_cast<std::int32_t>(event.motion.x),
                    static_cast<std::int32_t>(event.motion.y));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                pushWindowEvent(
                    event.button.windowID,
                    WindowEventType::mouse_button_down,
                    static_cast<std::int32_t>(event.button.button),
                    static_cast<std::int32_t>(event.button.clicks));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                pushWindowEvent(
                    event.button.windowID,
                    WindowEventType::mouse_button_up,
                    static_cast<std::int32_t>(event.button.button),
                    static_cast<std::int32_t>(event.button.clicks));
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                pushWindowEvent(
                    event.wheel.windowID,
                    WindowEventType::mouse_wheel,
                    static_cast<std::int32_t>(event.wheel.x),
                    static_cast<std::int32_t>(event.wheel.y));
                break;
            default:
                break;
        }
    }

    void markAllWindowsClosing() {
        for (auto& record : windows_) {
            record.state.should_close = true;
            events_.push_back(WindowEvent{
                .window = record.handle,
                .type = WindowEventType::close_requested
            });
        }
        rebuildStateCache();
    }

    template <typename TMutator>
    void pushWindowEvent(
        Uint32 window_id,
        WindowEventType type,
        std::int32_t a,
        std::int32_t b,
        TMutator&& mutator) {
        const auto index = findWindowIndex(window_id);
        if (!index.has_value()) {
            return;
        }

        auto& record = windows_[*index];
        std::forward<TMutator>(mutator)(record.state, a, b);
        events_.push_back(WindowEvent{
            .window = record.handle,
            .type = type,
            .a = a,
            .b = b
        });
        states_[*index] = record.state;
    }

    void pushWindowEvent(
        Uint32 window_id,
        WindowEventType type,
        std::int32_t a,
        std::int32_t b) {
        pushWindowEvent(
            window_id,
            type,
            a,
            b,
            [](WindowState&, std::int32_t, std::int32_t) {
            });
    }

    template <typename TMutator>
    void updateWindowState(Uint32 window_id, TMutator&& mutator) {
        const auto index = findWindowIndex(window_id);
        if (!index.has_value()) {
            return;
        }

        auto& record = windows_[*index];
        std::forward<TMutator>(mutator)(record.state);
        states_[*index] = record.state;
    }

    [[nodiscard]] std::optional<std::size_t> findWindowIndex(Uint32 window_id) const {
        const auto it = window_indices_.find(window_id);
        if (it == window_indices_.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    void rebuildStateCache() {
        states_.clear();
        states_.reserve(windows_.size());
        for (const auto& record : windows_) {
            states_.push_back(record.state);
        }
    }

    void syncFrameData() const {
        if (frame_data_sink_ == nullptr) {
            return;
        }

        frame_data_sink_->windows = states_;
        frame_data_sink_->events = events_;
    }

    bool video_initialized_{false};
    std::vector<WindowRecord> windows_{};
    std::unordered_map<Uint32, std::size_t> window_indices_{};
    std::vector<WindowState> states_{};
    std::vector<WindowEvent> events_{};
    std::shared_ptr<WindowFrameData> frame_data_sink_{};
};

} // namespace

std::unique_ptr<IWindowSystem> detail::createWindowSystem() {
    return std::make_unique<SDL3WindowSystem>();
}

} // namespace vve::v3
