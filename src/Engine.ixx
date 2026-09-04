module;

export module VEEngine;
import std;
import VEEngine.Simple;
export import VEEngine.Error;
export import VEEngine.Math;
export import VEEngine.Handle;
export import VEEngine.Vector;
export import VEEngine.Types;
export import :ECS;
export import :Window;
export import :World;
export import :Assets;
export import :RenderSystem;
export import :Gui;

/// @file
/// @brief Public engine facade; users import this module and use only namespace vve.

export namespace vve {

	inline constexpr std::string_view engineImplementationNamespaceName{"simple"};	///< Active implementation namespace name.

	struct WindowFrameInfo {
		WindowHandle handle{};								///< Runtime window handle.
		std::string id{};									///< Stable application-local window id.
		std::string title{};								///< Platform window title.
		PixelExtent extent{};								///< Current pixel dimensions.
		RendererId renderer_id{};							///< Renderer selected for this window.
		std::optional<Entity> camera{};					///< Camera rendered through this window, when selected.
		bool focused{false};								///< True while the window has keyboard focus.
		bool minimized{false};								///< True while the platform reports a minimized window.
		bool should_close{false};							///< True after a close request.
	};															///< Facade frame snapshot for one window.

	struct WindowFrameData {
		Vector<WindowFrameInfo> windows{};				///< Window states after event polling.
	};															///< Facade frame snapshot passed to user systems.

	namespace detail {

		struct EngineWindowSetup {
			std::string id{"main"};						///< Stable application-local window id.
			std::string title{"VVE simple"};			///< Platform window title.
			PixelExtent extent{.width = 960, .height = 540};	///< Initial pixel dimensions.
			std::optional<int> x{};						///< Optional initial screen x coordinate.
			std::optional<int> y{};						///< Optional initial screen y coordinate.
			RendererId renderer_id{};					///< Renderer selected for this window.
			bool resizable{true};						///< Enables platform resizing.
			bool visible{true};							///< Shows the window after creation.
		};													///< Opaque startup window descriptor for implementation conversion.

		struct EngineStartupOptions {
			EngineConfig config{};						///< Compact startup configuration.
			std::optional<std::vector<EngineWindowSetup>> windows{};	///< Optional startup windows.
		};													///< Facade-owned startup options consumed by the implementation unit.

		struct EngineState;								///< Opaque owning engine implementation state.
		struct EngineStateDeleter {
			void operator()(EngineState *state) const noexcept;
		};													///< Deletes the opaque engine state in the implementation unit.

		using EngineStateHandle = std::unique_ptr<EngineState, EngineStateDeleter>;	///< Owning engine state handle.

		[[nodiscard]] EngineStateHandle makeEngineState(EngineStartupOptions options);
		[[nodiscard]] auto engineVersionMajor(const EngineState &state)								-> std::uint32_t;
		[[nodiscard]] auto engineVersionName(const EngineState &state)								-> std::string_view;
		[[nodiscard]] auto engineEcs(EngineState &state)												-> ECS &;
		[[nodiscard]] auto engineAssets(EngineState &state)											-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem &;
		[[nodiscard]] auto engineGui(EngineState &state)												-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem &;
		[[nodiscard]] auto engineWindowSystem(EngineState &state)									-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowSystem &;
		[[nodiscard]] auto engineRenderSystem(EngineState &state)									-> VVE_ENGINE_IMPLEMENTATION_NAMESPACE::RenderSystem &;
		[[nodiscard]] auto engineInit(EngineState &state)											-> std::expected<void, Error>;
		[[nodiscard]] auto engineStep(EngineState &state)											-> std::expected<FrameStatus, Error>;
		[[nodiscard]] auto engineWindowFrame(EngineState &state)										-> WindowFrameData;
		[[nodiscard]] auto engineRenderFrame(EngineState &state)										-> std::expected<void, Error>;

	} // namespace detail

	template <typename... TSystems> class Engine {
	public:
		Engine();

		Engine(const Engine &) = delete;
		Engine(Engine &&) = delete;
		Engine &operator=(const Engine &) = delete;
		Engine &operator=(Engine &&) = delete;

		explicit Engine(EngineConfig config);

		template <typename... TOptions>
			requires(sizeof...(TOptions) > 0)
		explicit Engine(TOptions &&...options);

		[[nodiscard]] auto versionMajor() const												-> std::uint32_t;
		[[nodiscard]] auto versionName() const													-> std::string_view;
		[[nodiscard]] auto world();
		[[nodiscard]] auto world() const;

		[[nodiscard]] auto init()																	-> std::expected<void, Error>;
		[[nodiscard]] auto run()																	-> std::expected<void, Error>;
		[[nodiscard]] auto step()																	-> std::expected<FrameStatus, Error>;

	private:
		explicit Engine(detail::EngineStartupOptions options);

		template <typename... TOptions> static auto startupOptions(TOptions &&...options)	-> detail::EngineStartupOptions;
		template <typename TOption> static void appendStartupOption(detail::EngineStartupOptions &options, TOption &&option);
		static void appendStartupOption(detail::EngineStartupOptions &options, WindowSetups option);
		[[nodiscard]] auto makeWorld();
		template <typename TOption> void applyOption(TOption &&option);
		template <typename... TUserSystems> void applyOption(const UserSystems<TUserSystems...> &systems);
		template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &systems);
		template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &&systems);
		[[nodiscard]] auto initSystems()															-> std::expected<void, Error>;
		[[nodiscard]] auto updateSystems(const FrameContext &frame)						-> std::expected<void, Error>;
		template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system);
		template <typename TSystem>
		[[nodiscard]] std::expected<void, Error>
		updateOne(TSystem &system, const FrameContext &frame, const WindowFrameData &window_frame);

		detail::EngineStateHandle state_;								///< Opaque owning engine implementation state.
		ECS &ecs_;																///< ECS owned by the implementation, referenced by world views.
		AssetSystem assets_;												///< Public asset-system wrapper referenced by world views.
		GuiSystem gui_;														///< Public GUI wrapper referenced by world views.
		WindowSystem window_system_;										///< Public window wrapper referenced by world views.
		RenderSystem render_system_;										///< Public render wrapper referenced by world views.
		std::optional<std::tuple<TSystems...>> systems_{};				///< User systems supplied by the application.
		std::chrono::steady_clock::time_point last_frame_time_{};	///< Timestamp of the previous facade step().
		std::uint64_t frame_{0};												///< Number of completed facade step() calls.
		bool systems_initialized_{false};									///< True after user-system init hooks succeed.
	};														///< Facade engine template.

	namespace detail {

		template <typename T> struct IsUserSystemsOption : std::false_type {};
		template <typename... TSystems> struct IsUserSystemsOption<UserSystems<TSystems...>> : std::true_type {};

		template <typename TDefault, typename... TOptions> struct FindUserSystemsOption {
			using type = TDefault;
		};

		template <typename TDefault, typename TFirst, typename... TRest>
		struct FindUserSystemsOption<TDefault, TFirst, TRest...> {
			using TNormalized = std::remove_cvref_t<TFirst>;
			using type = std::conditional_t<IsUserSystemsOption<TNormalized>::value, TNormalized,
														typename FindUserSystemsOption<TDefault, TRest...>::type>;
		};

		template <typename TUserSystems> struct EngineTypeFromUserSystems;
		template <typename... TSystems> struct EngineTypeFromUserSystems<UserSystems<TSystems...>> {
			using type = Engine<TSystems...>;
		};

		template <std::size_t TPriority> struct Priority : Priority<TPriority - 1> {};
		template <> struct Priority<0> {};

		template <typename TCallable> [[nodiscard]] std::expected<void, Error> callSystemHook(TCallable &&callable) {
			using TResult = std::invoke_result_t<TCallable>;
			if constexpr (std::same_as<TResult, std::expected<void, Error>>) {
				return std::invoke(std::forward<TCallable>(callable));
			} else {
				std::invoke(std::forward<TCallable>(callable));
				return {};
			}
		}

		template <typename TSystem, typename TWorld>
		[[nodiscard]] auto invokeUserSystemInit(TSystem &system, TWorld &world, Priority<1>)
			-> decltype(system.init(world), std::expected<void, Error>{}) {
			return callSystemHook([&]() -> decltype(auto) { return system.init(world); });
		}

		template <typename TSystem, typename TWorld>
		[[nodiscard]] auto invokeUserSystemInit(TSystem &, TWorld &, Priority<0>)	-> std::expected<void, Error>{
			return {};
		}

		template <typename TSystem, typename TWorld, typename TWindowFrame>
		[[nodiscard]] auto invokeUserSystemUpdate(TSystem &system, TWorld &world, const FrameContext &frame,
																const TWindowFrame &window_frame, Priority<3>)
			-> decltype(system.update(world, frame, window_frame), std::expected<void, Error>{}) {
			return callSystemHook([&]() -> decltype(auto) { return system.update(world, frame, window_frame); });
		}

		template <typename TSystem, typename TWorld, typename TWindowFrame>
		[[nodiscard]] auto invokeUserSystemUpdate(TSystem &system, TWorld &world, const FrameContext &frame,
																const TWindowFrame &, Priority<2>)
			-> decltype(system.update(world, frame), std::expected<void, Error>{}) {
			return callSystemHook([&]() -> decltype(auto) { return system.update(world, frame); });
		}

		template <typename TSystem, typename TWorld, typename TWindowFrame>
		[[nodiscard]] auto invokeUserSystemUpdate(TSystem &system, TWorld &world, const FrameContext &,
																const TWindowFrame &, Priority<1>)
			-> decltype(system.update(world), std::expected<void, Error>{}) {
			return callSystemHook([&]() -> decltype(auto) { return system.update(world); });
		}

		template <typename TSystem, typename TWorld, typename TWindowFrame>
		[[nodiscard]] std::expected<void, Error> invokeUserSystemUpdate(TSystem &, TWorld &, const FrameContext &,
																								const TWindowFrame &, Priority<0>) {
			return {};
		}

	} // namespace detail

	template <typename... TSystems> class EngineBuilder {
	public:
		inline EngineBuilder() = default;
		explicit inline EngineBuilder(UserSystems<TSystems...> systems) : user_systems_{std::move(systems)} {}

		[[nodiscard]] inline EngineBuilder &applicationName(std::string value) {
			application_name_ = ApplicationName{.value = std::move(value)};
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &maxFrames(MaxFrames value) {
			max_frames_ = value;
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &windows(WindowSetups value) {
			windows_ = std::move(value);
			windows_configured_ = true;
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &addWindow(WindowSetup value) {
			if (!windows_configured_) { windows_ = WindowSetups{std::move(value)}; }
			else { windows_.add(std::move(value)); }
			windows_configured_ = true;
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &userSystems(UserSystems<TSystems...> value) {
			user_systems_ = std::move(value);
			return *this;
		}
		[[nodiscard]] inline auto build() const {
			if (windows_configured_) { return Engine<TSystems...>{application_name_, max_frames_, windows_, user_systems_}; }
			return Engine<TSystems...>{application_name_, max_frames_, user_systems_};
		}

	private:
		ApplicationName application_name_{};			///< Human-readable application name option.
		MaxFrames max_frames_{};						///< Optional frame cap option.
		WindowSetups windows_{};						///< Startup window collection option.
		UserSystems<TSystems...> user_systems_{};	///< User systems stored through the facade bundle.
		bool windows_configured_{false};				///< True after startup windows are explicitly configured.
	};														///< Chainable facade engine factory.

	template <typename... TSystems> Engine<TSystems...>::Engine() : Engine{detail::EngineStartupOptions{}} {}

	template <typename... TSystems>
	Engine<TSystems...>::Engine(detail::EngineStartupOptions options)
		: state_{detail::makeEngineState(std::move(options))}, ecs_{detail::engineEcs(*state_)},
		  assets_{detail::engineAssets(*state_)}, gui_{detail::engineGui(*state_)},
		  window_system_{detail::engineWindowSystem(*state_)}, render_system_{detail::engineRenderSystem(*state_)} {}

	template <typename... TSystems>
	Engine<TSystems...>::Engine(EngineConfig config) : Engine{startupOptions(std::move(config))} {}

	template <typename... TSystems>
	template <typename... TOptions>
		requires(sizeof...(TOptions) > 0)
	Engine<TSystems...>::Engine(TOptions &&...options) : Engine{startupOptions(options...)} {
		(applyOption(std::forward<TOptions>(options)), ...);
	}

	template <typename... TSystems> std::uint32_t Engine<TSystems...>::versionMajor() const {
		return detail::engineVersionMajor(*state_);
	}


	template <typename... TSystems> std::string_view Engine<TSystems...>::versionName() const {
		return detail::engineVersionName(*state_);
	}

	template <typename... TSystems> auto Engine<TSystems...>::world() {
		return makeWorld();
	}

	template <typename... TSystems> auto Engine<TSystems...>::world() const {
		return const_cast<Engine *>(this)->world();
	}

	template <typename... TSystems>
	template <typename... TOptions>
	auto Engine<TSystems...>::startupOptions(TOptions &&...options) -> detail::EngineStartupOptions {
		auto result = detail::EngineStartupOptions{};
		(appendStartupOption(result, std::forward<TOptions>(options)), ...);
		return result;
	}

	template <typename... TSystems>
	template <typename TOption>
	void Engine<TSystems...>::appendStartupOption(detail::EngineStartupOptions &options, TOption &&option) {
		using Option = std::remove_cvref_t<TOption>;
		if constexpr (std::same_as<Option, EngineConfig>) {
			options.config = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, ApplicationName>) {
			options.config.application_name = std::forward<TOption>(option).value;
		} else if constexpr (std::same_as<Option, MaxFrames>) {
			options.config.max_frames = std::forward<TOption>(option).value;
		} else {
			(void)options;
			(void)option;
		}
	}

	template <typename... TSystems>
	void Engine<TSystems...>::appendStartupOption(detail::EngineStartupOptions &options, WindowSetups option) {
		auto windows = std::vector<detail::EngineWindowSetup>{};
		windows.reserve(option.value_.size());
		for (auto &window : option.value_) {
			windows.push_back(detail::EngineWindowSetup{.id = std::move(window.id_),
																	  .title = std::move(window.title_),
																	  .extent = window.extent_,
																	  .x = window.x_,
																	  .y = window.y_,
																	  .renderer_id = std::move(window.renderer_id_),
																	  .resizable = window.resizable_,
																	  .visible = window.visible_});
		}
		options.windows = std::move(windows);
	}

	template <typename... TSystems> auto Engine<TSystems...>::makeWorld() {
		auto make_base = [&] {
			return World{std::ref(ecs_), std::ref(assets_), std::ref(gui_), std::ref(window_system_),
								std::ref(render_system_)};
		};
		if constexpr (sizeof...(TSystems) == 0) {
			return make_base();
		} else {
			return std::apply([&](auto &...system) {
				return World{std::ref(ecs_), std::ref(assets_), std::ref(gui_),
									std::ref(window_system_), std::ref(render_system_), std::ref(system)...};
			}, *systems_);
		}
	}

	template <typename... TSystems>
	template <typename TOption>
	void Engine<TSystems...>::applyOption(TOption &&option) {
		if constexpr (requires { std::forward<TOption>(option).value; }) {
			using Value = std::remove_cvref_t<decltype(std::forward<TOption>(option).value)>;
			if constexpr (std::same_as<Value, std::tuple<TSystems...>>) {
				systems_.emplace(std::forward<TOption>(option).value);
			} else {
				(void)option;
			}
		} else {
			(void)option;
		}
	}

	template <typename... TSystems>
	template <typename... TUserSystems>
	void Engine<TSystems...>::applyOption(const UserSystems<TUserSystems...> &systems) {
		systems_.emplace(systems.value);
	}

	template <typename... TSystems>
	template <typename... TUserSystems>
	void Engine<TSystems...>::applyOption(UserSystems<TUserSystems...> &systems) {
		systems_.emplace(systems.value);
	}

	template <typename... TSystems>
	template <typename... TUserSystems>
	void Engine<TSystems...>::applyOption(UserSystems<TUserSystems...> &&systems) {
		systems_.emplace(std::move(systems.value));
	}

	template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::init() {
		if (const auto result = detail::engineInit(*state_); !result) { return result; }
		if (systems_initialized_) { return {}; }
		if (const auto result = initSystems(); !result) { return result; }
		last_frame_time_ = std::chrono::steady_clock::now();
		systems_initialized_ = true;
		return {};
	}

	template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::run() {
		if (const auto result = init(); !result) { return result; }
		while (true) {
			const auto status = step();
			if (!status) { return std::unexpected(status.error()); }
			if (*status == FrameStatus::stopped) { return {}; }
		}
	}

	template <typename... TSystems> std::expected<FrameStatus, Error> Engine<TSystems...>::step() {
		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<double> delta = now - last_frame_time_;
		const auto status = detail::engineStep(*state_);
		if (!status) { return std::unexpected(status.error()); }

		const FrameContext frame{.frame_index = FrameCount{.value = frame_},
											.delta_time = DeltaTime{.seconds = delta.count()}};
		if (const auto result = updateSystems(frame); !result) { return std::unexpected(result.error()); }
		if (const auto result = detail::engineRenderFrame(*state_); !result) {
			return std::unexpected(result.error());
		}
		last_frame_time_ = now;
		++frame_;
		return *status;
	}

	template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::initSystems() {
		if (!systems_.has_value()) { return {}; }
		auto result = std::expected<void, Error>{};
		std::apply([&](auto &...system) { ((result ? result = initOne(system) : result), ...); }, *systems_);
		return result;
	}

	template <typename... TSystems>
	std::expected<void, Error> Engine<TSystems...>::updateSystems(const FrameContext &frame) {
		if (!systems_.has_value()) { return {}; }
		auto result = std::expected<void, Error>{};
		const auto window_frame = detail::engineWindowFrame(*state_);
		std::apply([&](auto &...system) {
			((result ? result = updateOne(system, frame, window_frame) : result), ...);
		}, *systems_);
		return result;
	}

	template <typename... TSystems>
	template <typename TSystem>
	std::expected<void, Error> Engine<TSystems...>::initOne(TSystem &system) {
		auto world_view = world();
		return detail::invokeUserSystemInit(system, world_view, detail::Priority<1>{});
	}

	template <typename... TSystems>
	template <typename TSystem>
	std::expected<void, Error>
	Engine<TSystems...>::updateOne(TSystem &system, const FrameContext &frame, const WindowFrameData &window_frame) {
		auto world_view = world();
		return detail::invokeUserSystemUpdate(system, world_view, frame, window_frame, detail::Priority<3>{});
	}

} // namespace vve
