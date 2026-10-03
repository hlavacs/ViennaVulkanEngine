module;

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_SIMPLE_DEFINED_SDL_MAIN_HANDLED
#endif

export module VVEngine.Simple:Window;
import std;
export import VVEngine.Simple.Types;

/// @file
/// @brief Simple window collection, input state and platform windows using the shared facade descriptors.

export namespace vve::simple {

	/// @brief Collection wrapper for all windows created during engine init().
	struct Windows {
		Vector<WindowDesc> value{};																						///< Explicit startup windows; Engine::applyDefaults supplies main when empty.
	};

	/// @brief Keyboard and mouse snapshot; held keys are independent of OS key-repeat speed.
	class InputState {
	public:
		auto beginFrame()																		-> void;
		auto holdKey(std::int32_t keycode)												-> void;
		auto pressKey(std::int32_t keycode)												-> void;
		auto releaseKey(std::int32_t keycode)											-> void;
		auto setMousePosition(WindowHandle window, Vec2 position)					-> void;
		auto addMouseDelta(WindowHandle window, Vec2 delta)							-> void;
		auto addMouseWheelDelta(WindowHandle window, Vec2 delta)					-> void;

		[[nodiscard]] auto isKeyDown(std::int32_t keycode) const					-> bool;
		[[nodiscard]] auto wasKeyPressed(std::int32_t keycode) const				-> bool;
		[[nodiscard]] auto wasKeyReleased(std::int32_t keycode) const			-> bool;
		[[nodiscard]] auto mousePosition(WindowHandle window) const				-> std::optional<Vec2>;
		[[nodiscard]] auto mouseDelta(WindowHandle window) const					-> Vec2;
		[[nodiscard]] auto mouseWheelDelta(WindowHandle window) const			-> Vec2;

	private:
		[[nodiscard]] static auto normalizeKey(std::int32_t keycode)				-> std::int32_t;

		std::set<std::int32_t> keys_down_{};																							///< Keys currently held down.
		std::set<std::int32_t> keys_pressed_{};																						///< Keys pressed this frame.
		std::set<std::int32_t> keys_released_{};																						///< Keys released this frame.
		std::map<WindowHandle, Vec2> mouse_position_{};																				///< Mouse positions by window.
		std::map<WindowHandle, Vec2> mouse_delta_{};																					///< Mouse motion by window.
		std::map<WindowHandle, Vec2> mouse_wheel_delta_{};																			///< Mouse wheel motion by window.
	};

	/// @brief Owned simple platform window implementation.
	class Window {
	public:
		Window(SDL_Window *window, WindowInfo info) noexcept;
		~Window();
		Window(Window &&other) noexcept;
		Window &operator=(Window &&other) noexcept;
		Window(const Window &) = delete;
		Window &operator=(const Window &) = delete;

		[[nodiscard]] SDL_Window *native() const noexcept;
		[[nodiscard]] WindowInfo &info() noexcept;
		[[nodiscard]] const WindowInfo &info() const noexcept;
		[[nodiscard]] auto rendererId() const											-> RendererId;

	private:
		auto reset() noexcept																	-> void;

		SDL_Window *window_{};																												///< Owned SDL window.
		WindowInfo info_{};																													///< Cached public window state.
	};

	/// @brief Owns SDL windows and translates platform events into input and window state.
	class WindowSystem {
	public:
		WindowSystem();																											///< Creates an empty window system.
		~WindowSystem();																														///< Destroys owned SDL windows and shuts down the video subsystem.
		WindowSystem(const WindowSystem &) = delete;																					///< SDL windows cannot be copied safely.
		WindowSystem &operator=(const WindowSystem &) = delete;																	///< SDL windows cannot be copied safely.

		[[nodiscard]] std::string_view name() const noexcept;														///< Returns implementation name.
		[[nodiscard]] std::expected<void, Error> init(const Windows &windows);								///< Creates startup windows.
		[[nodiscard]] std::expected<void, Error> poll();																///< Polls SDL events and updates state.
		[[nodiscard]] InputState &input();																				///< Returns the owned input state.
		[[nodiscard]] const InputState &input() const;																///< Returns the owned input state.
		[[nodiscard]] Vector<std::reference_wrapper<Window>> windows();											///< Owned window refs.
		[[nodiscard]] Vector<std::reference_wrapper<const Window>> windows() const;							///< Const refs.
		[[nodiscard]] std::size_t windowCount() const;																///< Returns owned window count.
		[[nodiscard]] Window *findWindow(std::string_view id);														///< Finds a window by application id.
		[[nodiscard]] const Window *findWindow(std::string_view id) const;										///< Finds a const window by id.
		[[nodiscard]] Window *findWindow(WindowHandle handle);														///< Finds a window by runtime handle.
		[[nodiscard]] const Window *findWindow(WindowHandle handle) const;										///< Finds a const window.
		[[nodiscard]] bool anyShouldClose() const;																		///< Returns true when any window should close.
		auto setGuiEventSink(std::function<bool(const SDL_Event &)> sink)					-> void;		///< Sets GUI event forwarding; the sink returns true for events the GUI claims.

	private:
		std::function<bool(const SDL_Event &)> guiEventSink_{};																		///< SDL event sink for GUI input; true = claimed by the GUI.
		struct Impl;																															///< SDL-owning implementation hidden from module importers.
		std::unique_ptr<Impl> impl_;																										///< Pimpl keeps SDL headers out of the public simple module.
	};

} // namespace vve::simple

export namespace vve::simple {

	auto InputState::beginFrame()															-> void{
		keys_pressed_.clear();
		keys_released_.clear();
		mouse_delta_.clear();
		mouse_wheel_delta_.clear();
	}

	void InputState::holdKey(std::int32_t keycode) { keys_down_.insert(normalizeKey(keycode)); }

	auto InputState::pressKey(std::int32_t keycode)									-> void{
		const auto key = normalizeKey(keycode);
		if (!keys_down_.contains(key)) { keys_pressed_.insert(key); }
		keys_down_.insert(key);
	}

	/// @brief Marks a held key as released; a key pressed and released within one poll still reports wasKeyPressed().
	auto InputState::releaseKey(std::int32_t keycode)								-> void{
		const auto key = normalizeKey(keycode);
		if (keys_down_.erase(key) > 0U) { keys_released_.insert(key); } // Keys the GUI swallowed were never down.
	}

	void InputState::setMousePosition(WindowHandle window, Vec2 position) { mouse_position_[window] = position; }

	auto InputState::addMouseDelta(WindowHandle window, Vec2 delta)				-> void{
		const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
		it->second = math::add(it->second, delta);
	}

	auto InputState::addMouseWheelDelta(WindowHandle window, Vec2 delta)		-> void{
		const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
		it->second = math::add(it->second, delta);
	}

	bool InputState::isKeyDown(std::int32_t keycode) const { return keys_down_.contains(normalizeKey(keycode)); }

	auto InputState::wasKeyPressed(std::int32_t keycode) const					-> bool{
		return keys_pressed_.contains(normalizeKey(keycode));
	}

	auto InputState::wasKeyReleased(std::int32_t keycode) const					-> bool{
		return keys_released_.contains(normalizeKey(keycode));
	}

	auto InputState::mousePosition(WindowHandle window) const						-> std::optional<Vec2>{
		const auto it = mouse_position_.find(window);
		return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
	}

	auto InputState::mouseDelta(WindowHandle window) const							-> Vec2{
		const auto it = mouse_delta_.find(window);
		return it == mouse_delta_.end() ? Vec2{} : it->second;
	}

	auto InputState::mouseWheelDelta(WindowHandle window) const					-> Vec2{
		const auto it = mouse_wheel_delta_.find(window);
		return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
	}

	auto InputState::normalizeKey(std::int32_t keycode)								-> std::int32_t{
		if (keycode >= static_cast<std::int32_t>('A') && keycode <= static_cast<std::int32_t>('Z')) {
			return keycode - static_cast<std::int32_t>('A') + static_cast<std::int32_t>('a');
		}
		return keycode;
	}

	Window::Window(SDL_Window *window, WindowInfo info) noexcept
			: window_{window}, info_{std::move(info)} {}

	Window::~Window() { reset(); }

	Window::Window(Window &&other) noexcept
			: window_{std::exchange(other.window_, nullptr)},
			info_{std::move(other.info_)} {}

	Window &Window::operator=(Window &&other) noexcept {
		if (this != std::addressof(other)) {
			reset();
			window_ = std::exchange(other.window_, nullptr);
			info_ = std::move(other.info_);
		}
		return *this;
	}

	SDL_Window *Window::native() const noexcept { return window_; }

	WindowInfo &Window::info() noexcept { return info_; }

	const WindowInfo &Window::info() const noexcept { return info_; }

	RendererId Window::rendererId() const { return info_.renderer_id; }

	auto Window::reset() noexcept															-> void{
		if (window_ != nullptr) {
			SDL_DestroyWindow(window_);
			window_ = nullptr;
		}
	}

	/// @brief Hidden implementation that stores owned simple window implementations.
	struct WindowSystem::Impl {
		/// @brief Destroys SDL windows and tears down video if this object initialized it.
		~Impl();

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
		auto closeAll()																						-> void{
			for (auto &window : windows) { window.info().should_close = true; }
		}

		bool video_initialized{false};																									///< True after SDL video init succeeds.
		InputState input{};																													///< Keyboard and mouse state produced by polling.
		std::vector<Window> windows{};																									///< Owned window implementations.
		std::map<SDL_WindowID, std::size_t> indices{};																				///< SDL id to window index.
	};

	WindowSystem::Impl::~Impl() {
		windows.clear();
		if (video_initialized) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
	}

	WindowSystem::WindowSystem() : impl_{std::make_unique<Impl>()} {}

	WindowSystem::~WindowSystem() {}

	std::string_view WindowSystem::name() const noexcept { return "SDL3WindowSystem"; }

	InputState &WindowSystem::input() { return impl_->input; }

	const InputState &WindowSystem::input() const { return impl_->input; }

	auto WindowSystem::setGuiEventSink(std::function<bool(const SDL_Event &)> sink)	-> void{
		guiEventSink_ = std::move(sink);
	}

	/// @brief Validates startup descriptors and commits all windows together; failed setup can be retried.
	auto WindowSystem::init(const Windows &windows)									-> std::expected<void, Error>{
		if (impl_->video_initialized || windowCount() > 0) { return std::unexpected(Error::already_initialized); }
		std::set<std::string_view> ids{};
		// Reject the entire list before any SDL call can acquire platform resources.
		for (const auto &desc : windows.value) {
			if (desc.id.empty() || desc.extent.width == 0 || desc.extent.height == 0) {
				return std::unexpected(Error::invalid_argument);
			}
			if (!ids.insert(desc.id).second) { return std::unexpected(Error::duplicate_object); }
		}

		std::vector<Window> created_windows{};				///< Owns pending windows until the whole list succeeds.
		std::map<SDL_WindowID, std::size_t> indices{};		///< SDL ids index the pending window list.
		bool video_initialized = false;						///< Tracks the SDL video reference acquired by this call.
		created_windows.reserve(windows.value.size());
		if (!windows.value.empty()) {
			SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
			SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
				std::cerr << "[vve::simple] SDL video init failed: " << SDL_GetError() << '\n';
				return std::unexpected(Error::platform_error);
			}
			video_initialized = true;
		}

		// Keep each new window local so a later failure leaves the system empty.
		for (const auto &desc : windows.value) {
			auto info = WindowInfo{.handle = makeCounterHandle<WindowHandle>(),
											.id = desc.id,
											.title = desc.title,
											.extent = desc.extent,
											.renderer_id = desc.renderer_id,
											.focused = false,
											.minimized = false,
											.should_close = false};
			SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
			if (desc.resizable) { flags |= SDL_WINDOW_RESIZABLE; }
			if (!desc.visible) { flags |= SDL_WINDOW_HIDDEN; }

			SDL_Window *const window = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.extent.width),
																		static_cast<int>(desc.extent.height), flags);
			if (window == nullptr) {
				std::cerr << "[vve::simple] SDL window creation failed: " << SDL_GetError() << '\n';
				// Run window destructors before releasing this call's SDL video reference.
				created_windows.clear();
				if (video_initialized) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
				return std::unexpected(Error::platform_error);
			}

			if (desc.x.has_value() || desc.y.has_value()) {
				int x = 0;
				int y = 0;
				SDL_GetWindowPosition(window, &x, &y);
				SDL_SetWindowPosition(window, desc.x.value_or(x), desc.y.value_or(y));
			}

			// SDL sizes windows in screen coordinates; the renderer and WindowInfo use pixels (2x on most HiDPI screens).
			int width = 0;
			int height = 0;
			SDL_GetWindowSizeInPixels(window, &width, &height);

			info.extent = PixelExtent{.width = static_cast<std::uint32_t>(std::max(width, 0)),
												.height = static_cast<std::uint32_t>(std::max(height, 0))};
			info.focused = SDL_GetKeyboardFocus() == window;
			const SDL_WindowID id = SDL_GetWindowID(window);
			indices[id] = created_windows.size();
			created_windows.emplace_back(window, std::move(info));
		}

		// Publish ownership only after every window has been created successfully.
		impl_->windows = std::move(created_windows);
		impl_->indices = std::move(indices);
		impl_->video_initialized = video_initialized;
		return {};
	}

	auto WindowSystem::poll()																-> std::expected<void, Error>{
		auto &input = impl_->input;
		input.beginFrame();
		if (!impl_->video_initialized) { return {}; }
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			// Events the GUI claims (typing into a text field, dragging a slider) do not also move the camera.
			const bool gui_claimed = guiEventSink_ && guiEventSink_(event);
			switch (event.type) {
			case SDL_EVENT_QUIT:
				impl_->closeAll();
				break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (auto *window = impl_->find(event.window.windowID)) { window->info().should_close = true; }
				break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: // Carries pixels; SDL_EVENT_WINDOW_RESIZED carries screen coordinates.
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
				if (!event.key.repeat && !gui_claimed) { input.pressKey(static_cast<std::int32_t>(event.key.key)); }
				break;
			case SDL_EVENT_KEY_UP: // Always delivered, so a key held before the GUI took focus cannot get stuck.
				input.releaseKey(static_cast<std::int32_t>(event.key.key));
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (gui_claimed) { break; }
				if (const auto *window = impl_->find(event.motion.windowID)) {
					/// Normalize in window coordinates so pixel density does not affect mouse input.
					int width{}, height{};
					SDL_GetWindowSize(window->native(), &width, &height);
					input.setMousePosition(window->info().handle, Vec2{event.motion.x / width, event.motion.y / height});
					input.addMouseDelta(window->info().handle, Vec2{event.motion.xrel / width, event.motion.yrel / height});
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				if (gui_claimed) { break; }
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

	auto WindowSystem::windows()															-> Vector<std::reference_wrapper<Window>>{
		Vector<std::reference_wrapper<Window>> result{};
		result.reserve(impl_->windows.size());
		for (auto &window : impl_->windows) { result.push_back(std::ref(window)); }
		return result;
	}

	auto WindowSystem::windows() const													-> Vector<std::reference_wrapper<const Window>>{
		Vector<std::reference_wrapper<const Window>> result{};
		result.reserve(impl_->windows.size());
		for (const auto &window : impl_->windows) { result.push_back(std::cref(window)); }
		return result;
	}

	std::size_t WindowSystem::windowCount() const { return impl_->windows.size(); }

	Window *WindowSystem::findWindow(std::string_view id) { return impl_->find(id); }

	const Window *WindowSystem::findWindow(std::string_view id) const { return impl_->find(id); }

	Window *WindowSystem::findWindow(WindowHandle handle) { return impl_->find(handle); }

	const Window *WindowSystem::findWindow(WindowHandle handle) const { return impl_->find(handle); }

	auto WindowSystem::anyShouldClose() const											-> bool{
		return std::ranges::any_of(impl_->windows, [](const Window &window) {
			return window.info().should_close;
		});
	}

} // namespace vve::simple
