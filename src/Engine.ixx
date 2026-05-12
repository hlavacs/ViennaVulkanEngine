module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine;
import std;
import VEEngine.V4;
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

   inline constexpr std::string_view engineImplementationNamespaceName{
      VVE_DETAIL_STRINGIFY(VVE_ENGINE_IMPLEMENTATION_NAMESPACE)}; ///< Active implementation namespace name.

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

      [[nodiscard]] std::uint32_t versionMajor() const;
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept;
      [[nodiscard]] std::string_view versionName() const;
      [[nodiscard]] auto world();
      [[nodiscard]] auto world() const;

      [[nodiscard]] std::expected<void, Error> init();
      [[nodiscard]] std::expected<void, Error> run();
      [[nodiscard]] std::expected<FrameStatus, Error> step();
      [[nodiscard]] std::expected<void, Error>
      writeDebugGraphs(const std::filesystem::path &directory = "graph_dumps") const;

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine;

      static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows implementationOption(WindowSetups option);
      template <typename... TUserSystems>
      static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
      implementationOption(const UserSystems<TUserSystems...> &systems);
      template <typename... TUserSystems>
      static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
      implementationOption(UserSystems<TUserSystems...> &systems);

      template <typename TOption> static decltype(auto) implementationOption(TOption &&option);

      [[nodiscard]] auto makeWorld();
      template <typename TOption> void applyOption(TOption &&option);
      template <typename... TUserSystems> void applyOption(const UserSystems<TUserSystems...> &systems);
      template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &systems);
      template <typename... TUserSystems> void applyOption(UserSystems<TUserSystems...> &&systems);
      [[nodiscard]] std::expected<void, Error> initSystems();
      [[nodiscard]] std::expected<void, Error> updateSystems(const FrameContext &frame);
      template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system);
      template <typename TSystem>
      [[nodiscard]] std::expected<void, Error>
      updateOne(TSystem &system, const FrameContext &frame,
                const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData &window_frame);
      template <typename TSystem> [[nodiscard]] static std::string systemDebugName(const TSystem &system);
      template <typename... TUserSystems>
      [[nodiscard]] static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
      makeUserSystemTasks(const std::tuple<TUserSystems...> &systems);

      Impl impl_{};
      ECS ecs_{impl_.ecs()};                   ///< Public ECS wrapper referenced by world views.
      AssetSystem assets_{impl_.assets()};     ///< Public asset-system wrapper referenced by world views.
      GuiSystem gui_{impl_.gui()};             ///< Public GUI wrapper referenced by world views.
      WindowSystem window_system_{impl_.windowSystem()}; ///< Public window wrapper referenced by world views.
      RenderSystem render_system_{impl_.renderSystem()}; ///< Public render wrapper referenced by world views.
      std::optional<std::tuple<TSystems...>> systems_{}; ///< User systems supplied by the application.
      std::chrono::steady_clock::time_point last_frame_time_{}; ///< Timestamp of the previous facade step().
      std::uint64_t frame_{0};                 ///< Number of completed facade step() calls.
      bool systems_initialized_{false};        ///< True after user-system init hooks succeed.
   }; ///< Facade engine template.

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
      [[nodiscard]] std::expected<void, Error> invokeUserSystemInit(TSystem &, TWorld &, Priority<0>) {
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

   struct MakeEngine {
      template <typename... TOptions> [[nodiscard]] auto operator()(TOptions &&...options) const;
   }; ///< Callable facade engine factory.

   template <typename... TOptions> auto MakeEngine::operator()(TOptions &&...options) const {
      using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
      using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;
      return TEngine(std::forward<TOptions>(options)...);
   }

   inline constexpr MakeEngine makeEngine{}; ///< Facade engine factory.

   template <typename... TSystems> Engine<TSystems...>::Engine() = default;

   template <typename... TSystems>
   Engine<TSystems...>::Engine(EngineConfig config) : impl_{std::move(config)} {}

   template <typename... TSystems>
   template <typename... TOptions>
      requires(sizeof...(TOptions) > 0)
   Engine<TSystems...>::Engine(TOptions &&...options) : impl_{implementationOption(options)...} {
      (applyOption(std::forward<TOptions>(options)), ...);
   }

   template <typename... TSystems> std::uint32_t Engine<TSystems...>::versionMajor() const {
      return impl_.versionMajor();
   }

   template <typename... TSystems> std::expected<int, Error> Engine<TSystems...>::getVersionMajor() const noexcept {
      return impl_.getVersionMajor();
   }

   template <typename... TSystems> std::string_view Engine<TSystems...>::versionName() const {
      return impl_.versionName();
   }

   template <typename... TSystems> auto Engine<TSystems...>::world() {
      return makeWorld();
   }

   template <typename... TSystems> auto Engine<TSystems...>::world() const {
      return const_cast<Engine *>(this)->world();
   }

   template <typename... TSystems>
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows Engine<TSystems...>::implementationOption(WindowSetups option) {
      return option;
   }

   template <typename... TSystems>
   template <typename... TUserSystems>
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
   Engine<TSystems...>::implementationOption(const UserSystems<TUserSystems...> &systems) {
      return makeUserSystemTasks(systems.value);
   }

   template <typename... TSystems>
   template <typename... TUserSystems>
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
   Engine<TSystems...>::implementationOption(UserSystems<TUserSystems...> &systems) {
      return makeUserSystemTasks(systems.value);
   }

   template <typename... TSystems>
   template <typename TOption>
   decltype(auto) Engine<TSystems...>::implementationOption(TOption &&option) {
      return std::forward<TOption>(option);
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
   template <typename TSystem>
   std::string Engine<TSystems...>::systemDebugName(const TSystem &system) {
      if constexpr (requires { std::string_view{system.name()}; }) {
         return std::string{std::string_view{system.name()}};
      } else if constexpr (requires { std::string_view{TSystem::name()}; }) {
         return std::string{std::string_view{TSystem::name()}};
      } else {
         return typeid(TSystem).name();
      }
   }

   template <typename... TSystems>
   template <typename... TUserSystems>
   VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks
   Engine<TSystems...>::makeUserSystemTasks(const std::tuple<TUserSystems...> &systems) {
      VVE_ENGINE_IMPLEMENTATION_NAMESPACE::UserSystemTasks result{};
      std::apply([&](const auto &...system) {
         (result.value.push_back(ObjectName{.value = "task.update_system." + systemDebugName(system)}), ...);
      }, systems);
      return result;
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
      if (const auto result = impl_.init(); !result) { return result; }
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
      const auto status = impl_.step();
      if (!status) { return std::unexpected(status.error()); }

      const FrameContext frame{.frame_index = FrameCount{.value = frame_},
                               .delta_time = DeltaTime{.seconds = delta.count()}};
      if (const auto result = updateSystems(frame); !result) { return std::unexpected(result.error()); }
      if (const auto result = impl_.renderSystem().renderFrame(impl_.windowSystem()); !result) {
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
      const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData window_frame{
         .windows = impl_.windowSystem().snapshot()};
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
   Engine<TSystems...>::updateOne(TSystem &system, const FrameContext &frame,
                                  const VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData &window_frame) {
      auto world_view = world();
      return detail::invokeUserSystemUpdate(system, world_view, frame, window_frame, detail::Priority<3>{});
   }

   template <typename... TSystems>
   std::expected<void, Error>
   Engine<TSystems...>::writeDebugGraphs(const std::filesystem::path &directory) const {
      return impl_.writeDebugGraphs(directory);
   }

} // namespace vve
