export module VEEngine.V4;
import std;
export import :WindowSystem;
export import :Assets;
export import :Renderer;
export import :Gui;

/// @file
/// @brief Small v4 runtime facade: world, SDL windows, input, systems, and stub subsystems.

export namespace vve::v4 {

   /// @brief Result of one engine frame.
   enum class FrameStatus {
      running,                  ///< Engine can continue stepping.
      stopped,                  ///< Engine stopped because a close request or frame cap was reached.
      continue_running = running, ///< Alias kept close to v3-style wording.
      should_close     = stopped  ///< Alias kept close to v3-style wording.
   };

   /// @brief Backward-compatible compact config; typed options are preferred for new examples.
   struct EngineConfig {
      std::string application_name{"v4"}; ///< Human-readable application name.
      std::uint32_t max_frames{0};        ///< Zero means no frame limit.
   };

   /// @brief Heterogeneous user-system storage used by makeEngine().
   template <typename... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{}; ///< User systems stored by value.
   };

   /// @brief Builds a user-system bundle while preserving concrete system types.
   template <typename... TSystems> [[nodiscard]] auto makeUserSystems(TSystems &&...systems) {
      return UserSystems<std::remove_cvref_t<TSystems>...>{
         .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
   }

   namespace detail {

      /// @brief Primary trait for detecting UserSystems options.
      template <typename T> struct IsUserSystemsOption : std::false_type {};

      /// @brief Specialization that detects UserSystems options.
      template <typename... TSystems> struct IsUserSystemsOption<UserSystems<TSystems...>> : std::true_type {};

      /// @brief Finds the first UserSystems option in an option pack.
      template <typename TDefault, typename... TOptions> struct FindUserSystemsOption {
         using type = TDefault; ///< Fallback when no user-system bundle is provided.
      };

      /// @brief Recursive option-pack scanner.
      template <typename TDefault, typename TFirst, typename... TRest>
      struct FindUserSystemsOption<TDefault, TFirst, TRest...> {
         using TNormalized = std::remove_cvref_t<TFirst>; ///< Option type stripped of references and cv-qualifiers.
         using type = std::conditional_t<IsUserSystemsOption<TNormalized>::value, TNormalized,
                                         typename FindUserSystemsOption<TDefault, TRest...>::type>;
      };

      /// @brief Invokes a system hook and accepts either void or expected<void, Error>.
      template <typename TCallable> [[nodiscard]] std::expected<void, Error> callSystemHook(TCallable &&callable) {
         using TResult = std::invoke_result_t<TCallable>;
         if constexpr (std::same_as<TResult, std::expected<void, Error>>) {
            return std::invoke(std::forward<TCallable>(callable));
         } else {
            std::invoke(std::forward<TCallable>(callable));
            return {};
         }
      }

   } // namespace detail

   /// @brief Educational v4 engine shell with SDL windows and a tiny world facade.
   template <typename... TSystems> class Engine {
   public:
      /// @brief Creates an engine with default options.
      Engine() { applyDefaults(); }

      /// @brief Creates an engine from the compact compatibility config.
      explicit Engine(EngineConfig config) {
         applyOption(std::move(config));
         applyDefaults();
      }

      /// @brief Creates an engine from typed options such as ApplicationName, Windows, and UserSystems.
      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit Engine(TOptions &&...options) {
         (applyOption(std::forward<TOptions>(options)), ...);
         applyDefaults();
      }

      /// @brief Returns the major engine version.
      [[nodiscard]] std::uint32_t versionMajor() const { return 4; }
      /// @brief Returns the major engine version through a v3-shaped accessor.
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept { return 4; }
      /// @brief Returns the printable engine version name.
      [[nodiscard]] std::string_view versionName() const { return "v4"; }
      /// @brief Returns the example-facing world facade.
      [[nodiscard]] World &world() { return world_; }
      /// @brief Returns the example-facing world facade.
      [[nodiscard]] const World &world() const { return world_; }
      /// @brief Returns the asset system.
      [[nodiscard]] AssetSystem &assets() { return assets_; }
      /// @brief Returns the resource system.
      [[nodiscard]] ResourceSystem &resources() { return resources_; }
      /// @brief Returns the task graph.
      [[nodiscard]] TaskGraph &tasks() { return tasks_; }
      /// @brief Returns the render graph.
      [[nodiscard]] RenderGraph &renderGraph() { return render_graph_; }
      /// @brief Returns the shader system.
      [[nodiscard]] ShaderSystem &shaders() { return shaders_; }
      /// @brief Returns the GUI system.
      [[nodiscard]] GuiSystem &gui() { return gui_; }
      /// @brief Returns the runtime ECS.
      [[nodiscard]] ECS &ecs() { return world_.ecs(); }

      /// @brief Creates SDL windows and calls optional user-system init(World&) hooks.
      [[nodiscard]] std::expected<void, Error> init() {
         if (initialized_) {
            return {};
         }
         if (const auto result = window_system_.init(windows_); !result) {
            return result;
         }
         world_.windows() = window_system_.snapshot();
         last_frame_time_ = std::chrono::steady_clock::now();
         initialized_ = true;
         return initSystems();
      }

      /// @brief Runs the engine until a window closes, a system fails, or the frame cap is reached.
      [[nodiscard]] std::expected<void, Error> run() {
         if (!initialized_) {
            if (const auto result = init(); !result) {
               return result;
            }
         }
         while (true) {
            const auto status = step();
            if (!status) {
               return std::unexpected(status.error());
            }
            if (*status == FrameStatus::stopped) {
               return {};
            }
         }
      }

      /// @brief Polls input, updates World window state, and calls optional user-system update hooks.
      [[nodiscard]] std::expected<FrameStatus, Error> step() {
         if (!initialized_) {
            return std::unexpected(Error::missing_object);
         }
         if (const auto result = window_system_.poll(world_.input()); !result) {
            return std::unexpected(result.error());
         }
         world_.windows() = window_system_.snapshot();

         const auto now = std::chrono::steady_clock::now();
         const std::chrono::duration<double> delta = now - last_frame_time_;
         last_frame_time_ = now;

         const FrameContext frame{.frame_index = frame_, .delta_seconds = delta.count()};
         const WindowFrameData window_frame{.windows = world_.windows()};
         if (const auto result = updateSystems(frame, window_frame); !result) {
            return std::unexpected(result.error());
         }

         ++frame_;
         if (window_system_.anyShouldClose() || (max_frames_.value > 0 && frame_ >= max_frames_.value)) {
            return FrameStatus::stopped;
         }
         return FrameStatus::running;
      }

   private:
      /// @brief Applies the compact compatibility config.
      void applyOption(EngineConfig config) {
         application_name_.value = std::move(config.application_name);
         max_frames_.value = config.max_frames;
      }

      /// @brief Applies the application-name typed option.
      void applyOption(ApplicationName option) { application_name_ = std::move(option); }
      /// @brief Applies the frame-cap typed option.
      void applyOption(MaxFrames option) { max_frames_ = option; }
      /// @brief Applies the startup-window typed option.
      void applyOption(Windows option) { windows_ = std::move(option); }
      /// @brief Applies the user-system typed option.
      void applyOption(UserSystems<TSystems...> option) { systems_ = std::move(option.value); }
      /// @brief Ignores unknown option types so examples can evolve one option at a time.
      template <typename TOption> void applyOption(TOption &&) {}

      /// @brief Fills small defaults after options have been applied.
      void applyDefaults() {
         if (windows_.value.empty()) {
            windows_.value.push_back(WindowDesc{});
         }
         for (auto &window : windows_.value) {
            if (window.title == WindowDesc{}.title && application_name_.value != ApplicationName{}.value) {
               window.title = application_name_.value;
            }
         }
      }

      /// @brief Calls init(World&) on each user system when that hook exists.
      [[nodiscard]] std::expected<void, Error> initSystems() {
         auto result = std::expected<void, Error>{};
         std::apply([&](auto &...system) { ((result ? result = initOne(system) : result), ...); }, systems_);
         return result;
      }

      /// @brief Calls the best matching update hook on each user system.
      [[nodiscard]] std::expected<void, Error> updateSystems(const FrameContext &frame,
                                                            const WindowFrameData &window_frame) {
         auto result = std::expected<void, Error>{};
         std::apply([&](auto &...system) {
            ((result ? result = updateOne(system, frame, window_frame) : result), ...);
         }, systems_);
         return result;
      }

      /// @brief Calls one system's init hook if present.
      template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system) {
         if constexpr (requires { system.init(world_); }) {
            return detail::callSystemHook([&]() -> decltype(auto) { return system.init(world_); });
         } else {
            return {};
         }
      }

      /// @brief Calls one system's most expressive update hook if present.
      template <typename TSystem>
      [[nodiscard]] std::expected<void, Error> updateOne(TSystem &system, const FrameContext &frame,
                                                         const WindowFrameData &window_frame) {
         if constexpr (requires { system.update(world_, frame, window_frame); }) {
            return detail::callSystemHook([&]() -> decltype(auto) {
               return system.update(world_, frame, window_frame);
            });
         } else if constexpr (requires { system.update(world_, frame); }) {
            return detail::callSystemHook([&]() -> decltype(auto) { return system.update(world_, frame); });
         } else if constexpr (requires { system.update(world_); }) {
            return detail::callSystemHook([&]() -> decltype(auto) { return system.update(world_); });
         } else {
            return {};
         }
      }

      ApplicationName application_name_{};      ///< Name used for default window titles.
      MaxFrames max_frames_{};                 ///< Optional frame cap.
      Windows windows_{};                      ///< Startup window descriptors.
      World world_{};                          ///< Example-facing runtime state.
      WindowSystem window_system_{};           ///< SDL platform window owner.
      AssetSystem assets_{};                   ///< Asset and object catalog facade.
      ResourceSystem resources_{};             ///< Resource descriptor facade.
      TaskGraph tasks_{};                      ///< CPU task graph facade.
      RenderGraph render_graph_{};             ///< Render pass graph facade.
      ShaderSystem shaders_{};                 ///< Shader descriptor facade.
      GuiSystem gui_{};                        ///< GUI descriptor facade.
      std::tuple<TSystems...> systems_{};      ///< User systems supplied by the application.
      std::chrono::steady_clock::time_point last_frame_time_{}; ///< Timestamp of the previous step().
      std::uint64_t frame_{0};                 ///< Number of completed step() calls.
      bool initialized_{false};                ///< True after init() succeeds.
   };

   namespace detail {

      /// @brief Maps a UserSystems option to the matching Engine specialization.
      template <typename TUserSystems> struct EngineTypeFromUserSystems;

      /// @brief Specialization that injects user-system types into Engine.
      template <typename... TSystems> struct EngineTypeFromUserSystems<UserSystems<TSystems...>> {
         using type = Engine<TSystems...>; ///< Engine specialization for this user-system pack.
      };

   } // namespace detail

   /// @brief Builds a v4 engine from typed options in any order.
   template <typename... TOptions> [[nodiscard]] auto makeEngine(TOptions &&...options) {
      using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
      using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;
      return TEngine(std::forward<TOptions>(options)...);
   }

} // namespace vve::v4
