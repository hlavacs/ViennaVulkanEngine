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

export module VEEngine.Simple:Window;
import std;
export import VEEngine.Simple.Types;

/// @file
/// @brief simple window descriptors, input state, and owned platform window object.

export namespace vve::simple {

	/// @brief Window creation descriptor consumed by the simple platform layer.
	struct WindowDesc {
		std::string id{"main"};																												///< Stable application-local window id.
		std::string title{"VVE simple"};																										///< Platform window title.
		PixelExtent extent{.width = 960, .height = 540};																			///< Initial pixel dimensions.
		std::optional<int> x{};																												///< Optional initial screen x coordinate.
		std::optional<int> y{};																												///< Optional initial screen y coordinate.
		RendererId renderer_id{};																											///< Renderer id selected for this window.
		bool resizable{true};																												///< Enables platform resizing.
		bool visible{true};																													///< Shows the window after creation.
	};

	/// @brief Collection wrapper for all windows created during engine init().
	struct Windows {
		Vector<WindowDesc> value{WindowDesc{}};																						///< Startup windows; defaults to one main window.
	};

	/// @brief Runtime window state exposed through the facade window wrapper.
	struct WindowInfo {
		WindowHandle handle{};																												///< 64-bit runtime window handle.
		std::string id{};																														///< Stable id copied from WindowDesc.
		std::string title{};																													///< Current platform title.
		PixelExtent extent{};																												///< Current pixel dimensions.
		RendererId renderer_id{};																											///< Renderer id selected for this window.
		std::optional<Entity> camera{};																									///< Camera entity rendered through this window, when selected.
		bool focused{false};																													///< True while the window has keyboard focus.
		bool minimized{false};																												///< True while the platform reports a minimized window.
		bool should_close{false};																											///< True after a close request.
	};

	/// @brief Snapshot passed to user systems that want window data for the current frame.
	struct WindowFrameData {
		Vector<WindowInfo> windows{};																										///< Window states after event polling.
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
		Window(SDL_Window *window, SDL_WindowID sdl_id, WindowInfo info) noexcept;
		~Window();
		Window(Window &&other) noexcept;
		Window &operator=(Window &&other) noexcept;
		Window(const Window &) = delete;
		Window &operator=(const Window &) = delete;

		[[nodiscard]] SDL_Window *native() const noexcept;
		[[nodiscard]] auto sdlId() const noexcept										-> SDL_WindowID;
		[[nodiscard]] WindowInfo &info() noexcept;
		[[nodiscard]] const WindowInfo &info() const noexcept;
		[[nodiscard]] auto handle() const noexcept										-> WindowHandle;
		[[nodiscard]] auto id() const noexcept											-> std::string_view;
		[[nodiscard]] auto title() const noexcept										-> std::string_view;
		[[nodiscard]] auto extent() const noexcept										-> PixelExtent;
		[[nodiscard]] auto rendererId() const											-> RendererId;
		[[nodiscard]] auto camera() const													-> std::optional<Entity>;
		[[nodiscard]] auto focused() const noexcept									-> bool;
		[[nodiscard]] auto minimized() const noexcept									-> bool;
		[[nodiscard]] auto shouldClose() const noexcept								-> bool;

	private:
		auto reset() noexcept																	-> void;

		SDL_Window *window_{};																												///< Owned SDL window.
		SDL_WindowID sdl_id_{};																												///< SDL-local window id.
		WindowInfo info_{};																													///< Cached public window state.
	};

	/// @brief Owns SDL windows and translates platform events into input and window state.
	class WindowSystem {
	public:
		WindowSystem();																											///< Creates an empty window system.
		~WindowSystem();																														///< Destroys owned SDL windows and shuts down the video subsystem.
		WindowSystem(WindowSystem &&) noexcept;																						///< Moves the window system and owned implementation.
		WindowSystem &operator=(WindowSystem &&) noexcept;																			///< Moves the window system and owned implementation.
		WindowSystem(const WindowSystem &) = delete;																					///< SDL windows cannot be copied safely.
		WindowSystem &operator=(const WindowSystem &) = delete;																	///< SDL windows cannot be copied safely.

		[[nodiscard]] std::string_view name() const noexcept;														///< Returns implementation name.
		[[nodiscard]] std::expected<void, Error> init(const Windows &windows);								///< Creates startup windows.
		[[nodiscard]] std::expected<void, Error> poll();																///< Polls SDL events and updates state.
		[[nodiscard]] InputState &input();																				///< Returns the owned input state.
		[[nodiscard]] const InputState &input() const;																///< Returns the owned input state.
		[[nodiscard]] Vector<WindowInfo> snapshot() const;															///< Returns current window states.
		[[nodiscard]] Vector<std::reference_wrapper<Window>> windows();											///< Owned window refs.
		[[nodiscard]] Vector<std::reference_wrapper<const Window>> windows() const;							///< Const refs.
		[[nodiscard]] std::size_t windowCount() const;																///< Returns owned window count.
		[[nodiscard]] Window *findWindow(std::string_view id);														///< Finds a window by application id.
		[[nodiscard]] const Window *findWindow(std::string_view id) const;										///< Finds a const window by id.
		[[nodiscard]] Window *findWindow(WindowHandle handle);														///< Finds a window by runtime handle.
		[[nodiscard]] const Window *findWindow(WindowHandle handle) const;										///< Finds a const window.
		[[nodiscard]] std::expected<void, Error> setWindowCamera(WindowHandle window, Entity camera);	///< Sets camera.
		[[nodiscard]] std::expected<void, Error> setWindowCamera(std::string_view id, Entity camera);	///< Sets camera.
		[[nodiscard]] std::expected<void, Error> clearWindowCamera(WindowHandle window);					///< Clears camera.
		[[nodiscard]] std::expected<void, Error> clearWindowCamera(std::string_view id);					///< Clears camera.
		[[nodiscard]] std::optional<Entity> windowCamera(WindowHandle window) const;						///< Selected camera.
		[[nodiscard]] std::optional<Entity> windowCamera(std::string_view id) const;						///< Selected camera.
		[[nodiscard]] std::expected<void, Error> setActiveCamera(Entity camera);								///< Assigns camera.
		[[nodiscard]] std::optional<Entity> activeCamera() const;													///< Returns the first selected camera.
		[[nodiscard]] bool anyShouldClose() const;																		///< Returns true when any window should close.
		auto setGuiEventSink(std::function<void(const SDL_Event &)> sink)					-> void;		///< Sets optional GUI event forwarding.

	private:
		template <typename TKey, typename TFunction>
		[[nodiscard]] auto editWindow(TKey key, TFunction function)								-> std::expected<void, Error>;
		template <typename TKey> [[nodiscard]] std::optional<Entity> cameraFor(TKey key) const;

		std::function<void(const SDL_Event &)> guiEventSink_{};																		///< Non-owning SDL event sink for GUI input.
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

	auto InputState::releaseKey(std::int32_t keycode)								-> void{
		const auto key = normalizeKey(keycode);
		keys_down_.erase(key);
		keys_pressed_.erase(key);
		keys_released_.insert(key);
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
		Vector<Window> windows{};																											///< Owned window implementations.
		std::map<SDL_WindowID, std::size_t> indices{};																				///< SDL id to window index.
	};

	WindowSystem::Impl::~Impl() {
		windows.clear();
		if (video_initialized) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
	}

	WindowSystem::WindowSystem() : impl_{std::make_unique<Impl>()} {}

	WindowSystem::~WindowSystem() {}

	WindowSystem::WindowSystem(WindowSystem &&other) noexcept
		: guiEventSink_{std::move(other.guiEventSink_)}, impl_{std::move(other.impl_)} {}

	WindowSystem &WindowSystem::operator=(WindowSystem &&other) noexcept {
		guiEventSink_ = std::move(other.guiEventSink_);
		impl_ = std::move(other.impl_);
		return *this;
	}

	std::string_view WindowSystem::name() const noexcept { return "SDL3WindowSystem"; }

	InputState &WindowSystem::input() { return impl_->input; }

	const InputState &WindowSystem::input() const { return impl_->input; }

	auto WindowSystem::setGuiEventSink(std::function<void(const SDL_Event &)> sink)	-> void{
		guiEventSink_ = std::move(sink);
	}

	auto WindowSystem::init(const Windows &windows)									-> std::expected<void, Error>{
		const auto needs_platform_windows = std::ranges::any_of(windows.value, [](const WindowDesc &desc) {
			return desc.visible;
		});
		if (needs_platform_windows) {
			SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
			SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
				std::cerr << "[vve::simple] SDL video init failed: " << SDL_GetError() << '\n';
				return std::unexpected(Error::platform_error);
			}
			impl_->video_initialized = true;
		}

		for (const auto &desc : windows.value) {
			auto info = WindowInfo{.handle = makeCounterHandle<WindowHandle>(),
											.id = desc.id,
											.title = desc.title,
											.extent = desc.extent,
											.renderer_id = desc.renderer_id,
											.focused = false,
											.minimized = false,
											.should_close = false};
			if (!desc.visible) {
				impl_->windows.emplace_back(nullptr, 0, std::move(info));
				continue;
			}

			SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
			if (desc.resizable) { flags |= SDL_WINDOW_RESIZABLE; }

			SDL_Window *const window = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.extent.width),
																		static_cast<int>(desc.extent.height), flags);
			if (window == nullptr) {
				std::cerr << "[vve::simple] SDL window creation failed: " << SDL_GetError() << '\n';
				return std::unexpected(Error::platform_error);
			}

			if (desc.x.has_value() || desc.y.has_value()) {
				int x = 0;
				int y = 0;
				SDL_GetWindowPosition(window, &x, &y);
				SDL_SetWindowPosition(window, desc.x.value_or(x), desc.y.value_or(y));
			}

			int width = 0;
			int height = 0;
			SDL_GetWindowSize(window, &width, &height);

			info.extent = PixelExtent{.width = static_cast<std::uint32_t>(std::max(width, 0)),
												.height = static_cast<std::uint32_t>(std::max(height, 0))};
			info.focused = SDL_GetKeyboardFocus() == window;
			const SDL_WindowID id = SDL_GetWindowID(window);
			impl_->indices[id] = impl_->windows.size();
			impl_->windows.emplace_back(window, id, std::move(info));
		}

		return {};
	}

	auto WindowSystem::poll()																-> std::expected<void, Error>{
		auto &input = impl_->input;
		input.beginFrame();
		if (!impl_->video_initialized) { return {}; }
		SDL_Event event{};
		while (SDL_PollEvent(&event)) {
			if (guiEventSink_) { guiEventSink_(event); }
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

	auto WindowSystem::snapshot() const													-> Vector<WindowInfo>{
		Vector<WindowInfo> result{};
		result.reserve(impl_->windows.size());
		for (const auto &window : impl_->windows) { result.push_back(window.info()); }
		return result;
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

	/// @brief Edits one selected window or reports an invalid selector.
	template <typename TKey, typename TFunction>
	auto WindowSystem::editWindow(TKey key, TFunction function)									-> std::expected<void, Error>{
		auto *window = impl_->find(key);
		if (window == nullptr) { return std::unexpected(Error::invalid_handle); }
		std::invoke(std::move(function), *window);
		return {};
	}

	/// @brief Returns the selected camera for one window selector.
	template <typename TKey> std::optional<Entity> WindowSystem::cameraFor(TKey key) const {
		const auto *window = impl_->find(key);
		return window == nullptr ? std::optional<Entity>{} : window->info().camera;
	}

	auto WindowSystem::setWindowCamera(WindowHandle window, Entity camera)	-> std::expected<void, Error>{
		return editWindow(window, [camera](Window &selected) { selected.info().camera = camera; });
	}

	auto WindowSystem::setWindowCamera(std::string_view id, Entity camera)	-> std::expected<void, Error>{
		return editWindow(id, [camera](Window &selected) { selected.info().camera = camera; });
	}

	auto WindowSystem::clearWindowCamera(WindowHandle window)						-> std::expected<void, Error>{
		return editWindow(window, [](Window &selected) { selected.info().camera.reset(); });
	}

	auto WindowSystem::clearWindowCamera(std::string_view id)						-> std::expected<void, Error>{
		return editWindow(id, [](Window &selected) { selected.info().camera.reset(); });
	}

	std::optional<Entity> WindowSystem::windowCamera(WindowHandle window) const { return cameraFor(window); }

	std::optional<Entity> WindowSystem::windowCamera(std::string_view id) const { return cameraFor(id); }

	auto WindowSystem::setActiveCamera(Entity camera)								-> std::expected<void, Error>{
		if (impl_->windows.empty()) { return std::unexpected(Error::missing_object); }
		for (auto &window : impl_->windows) { window.info().camera = camera; }
		return {};
	}

	auto WindowSystem::activeCamera() const												-> std::optional<Entity>{
		const auto it = std::ranges::find_if(impl_->windows, [](const Window &window) {
			return window.info().camera.has_value();
		});
		return it == impl_->windows.end() ? std::optional<Entity>{} : it->info().camera;
	}

	auto WindowSystem::anyShouldClose() const											-> bool{
		return std::ranges::any_of(impl_->windows, [](const Window &window) {
			return window.info().should_close;
		});
	}

} // namespace vve::simple
