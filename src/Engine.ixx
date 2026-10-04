export module VVEngine;
import std;
export import :Implementation;
export import VVEngine.Error;
export import VVEngine.Math;
export import VVEngine.Handle;
export import VVEngine.Vector;
export import VVEngine.Types;
export import :ECS;
export import :Window;
export import :World;
export import :Assets;
export import :RenderSystem;
export import :Gui;
export import :Audio;

/// @file
/// @brief Public engine facade; users import this module and use only namespace vve.

export namespace vve {


	namespace detail {

		struct EngineStartupOptions {
			ApplicationName application_name{};	///< Application name passed to the selected engine.
			MaxFrames max_frames{};	///< Optional frame limit passed to the selected engine.
			std::optional<Vector<WindowDesc>> windows{};	///< Optional startup windows.
		};													///< Facade-owned startup options consumed by the engine factory.

		/// @brief Converts startup window options and returns the implementation owned by the facade.
		[[nodiscard]] inline std::unique_ptr<EngineImpl> makeEngineImpl(EngineStartupOptions options) {
			if (options.windows.has_value()) {
				return std::make_unique<EngineImpl>(std::move(options.application_name), options.max_frames,
					WindowsImpl{.value = std::move(*options.windows)});
			}
			return std::make_unique<EngineImpl>(std::move(options.application_name), options.max_frames);
		}

	} // namespace detail

	template <typename... TSystems> class Engine {
	public:
		Engine();

		Engine(const Engine &) = delete;
		Engine(Engine &&) = delete;
		Engine &operator=(const Engine &) = delete;
		Engine &operator=(Engine &&) = delete;

		template <typename... TOptions>
			requires(sizeof...(TOptions) > 0)
		explicit Engine(TOptions &&...options);

		[[nodiscard]] auto versionMajor() const												-> std::uint32_t;
		[[nodiscard]] auto versionName() const													-> std::string_view;
		[[nodiscard]] auto world();

		[[nodiscard]] auto init()																	-> std::expected<void, Error>;
		[[nodiscard]] auto run()																	-> std::expected<void, Error>;
		[[nodiscard]] auto step()																	-> std::expected<FrameStatus, Error>;
		[[nodiscard]] FrameContext frameContext() const;

	private:
		explicit Engine(detail::EngineStartupOptions options);

		template <typename... TOptions> static auto startupOptions(TOptions &&...options)	-> detail::EngineStartupOptions;
		template <typename TOption> static void appendStartupOption(detail::EngineStartupOptions &options, TOption &&option);
		static void appendStartupOption(detail::EngineStartupOptions &options, WindowSetups option);
		[[nodiscard]] auto makeWorld();
		void defaultSystems();
		template <typename TOption> void applyOption(TOption &&option);
		template <typename... TUserSystems> void applyOption(const UserSystems<TUserSystems...> &systems);
		template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &systems);
		template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &&systems);
		[[nodiscard]] auto initSystems()															-> std::expected<void, Error>;
		[[nodiscard]] auto updateSystems(const FrameContext &frame)						-> std::expected<void, Error>;
		template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system);

		std::unique_ptr<detail::EngineImpl> impl_;					///< Selected engine implementation owned by the facade.
		ECS &ecs_;																///< ECS owned by the implementation, referenced by world views.
		AssetSystem assets_;												///< Public asset-system wrapper referenced by world views.
		GuiSystem gui_;														///< Public GUI wrapper referenced by world views.
		AudioSystem audio_; ///< Public audio wrapper referenced by world views.
		WindowSystem window_system_;										///< Public window wrapper referenced by world views.
		RenderSystem render_system_;										///< Public render wrapper referenced by world views.
		std::optional<std::tuple<TSystems...>> systems_{};				///< User systems; always engaged after construction.
		std::size_t systems_initialized_count_{0};					///< Number of user systems successfully initialized in tuple order.
	};														///< Facade engine template.

	namespace detail {

		template <typename T> struct IsUserSystemsOption : std::false_type {};
		template <typename... TSystems> struct IsUserSystemsOption<UserSystems<TSystems...>> : std::true_type {};

		template <std::size_t TPriority> struct Priority : Priority<TPriority - 1> {};
		template <> struct Priority<0> {};

		/// @brief Checks the three-argument hook per system before folding results for window-frame preparation.
		template <typename TSystem, typename TWorld>
		concept HasWindowFrameUpdate = requires(TSystem &system, TWorld &world,
			const FrameContext &frame, const WindowFrameData &window_frame) {
			system.update(world, frame, window_frame);
		};

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
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &addWindow(WindowSetup value) {
			windows_.add(std::move(value));
			return *this;
		}
		[[nodiscard]] inline EngineBuilder &userSystems(UserSystems<TSystems...> value) {
			user_systems_ = std::move(value);
			return *this;
		}
		[[nodiscard]] inline auto build() const {
			return Engine<TSystems...>{application_name_, max_frames_, windows_, user_systems_};
		}

	private:
		ApplicationName application_name_{};			///< Human-readable application name option.
		MaxFrames max_frames_{};						///< Optional frame cap option.
		WindowSetups windows_{};						///< Startup window collection option.
		UserSystems<TSystems...> user_systems_{};	///< User systems stored through the facade bundle.
	};														///< Chainable facade engine factory.

	template <typename... TSystems> Engine<TSystems...>::Engine() : Engine{detail::EngineStartupOptions{}} {
		static_assert((std::default_initializable<TSystems> && ...),
			"Engine<TSystems...>: pass UserSystems{...} when a system has no default constructor");
		defaultSystems();
	}

	template <typename... TSystems>
	Engine<TSystems...>::Engine(detail::EngineStartupOptions options)
		: impl_{detail::makeEngineImpl(std::move(options))}, ecs_{impl_->ecs()},
		  assets_{impl_->assets()}, gui_{impl_->gui()}, audio_{impl_->audioSystem()},
		  window_system_{impl_->windowSystem()}, render_system_{impl_->renderSystem()} {}

	template <typename... TSystems>
	template <typename... TOptions>
		requires(sizeof...(TOptions) > 0)
	Engine<TSystems...>::Engine(TOptions &&...options) : Engine{startupOptions(options...)} {
		static_assert((std::default_initializable<TSystems> && ...) ||
			(detail::IsUserSystemsOption<std::remove_cvref_t<TOptions>>::value || ...),
			"Engine<TSystems...>: pass UserSystems{...} when a system has no default constructor");
		(applyOption(std::forward<TOptions>(options)), ...);
		defaultSystems();
	}

	/// @brief Default-constructs the user systems when no UserSystems option supplied them, so world() can always reference them.
	template <typename... TSystems> void Engine<TSystems...>::defaultSystems() {
		if constexpr ((std::default_initializable<TSystems> && ...)) {
			if (!systems_.has_value()) { systems_.emplace(); }
		}
	}

	template <typename... TSystems> std::uint32_t Engine<TSystems...>::versionMajor() const {
		return impl_->versionMajor();
	}


	template <typename... TSystems> std::string_view Engine<TSystems...>::versionName() const {
		return impl_->versionName();
	}

	template <typename... TSystems> auto Engine<TSystems...>::world() {
		return makeWorld();
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
		if constexpr (std::same_as<Option, ApplicationName>) {
			options.application_name = std::forward<TOption>(option);
		} else if constexpr (std::same_as<Option, MaxFrames>) {
			options.max_frames = std::forward<TOption>(option);
		} else if constexpr (detail::IsUserSystemsOption<Option>::value) {
			(void)options;	// User systems are applied after construction by applyOption.
		} else {
			static_assert(!std::same_as<Option, Option>, "Engine: unknown startup option type");
		}
	}

	template <typename... TSystems>
	void Engine<TSystems...>::appendStartupOption(detail::EngineStartupOptions &options, WindowSetups option) {
		auto windows = Vector<WindowDesc>{};
		windows.reserve(option.value_.size());
		for (auto &window : option.value_) {
			windows.push_back(std::move(window.value_));
		}
		options.windows = std::move(windows);
	}

	template <typename... TSystems> auto Engine<TSystems...>::makeWorld() {
		auto make_base = [&] {
			return World{std::ref(ecs_), std::ref(assets_), std::ref(gui_), std::ref(window_system_),
								std::ref(render_system_), std::ref(audio_)};
		};
		if constexpr (sizeof...(TSystems) == 0) {
			return make_base();
		} else {
			return std::apply([&](auto &...system) {
				return World{std::ref(ecs_), std::ref(assets_), std::ref(gui_),
									std::ref(window_system_), std::ref(render_system_), std::ref(audio_), std::ref(system)...};
			}, *systems_);
		}
	}

	/// @brief Startup options were already consumed by startupOptions; only UserSystems (the overloads below) apply here.
	template <typename... TSystems>
	template <typename TOption>
	void Engine<TSystems...>::applyOption(TOption &&option) {
		(void)option;
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

	/// @brief Initializes the engine and resumes user-system initialization after the last successful hook.
	template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::init() {
		if (const auto result = impl_->init(); !result) { return result; }
		if (systems_initialized_count_ == sizeof...(TSystems)) { return {}; }
		if (const auto result = initSystems(); !result) { return result; }
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
		const auto status = impl_->step();
		if (!status) { return std::unexpected(status.error()); }

		const FrameContext frame = impl_->frameContext();
		if (const auto result = updateSystems(frame); !result) { return std::unexpected(result.error()); }
		if (const auto result = impl_->renderFrame(); !result) {
			return std::unexpected(result.error());
		}
		return *status;
	}

	/// @brief Returns the implementation's latest polled frame, including failed updates or renders; the first delta is DeltaTime{}.
	template <typename... TSystems> FrameContext Engine<TSystems...>::frameContext() const {
		return impl_->frameContext();
	}

	/// @brief Initializes remaining systems in order, retaining progress and the first error for retries.
	template <typename... TSystems> std::expected<void, Error> Engine<TSystems...>::initSystems() {
		if (!systems_.has_value()) { return {}; }
		auto result = std::expected<void, Error>{};
		// Keep successful hooks, including absent hooks, complete across init retries.
		[&]<std::size_t... I>(std::index_sequence<I...>) {
			([&] {
				if (!result || I < systems_initialized_count_) { return; }
				result = initOne(std::get<I>(*systems_));
				if (result) { ++systems_initialized_count_; }
			}(), ...);
		}(std::index_sequence_for<TSystems...>{});
		return result;
	}

	/// @brief Updates systems in order, copying window states only for hooks that receive them.
	template <typename... TSystems>
	std::expected<void, Error> Engine<TSystems...>::updateSystems(const FrameContext &frame) {
		// An empty engine has no user hooks or window data to prepare.
		if constexpr (sizeof...(TSystems) == 0) { return {}; }
		if (!systems_.has_value()) { return {}; }
		auto result = std::expected<void, Error>{};
		// Match the call expression and reference types used by the Priority<3> overload.
		constexpr bool needs_window_frame = (detail::HasWindowFrameUpdate<TSystems, decltype(world())> || ...);
		const auto update = [&](const auto &window_frame, auto priority) {
			// Preserve hook order, a fresh world view per system and the first failure.
			std::apply([&](auto &...system) {
				([&] {
					if (!result) { return; }
					auto world_view = world();
					result = detail::invokeUserSystemUpdate(system, world_view, frame, window_frame, priority);
				}(), ...);
			}, *systems_);
		};
		if constexpr (needs_window_frame) {
			WindowFrameData window_frame{};
			window_frame.windows.reserve(impl_->windowSystem().windowCount());
			// Copy each owned window's state once for all three-argument hooks.
			for (const auto &window : impl_->windowSystem().windows()) {
				window_frame.windows.push_back(window.get().info());
			}
			update(window_frame, detail::Priority<3>{});
		} else {
			update(nullptr, detail::Priority<2>{});
		}
		return result;
	}

	template <typename... TSystems>
	template <typename TSystem>
	std::expected<void, Error> Engine<TSystems...>::initOne(TSystem &system) {
		auto world_view = world();
		return detail::invokeUserSystemInit(system, world_view, detail::Priority<1>{});
	}

} // namespace vve
