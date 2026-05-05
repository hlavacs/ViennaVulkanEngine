module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine;
import std;
import VEEngine.V4;
export import :Error;
export import :Math;
export import :Handle;
export import :Vector;
export import :Types;
export import :Graph;
export import :ECS;
export import :Window;
export import :World;

/// @file
/// @brief Public engine facade; users import this module and use only namespace vve.

export namespace vve {

   inline constexpr std::string_view engineImplementationNamespaceName{
      VVE_DETAIL_STRINGIFY(VVE_ENGINE_IMPLEMENTATION_NAMESPACE)}; ///< Active implementation namespace name.

   using GuiWidget       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiWidget;       ///< Public GUI widget data.
   using GuiWidgetHandle = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiWidgetHandle; ///< GUI widget handle.

   template <typename... TSystems> class Engine;

   class AssetSystem {
   public:
      AssetSystem() = default;
      AssetSystem(const AssetSystem &) = delete;
      AssetSystem(AssetSystem &&other) noexcept : owned_{std::move(other.owned_)} {
         impl_ = other.impl_ == std::addressof(other.owned_) ? std::addressof(owned_) : other.impl_;
      }
      AssetSystem &operator=(const AssetSystem &) = delete;
      AssetSystem &operator=(AssetSystem &&other) noexcept {
         if (this != std::addressof(other)) {
            owned_ = std::move(other.owned_);
            impl_ = other.impl_ == std::addressof(other.owned_) ? std::addressof(owned_) : other.impl_;
         }
         return *this;
      }

      [[nodiscard]] ObjectCatalog &catalog() { return impl().catalog(); }
      [[nodiscard]] const ObjectCatalog &catalog() const { return impl().catalog(); }
      [[nodiscard]] std::expected<SceneHandle, Error> addScene(ObjectName name) {
         return impl().addScene(std::move(name));
      }
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source) {
         return impl().loadScene(source);
      }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem;

      explicit AssetSystem(Impl &implementation) noexcept : impl_{std::addressof(implementation)} {}

      [[nodiscard]] Impl &impl() { return *impl_; }
      [[nodiscard]] const Impl &impl() const { return *impl_; }

      Impl owned_{};
      Impl *impl_{std::addressof(owned_)};

      template <typename... TSystems> friend class Engine;
   }; ///< Public asset importer.

   class GuiSystem {
   public:
      GuiSystem() = default;
      GuiSystem(const GuiSystem &) = delete;
      GuiSystem(GuiSystem &&other) noexcept : owned_{std::move(other.owned_)} {
         impl_ = other.impl_ == std::addressof(other.owned_) ? std::addressof(owned_) : other.impl_;
      }
      GuiSystem &operator=(const GuiSystem &) = delete;
      GuiSystem &operator=(GuiSystem &&other) noexcept {
         if (this != std::addressof(other)) {
            owned_ = std::move(other.owned_);
            impl_ = other.impl_ == std::addressof(other.owned_) ? std::addressof(owned_) : other.impl_;
         }
         return *this;
      }

      [[nodiscard]] std::expected<GuiWidgetHandle, Error> label(std::string text) {
         return impl().label(std::move(text));
      }
      [[nodiscard]] const GuiWidget *find(GuiWidgetHandle handle) const { return impl().find(handle); }
      [[nodiscard]] std::size_t size() const { return impl().size(); }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem;

      explicit GuiSystem(Impl &implementation) noexcept : impl_{std::addressof(implementation)} {}

      [[nodiscard]] Impl &impl() { return *impl_; }
      [[nodiscard]] const Impl &impl() const { return *impl_; }

      Impl owned_{};
      Impl *impl_{std::addressof(owned_)};

      template <typename... TSystems> friend class Engine;
   }; ///< Public GUI descriptor system.

   template <typename... TSystems> class Engine {
   public:
      Engine() : ecs_{impl_.ecs()}, world_{impl_.world(), ecs_}, assets_{impl_.assets()}, gui_{impl_.gui()} {}

      Engine(const Engine &) = delete;
      Engine(Engine &&) = delete;
      Engine &operator=(const Engine &) = delete;
      Engine &operator=(Engine &&) = delete;

      explicit Engine(EngineConfig config)
         : impl_{std::move(config)}, ecs_{impl_.ecs()}, world_{impl_.world(), ecs_}, assets_{impl_.assets()},
           gui_{impl_.gui()} {}

      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit Engine(TOptions &&...options)
         : impl_{std::forward<TOptions>(options)...}, ecs_{impl_.ecs()}, world_{impl_.world(), ecs_},
           assets_{impl_.assets()}, gui_{impl_.gui()} {
         (applyOption(std::forward<TOptions>(options)), ...);
      }

      [[nodiscard]] std::uint32_t versionMajor() const { return impl_.versionMajor(); }
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept { return impl_.getVersionMajor(); }
      [[nodiscard]] std::string_view versionName() const { return impl_.versionName(); }
      [[nodiscard]] World &world() { return world_; }
      [[nodiscard]] const World &world() const { return world_; }
      [[nodiscard]] AssetSystem &assets() { return assets_; }
      [[nodiscard]] GuiSystem &gui() { return gui_; }
      [[nodiscard]] ECS &ecs() { return ecs_; }

      [[nodiscard]] std::expected<void, Error> init() {
         if (initialized_) { return {}; }
         if (const auto result = impl_.init(); !result) { return result; }
         initialized_ = true;
         return initSystems();
      }

      [[nodiscard]] std::expected<void, Error> run() {
         if (!initialized_) {
            if (const auto result = init(); !result) { return result; }
         }
         while (true) {
            const auto status = step();
            if (!status) { return std::unexpected(status.error()); }
            if (*status == FrameStatus::stopped) { return {}; }
         }
      }

      [[nodiscard]] std::expected<FrameStatus, Error> step() {
         const auto status = impl_.step();
         if (!status) { return std::unexpected(status.error()); }
         const FrameContext frame{.frame_index = FrameCount{.value = frame_}, .delta_time = DeltaTime{}};
         const WindowFrameData window_frame{.windows = world_.windows()};
         if (const auto result = updateSystems(frame, window_frame); !result) {
            return std::unexpected(result.error());
         }
         ++frame_;
         return *status;
      }

   private:
      void applyOption(UserSystems<TSystems...> option) { systems_.emplace(std::move(option.value)); }
      template <typename TOption> void applyOption(TOption &&) {}

      template <typename TCallable> [[nodiscard]] std::expected<void, Error> callSystemHook(TCallable &&callable) {
         using TResult = std::invoke_result_t<TCallable>;
         if constexpr (std::same_as<TResult, std::expected<void, Error>>) {
            return std::invoke(std::forward<TCallable>(callable));
         } else {
            std::invoke(std::forward<TCallable>(callable));
            return {};
         }
      }

      [[nodiscard]] std::expected<void, Error> initSystems() {
         if (!systems_.has_value()) { return {}; }
         auto result = std::expected<void, Error>{};
         std::apply([&](auto &...system) { ((result ? result = initOne(system) : result), ...); }, *systems_);
         return result;
      }

      [[nodiscard]] std::expected<void, Error> updateSystems(const FrameContext &frame,
                                                            const WindowFrameData &window_frame) {
         if (!systems_.has_value()) { return {}; }
         auto result = std::expected<void, Error>{};
         std::apply([&](auto &...system) {
            ((result ? result = updateOne(system, frame, window_frame) : result), ...);
         }, *systems_);
         return result;
      }

      template <typename TSystem> [[nodiscard]] std::expected<void, Error> initOne(TSystem &system) {
         if constexpr (requires { system.init(world_); }) {
            return callSystemHook([&]() -> decltype(auto) { return system.init(world_); });
         } else {
            return {};
         }
      }

      template <typename TSystem>
      [[nodiscard]] std::expected<void, Error> updateOne(TSystem &system, const FrameContext &frame,
                                                         const WindowFrameData &window_frame) {
         if constexpr (requires { system.update(world_, frame, window_frame); }) {
            return callSystemHook([&]() -> decltype(auto) { return system.update(world_, frame, window_frame); });
         } else if constexpr (requires { system.update(world_, frame); }) {
            return callSystemHook([&]() -> decltype(auto) { return system.update(world_, frame); });
         } else if constexpr (requires { system.update(world_); }) {
            return callSystemHook([&]() -> decltype(auto) { return system.update(world_); });
         } else {
            return {};
         }
      }

      VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine<> impl_{};
      ECS ecs_;
      World world_;
      AssetSystem assets_;
      GuiSystem gui_;
      std::optional<std::tuple<TSystems...>> systems_{};
      std::uint64_t frame_{0};
      bool initialized_{false};
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

   } // namespace detail

   struct MakeEngine {
      template <typename... TOptions> [[nodiscard]] auto operator()(TOptions &&...options) const {
         using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
         using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;
         return TEngine(std::forward<TOptions>(options)...);
      }
   }; ///< Callable facade engine factory.

   inline constexpr MakeEngine makeEngine{}; ///< Facade engine factory.

   template <typename T> concept GuiWidgetHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, GuiWidgetHandle>; ///< GUI widget handle contract.

   template <typename T> concept GuiWidgetLike = requires(T widget) {
      typename T::HandleType;
      { widget.handle } -> std::same_as<GuiWidgetHandle &>;
      { widget.label } -> std::same_as<std::string &>;
   }; ///< Contract for GUI widget descriptors.

   template <typename T> concept GuiSystemLike = requires(T gui, GuiWidgetHandle handle, std::string label) {
      { gui.label(label) } -> std::same_as<std::expected<GuiWidgetHandle, Error>>;
      { gui.find(handle) } -> std::same_as<const GuiWidget *>;
      { gui.size() } -> std::convertible_to<std::size_t>;
   }; ///< Contract for the public GUI system.

   template <typename T> concept AssetSystemLike =
      requires(T assets, ObjectName name, std::filesystem::path path) {
         { assets.catalog() } -> std::same_as<ObjectCatalog &>;
         { assets.addScene(name) } -> std::same_as<std::expected<SceneHandle, Error>>;
         { assets.loadScene(path) } -> std::same_as<std::expected<SceneHandle, Error>>;
      }; ///< Contract for the public asset system.

   template <typename TEngine> concept EngineLike = requires(TEngine engine) {
      { engine.versionMajor() } -> std::convertible_to<std::uint32_t>;
      { engine.getVersionMajor() } -> std::same_as<std::expected<int, Error>>;
      { engine.versionName() } -> std::convertible_to<std::string_view>;
      { engine.world() } -> std::same_as<World &>;
      { engine.assets() } -> std::same_as<AssetSystem &>;
      { engine.gui() } -> std::same_as<GuiSystem &>;
      { engine.ecs() } -> std::same_as<ECS &>;
      { engine.init() } -> std::same_as<std::expected<void, Error>>;
      { engine.run() } -> std::same_as<std::expected<void, Error>>;
      { engine.step() } -> std::same_as<std::expected<FrameStatus, Error>>;
   }; ///< Contract for the public engine facade.

   template <typename... TOptions> concept MakeEngineFunctionLike = requires(TOptions... options) {
      { makeEngine(options...) };
   }; ///< Contract for makeEngine(...).

   static_assert(AssetSystemLike<AssetSystem>);
   static_assert(EngineLike<Engine<>>);
   static_assert(GuiSystemLike<GuiSystem>);
   static_assert(GuiWidgetHandleLike<GuiWidgetHandle>);
   static_assert(GuiWidgetLike<GuiWidget>);
   static_assert(MakeEngineFunctionLike<>);
   static_assert(MakeEngineFunctionLike<ApplicationName, MaxFrames>);

} // namespace vve
