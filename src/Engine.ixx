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

/// @file
/// @brief Public engine facade; users import this module and use only namespace vve.

export namespace vve {

   inline constexpr std::string_view engineImplementationNamespaceName{
      VVE_DETAIL_STRINGIFY(VVE_ENGINE_IMPLEMENTATION_NAMESPACE)}; ///< Active implementation namespace name.

   template <typename... TSystems> class Engine;

   class AssetSystem {
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem;

   public:
      AssetSystem() : impl_{std::make_shared<Impl>()} {}
      explicit AssetSystem(Impl &implementation)
         : impl_{std::shared_ptr<Impl>(std::addressof(implementation), [](Impl *) {})} {}
      AssetSystem(const AssetSystem &) = delete;
      AssetSystem(AssetSystem &&) noexcept = default;
      AssetSystem &operator=(const AssetSystem &) = delete;
      AssetSystem &operator=(AssetSystem &&) noexcept = default;

      [[nodiscard]] std::expected<SceneHandle, Error> addScene(ObjectName name) {
         return impl_->addScene(std::move(name));
      }
      [[nodiscard]] std::expected<SceneHandle, Error> loadScene(const std::filesystem::path &source) {
         return impl_->loadScene(source);
      }
      [[nodiscard]] bool containsScene(SceneHandle scene) const { return impl_->containsScene(scene); }
      [[nodiscard]] std::expected<ObjectName, Error> sceneName(SceneHandle scene) const {
         return impl_->sceneName(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneNodeCount(SceneHandle scene) const {
         return impl_->sceneNodeCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneMeshCount(SceneHandle scene) const {
         return impl_->sceneMeshCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneMaterialCount(SceneHandle scene) const {
         return impl_->sceneMaterialCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneTextureCount(SceneHandle scene) const {
         return impl_->sceneTextureCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneLightCount(SceneHandle scene) const {
         return impl_->sceneLightCount(scene);
      }
      [[nodiscard]] std::expected<std::size_t, Error> sceneCameraCount(SceneHandle scene) const {
         return impl_->sceneCameraCount(scene);
      }

   private:
      std::shared_ptr<Impl> impl_{};
   }; ///< Public asset importer.

   template <typename... TSystems> class Engine {
   public:
      Engine() = default;

      Engine(const Engine &) = delete;
      Engine(Engine &&) noexcept = default;
      Engine &operator=(const Engine &) = delete;
      Engine &operator=(Engine &&) noexcept = default;

      explicit Engine(EngineConfig config) : impl_{std::move(config)} {}

      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit Engine(TOptions &&...options) : impl_{implementationOption(std::forward<TOptions>(options))...} {}

      [[nodiscard]] std::uint32_t versionMajor() const { return impl_.versionMajor(); }
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept { return impl_.getVersionMajor(); }
      [[nodiscard]] std::string_view versionName() const { return impl_.versionName(); }
      [[nodiscard]] World world() { return World{impl_.world()}; }
      [[nodiscard]] World world() const { return World{const_cast<Impl &>(impl_).world()}; }
      [[nodiscard]] AssetSystem assets() { return AssetSystem{impl_.assets()}; }
      [[nodiscard]] decltype(auto) ecs() { return impl_.ecs(); }

      [[nodiscard]] std::expected<void, Error> init() { return impl_.init(); }
      [[nodiscard]] std::expected<void, Error> run() { return impl_.run(); }
      [[nodiscard]] std::expected<FrameStatus, Error> step() { return impl_.step(); }

   private:
      using Impl = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine<TSystems...>;

      static VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows implementationOption(WindowSetups option) {
         return option;
      }

      template <typename TOption> static decltype(auto) implementationOption(TOption &&option) {
         return std::forward<TOption>(option);
      }

      Impl impl_{};
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

} // namespace vve
