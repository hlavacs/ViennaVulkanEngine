module;

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#define VVE_V5_DEFINED_SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#ifdef VVE_V5_DEFINED_SDL_MAIN_HANDLED
#undef SDL_MAIN_HANDLED
#undef VVE_V5_DEFINED_SDL_MAIN_HANDLED
#endif

#if defined(_WIN32) && defined(VVE_ENGINE_BUILD)
#define VVE_V5_API __declspec(dllexport)
#else
#define VVE_V5_API
#endif

export module VEEngine.V5:Window;
import std;
export import :Types;

/// @file
/// @brief v5 window descriptors, input state, and owned platform window object.

export namespace vve::v5 {

	/// @brief Window creation descriptor consumed by the v5 platform layer.
	struct WindowDesc {
		std::string id{"main"};																												///< Stable application-local window id.
		std::string title{"VVE v5"};																										///< Platform window title.
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
		VVE_V5_API auto beginFrame()																		-> void;
		VVE_V5_API auto holdKey(std::int32_t keycode)												-> void;
		VVE_V5_API auto pressKey(std::int32_t keycode)												-> void;
		VVE_V5_API auto releaseKey(std::int32_t keycode)											-> void;
		VVE_V5_API auto setMousePosition(WindowHandle window, Vec2 position)					-> void;
		VVE_V5_API auto addMouseDelta(WindowHandle window, Vec2 delta)							-> void;
		VVE_V5_API auto addMouseWheelDelta(WindowHandle window, Vec2 delta)					-> void;

		[[nodiscard]] VVE_V5_API auto isKeyDown(std::int32_t keycode) const					-> bool;
		[[nodiscard]] VVE_V5_API auto wasKeyPressed(std::int32_t keycode) const				-> bool;
		[[nodiscard]] VVE_V5_API auto wasKeyReleased(std::int32_t keycode) const			-> bool;
		[[nodiscard]] VVE_V5_API auto mousePosition(WindowHandle window) const				-> std::optional<Vec2>;
		[[nodiscard]] VVE_V5_API auto mouseDelta(WindowHandle window) const					-> Vec2;
		[[nodiscard]] VVE_V5_API auto mouseWheelDelta(WindowHandle window) const			-> Vec2;

	private:
		[[nodiscard]] static VVE_V5_API auto normalizeKey(std::int32_t keycode)				-> std::int32_t;

		std::set<std::int32_t> keys_down_{};																							///< Keys currently held down.
		std::set<std::int32_t> keys_pressed_{};																						///< Keys pressed this frame.
		std::set<std::int32_t> keys_released_{};																						///< Keys released this frame.
		std::map<WindowHandle, Vec2> mouse_position_{};																				///< Mouse positions by window.
		std::map<WindowHandle, Vec2> mouse_delta_{};																					///< Mouse motion by window.
		std::map<WindowHandle, Vec2> mouse_wheel_delta_{};																			///< Mouse wheel motion by window.
	};

	/// @brief Owned v5 platform window implementation.
	class Window {
	public:
		VVE_V5_API Window(SDL_Window *window, SDL_WindowID sdl_id, WindowInfo info) noexcept;
		VVE_V5_API ~Window();
		VVE_V5_API Window(Window &&other) noexcept;
		VVE_V5_API Window &operator=(Window &&other) noexcept;
		Window(const Window &) = delete;
		Window &operator=(const Window &) = delete;

		[[nodiscard]] VVE_V5_API SDL_Window *native() const noexcept;
		[[nodiscard]] VVE_V5_API auto sdlId() const noexcept										-> SDL_WindowID;
		[[nodiscard]] VVE_V5_API WindowInfo &info() noexcept;
		[[nodiscard]] VVE_V5_API const WindowInfo &info() const noexcept;
		[[nodiscard]] VVE_V5_API auto handle() const noexcept										-> WindowHandle;
		[[nodiscard]] VVE_V5_API auto id() const noexcept											-> std::string_view;
		[[nodiscard]] VVE_V5_API auto title() const noexcept										-> std::string_view;
		[[nodiscard]] VVE_V5_API auto extent() const noexcept										-> PixelExtent;
		[[nodiscard]] VVE_V5_API auto rendererId() const											-> RendererId;
		[[nodiscard]] VVE_V5_API auto camera() const													-> std::optional<Entity>;
		[[nodiscard]] VVE_V5_API auto focused() const noexcept									-> bool;
		[[nodiscard]] VVE_V5_API auto minimized() const noexcept									-> bool;
		[[nodiscard]] VVE_V5_API auto shouldClose() const noexcept								-> bool;

	private:
		VVE_V5_API auto reset() noexcept																	-> void;

		SDL_Window *window_{};																												///< Owned SDL window.
		SDL_WindowID sdl_id_{};																												///< SDL-local window id.
		WindowInfo info_{};																													///< Cached public window state.
	};

	/// @brief Owns SDL windows and translates platform events into input and window state.
	class WindowSystem {
	public:
		VVE_V5_API WindowSystem();																											///< Creates an empty window system.
		~WindowSystem();																														///< Destroys owned SDL windows and shuts down the video subsystem.
		WindowSystem(WindowSystem &&) noexcept;																						///< Moves the window system and owned implementation.
		WindowSystem &operator=(WindowSystem &&) noexcept;																			///< Moves the window system and owned implementation.
		WindowSystem(const WindowSystem &) = delete;																					///< SDL windows cannot be copied safely.
		WindowSystem &operator=(const WindowSystem &) = delete;																	///< SDL windows cannot be copied safely.

		[[nodiscard]] VVE_V5_API std::string_view name() const noexcept;														///< Returns implementation name.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> init(const Windows &windows);								///< Creates startup windows.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> poll();																///< Polls SDL events and updates state.
		[[nodiscard]] VVE_V5_API InputState &input();																				///< Returns the owned input state.
		[[nodiscard]] VVE_V5_API const InputState &input() const;																///< Returns the owned input state.
		[[nodiscard]] VVE_V5_API Vector<WindowInfo> snapshot() const;															///< Returns current window states.
		[[nodiscard]] VVE_V5_API Vector<std::reference_wrapper<Window>> windows();											///< Owned window refs.
		[[nodiscard]] VVE_V5_API Vector<std::reference_wrapper<const Window>> windows() const;							///< Const refs.
		[[nodiscard]] VVE_V5_API std::size_t windowCount() const;																///< Returns owned window count.
		[[nodiscard]] VVE_V5_API Window *findWindow(std::string_view id);														///< Finds a window by application id.
		[[nodiscard]] VVE_V5_API const Window *findWindow(std::string_view id) const;										///< Finds a const window by id.
		[[nodiscard]] VVE_V5_API Window *findWindow(WindowHandle handle);														///< Finds a window by runtime handle.
		[[nodiscard]] VVE_V5_API const Window *findWindow(WindowHandle handle) const;										///< Finds a const window.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> setWindowCamera(WindowHandle window, Entity camera);	///< Sets camera.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> setWindowCamera(std::string_view id, Entity camera);	///< Sets camera.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> clearWindowCamera(WindowHandle window);					///< Clears camera.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> clearWindowCamera(std::string_view id);					///< Clears camera.
		[[nodiscard]] VVE_V5_API std::optional<Entity> windowCamera(WindowHandle window) const;						///< Selected camera.
		[[nodiscard]] VVE_V5_API std::optional<Entity> windowCamera(std::string_view id) const;						///< Selected camera.
		[[nodiscard]] VVE_V5_API std::expected<void, Error> setActiveCamera(Entity camera);								///< Assigns camera.
		[[nodiscard]] VVE_V5_API std::optional<Entity> activeCamera() const;													///< Returns the first selected camera.
		[[nodiscard]] VVE_V5_API bool anyShouldClose() const;																		///< Returns true when any window should close.

	private:
		template <typename TKey, typename TFunction>
		[[nodiscard]] auto editWindow(TKey key, TFunction function)								-> std::expected<void, Error>;
		template <typename TKey> [[nodiscard]] std::optional<Entity> cameraFor(TKey key) const;

		struct Impl;																															///< SDL-owning implementation hidden from module importers.
		std::unique_ptr<Impl> impl_;																										///< Pimpl keeps SDL headers out of the public v5 module.
	};

} // namespace vve::v5

export namespace vve::v5 {

	VVE_V5_API auto InputState::beginFrame()															-> void{
		keys_pressed_.clear();
		keys_released_.clear();
		mouse_delta_.clear();
		mouse_wheel_delta_.clear();
	}

	VVE_V5_API void InputState::holdKey(std::int32_t keycode) { keys_down_.insert(normalizeKey(keycode)); }

	VVE_V5_API auto InputState::pressKey(std::int32_t keycode)									-> void{
		const auto key = normalizeKey(keycode);
		if (!keys_down_.contains(key)) { keys_pressed_.insert(key); }
		keys_down_.insert(key);
	}

	VVE_V5_API auto InputState::releaseKey(std::int32_t keycode)								-> void{
		const auto key = normalizeKey(keycode);
		keys_down_.erase(key);
		keys_pressed_.erase(key);
		keys_released_.insert(key);
	}

	VVE_V5_API void InputState::setMousePosition(WindowHandle window, Vec2 position) { mouse_position_[window] = position; }

	VVE_V5_API auto InputState::addMouseDelta(WindowHandle window, Vec2 delta)				-> void{
		const auto [it, _] = mouse_delta_.try_emplace(window, Vec2{zero(), zero()});
		it->second = math::add(it->second, delta);
	}

	VVE_V5_API auto InputState::addMouseWheelDelta(WindowHandle window, Vec2 delta)		-> void{
		const auto [it, _] = mouse_wheel_delta_.try_emplace(window, Vec2{zero(), zero()});
		it->second = math::add(it->second, delta);
	}

	VVE_V5_API bool InputState::isKeyDown(std::int32_t keycode) const { return keys_down_.contains(normalizeKey(keycode)); }

	VVE_V5_API auto InputState::wasKeyPressed(std::int32_t keycode) const					-> bool{
		return keys_pressed_.contains(normalizeKey(keycode));
	}

	VVE_V5_API auto InputState::wasKeyReleased(std::int32_t keycode) const					-> bool{
		return keys_released_.contains(normalizeKey(keycode));
	}

	VVE_V5_API auto InputState::mousePosition(WindowHandle window) const						-> std::optional<Vec2>{
		const auto it = mouse_position_.find(window);
		return it == mouse_position_.end() ? std::optional<Vec2>{} : std::optional<Vec2>{it->second};
	}

	VVE_V5_API auto InputState::mouseDelta(WindowHandle window) const							-> Vec2{
		const auto it = mouse_delta_.find(window);
		return it == mouse_delta_.end() ? Vec2{} : it->second;
	}

	VVE_V5_API auto InputState::mouseWheelDelta(WindowHandle window) const					-> Vec2{
		const auto it = mouse_wheel_delta_.find(window);
		return it == mouse_wheel_delta_.end() ? Vec2{} : it->second;
	}

	VVE_V5_API auto InputState::normalizeKey(std::int32_t keycode)								-> std::int32_t{
		if (keycode >= static_cast<std::int32_t>('A') && keycode <= static_cast<std::int32_t>('Z')) {
			return keycode - static_cast<std::int32_t>('A') + static_cast<std::int32_t>('a');
		}
		return keycode;
	}

	VVE_V5_API Window::Window(SDL_Window *window, SDL_WindowID sdl_id, WindowInfo info) noexcept
			: window_{window}, sdl_id_{sdl_id}, info_{std::move(info)} {}

	VVE_V5_API Window::~Window() { reset(); }

	VVE_V5_API Window::Window(Window &&other) noexcept
			: window_{std::exchange(other.window_, nullptr)},
			sdl_id_{std::exchange(other.sdl_id_, 0)},
			info_{std::move(other.info_)} {}

	VVE_V5_API Window &Window::operator=(Window &&other) noexcept {
		if (this != std::addressof(other)) {
			reset();
			window_ = std::exchange(other.window_, nullptr);
			sdl_id_ = std::exchange(other.sdl_id_, 0);
			info_ = std::move(other.info_);
		}
		return *this;
	}

	VVE_V5_API SDL_Window *Window::native() const noexcept { return window_; }

	VVE_V5_API SDL_WindowID Window::sdlId() const noexcept { return sdl_id_; }

	VVE_V5_API WindowInfo &Window::info() noexcept { return info_; }

	VVE_V5_API const WindowInfo &Window::info() const noexcept { return info_; }

	VVE_V5_API WindowHandle Window::handle() const noexcept { return info_.handle; }

	VVE_V5_API std::string_view Window::id() const noexcept { return info_.id; }

	VVE_V5_API std::string_view Window::title() const noexcept { return info_.title; }

	VVE_V5_API PixelExtent Window::extent() const noexcept { return info_.extent; }

	VVE_V5_API RendererId Window::rendererId() const { return info_.renderer_id; }

	VVE_V5_API std::optional<Entity> Window::camera() const { return info_.camera; }

	VVE_V5_API bool Window::focused() const noexcept { return info_.focused; }

	VVE_V5_API bool Window::minimized() const noexcept { return info_.minimized; }

	VVE_V5_API bool Window::shouldClose() const noexcept { return info_.should_close; }

	VVE_V5_API auto Window::reset() noexcept															-> void{
		if (window_ != nullptr) {
			SDL_DestroyWindow(window_);
			window_ = nullptr;
		}
	}

	/// @brief Hidden implementation that stores owned v5 window implementations.
	struct WindowSystem::Impl {
		/// @brief Destroys SDL windows and tears down video if this object initialized it.
		VVE_V5_API ~Impl();

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

	VVE_V5_API WindowSystem::Impl::~Impl() {
		windows.clear();
		if (video_initialized) { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
	}

	VVE_V5_API WindowSystem::WindowSystem() : impl_{std::make_unique<Impl>()} {}

	WindowSystem::~WindowSystem() = default;

	WindowSystem::WindowSystem(WindowSystem &&) noexcept = default;

	WindowSystem &WindowSystem::operator=(WindowSystem &&) noexcept = default;

	VVE_V5_API std::string_view WindowSystem::name() const noexcept { return "SDL3WindowSystem"; }

	VVE_V5_API InputState &WindowSystem::input() { return impl_->input; }

	VVE_V5_API const InputState &WindowSystem::input() const { return impl_->input; }

	VVE_V5_API auto WindowSystem::init(const Windows &windows)									-> std::expected<void, Error>{
		const auto needs_platform_windows = std::ranges::any_of(windows.value, [](const WindowDesc &desc) {
			return desc.visible;
		});
		if (needs_platform_windows) {
			SDL_SetMainReady();
#ifdef VVE_SDL_VULKAN_LIBRARY
			SDL_SetHint(SDL_HINT_VULKAN_LIBRARY, VVE_SDL_VULKAN_LIBRARY);
#endif
			if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) { return std::unexpected(Error::platform_error); }
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

			info.extent = PixelExtent{.width = static_cast<std::uint32_t>(std::max(width, 0)),
												.height = static_cast<std::uint32_t>(std::max(height, 0))};
			info.focused = SDL_GetKeyboardFocus() == window;
			const SDL_WindowID id = SDL_GetWindowID(window);
			impl_->indices[id] = impl_->windows.size();
			impl_->windows.emplace_back(window, id, std::move(info));
		}

		return {};
	}

	VVE_V5_API auto WindowSystem::poll()																-> std::expected<void, Error>{
		auto &input = impl_->input;
		input.beginFrame();
		if (!impl_->video_initialized) { return {}; }
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

	VVE_V5_API auto WindowSystem::snapshot() const													-> Vector<WindowInfo>{
		Vector<WindowInfo> result{};
		result.reserve(impl_->windows.size());
		for (const auto &window : impl_->windows) { result.push_back(window.info()); }
		return result;
	}

	VVE_V5_API auto WindowSystem::windows()															-> Vector<std::reference_wrapper<Window>>{
		Vector<std::reference_wrapper<Window>> result{};
		result.reserve(impl_->windows.size());
		for (auto &window : impl_->windows) { result.push_back(std::ref(window)); }
		return result;
	}

	VVE_V5_API auto WindowSystem::windows() const													-> Vector<std::reference_wrapper<const Window>>{
		Vector<std::reference_wrapper<const Window>> result{};
		result.reserve(impl_->windows.size());
		for (const auto &window : impl_->windows) { result.push_back(std::cref(window)); }
		return result;
	}

	VVE_V5_API std::size_t WindowSystem::windowCount() const { return impl_->windows.size(); }

	VVE_V5_API Window *WindowSystem::findWindow(std::string_view id) { return impl_->find(id); }

	VVE_V5_API const Window *WindowSystem::findWindow(std::string_view id) const { return impl_->find(id); }

	VVE_V5_API Window *WindowSystem::findWindow(WindowHandle handle) { return impl_->find(handle); }

	VVE_V5_API const Window *WindowSystem::findWindow(WindowHandle handle) const { return impl_->find(handle); }

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

	VVE_V5_API auto WindowSystem::setWindowCamera(WindowHandle window, Entity camera)	-> std::expected<void, Error>{
		return editWindow(window, [camera](Window &selected) { selected.info().camera = camera; });
	}

	VVE_V5_API auto WindowSystem::setWindowCamera(std::string_view id, Entity camera)	-> std::expected<void, Error>{
		return editWindow(id, [camera](Window &selected) { selected.info().camera = camera; });
	}

	VVE_V5_API auto WindowSystem::clearWindowCamera(WindowHandle window)						-> std::expected<void, Error>{
		return editWindow(window, [](Window &selected) { selected.info().camera.reset(); });
	}

	VVE_V5_API auto WindowSystem::clearWindowCamera(std::string_view id)						-> std::expected<void, Error>{
		return editWindow(id, [](Window &selected) { selected.info().camera.reset(); });
	}

	VVE_V5_API std::optional<Entity> WindowSystem::windowCamera(WindowHandle window) const { return cameraFor(window); }

	VVE_V5_API std::optional<Entity> WindowSystem::windowCamera(std::string_view id) const { return cameraFor(id); }

	VVE_V5_API auto WindowSystem::setActiveCamera(Entity camera)								-> std::expected<void, Error>{
		if (impl_->windows.empty()) { return std::unexpected(Error::missing_object); }
		for (auto &window : impl_->windows) { window.info().camera = camera; }
		return {};
	}

	VVE_V5_API auto WindowSystem::activeCamera() const												-> std::optional<Entity>{
		const auto it = std::ranges::find_if(impl_->windows, [](const Window &window) {
			return window.info().camera.has_value();
		});
		return it == impl_->windows.end() ? std::optional<Entity>{} : it->info().camera;
	}

	VVE_V5_API auto WindowSystem::anyShouldClose() const											-> bool{
		return std::ranges::any_of(impl_->windows, [](const Window &window) {
			return window.info().should_close;
		});
	}

} // namespace vve::v5
