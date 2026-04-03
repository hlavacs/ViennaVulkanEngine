module;

#if defined(_WIN32)
#if defined(VVE_ENGINE_BUILD)
#define VVE_API __declspec(dllexport)
#else
#define VVE_API __declspec(dllimport)
#endif
#else
#define VVE_API
#endif

export module VEEngine;
import std;
export import :ECS;
export import :Handle;
export import :Math;
export import :Error;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

export namespace vve::v3 {

   template <typename... TUserSystems> class BasicEngineImplementation;

   using EngineImplementation = BasicEngineImplementation<>;

} // namespace vve::v3

export namespace vve {

   namespace detail {

      using DefaultEngineImplementation = vve::VVE_DEFAULT_ENGINE_NAMESPACE::EngineImplementation;

      template <typename... TSystems>
      using DefaultEngineImplementationTemplate =
          vve::VVE_DEFAULT_ENGINE_NAMESPACE::BasicEngineImplementation<TSystems...>;

   } // namespace detail

   // Engine configuration options

   enum class GraphicsApi { vulkan, direct3d12, metal };

   enum class RendererKind { forward_renderer, deferred_renderer, path_tracing };

   enum class ShadowKind { none, shadow_map, ray_traced };

   enum class FrameStatus { continue_running = 0, should_close };

   /// @brief Configuration for creating an Engine instance. Can be constructed
   /// with arbitrary options using the set() method or the variadic
   /// constructor.
   struct ApplicationName {
      std::string value;
   };

   struct EnableValidation {
      bool value = false;
   };

   struct PreferredGraphicsApi {
      GraphicsApi value = GraphicsApi::vulkan;
   };

   struct PreferredRenderer {
      RendererKind value = RendererKind::forward_renderer;
   };

   struct PreferredShadow {
      ShadowKind value = ShadowKind::none;
   };

   struct EnableImGui {
      bool value = true;
   };

   struct WindowDesc {
      std::string id{"main"};
      std::string title{"Vienna Vulkan Engine V3"};
      std::uint32_t width{1280};
      std::uint32_t height{720};
      bool resizable{true};
      bool visible{true};
   };

   struct Windows {
      std::vector<WindowDesc> value{};
   };

   template <typename T>
   concept UserSystemLike = requires(const std::remove_cvref_t<T> &system) {
      { system.name() } -> std::convertible_to<std::string_view>;
   };

   template <UserSystemLike... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{};
   };

   template <UserSystemLike... TSystems> [[nodiscard]] auto makeUserSystems(TSystems &&...systems) {
      return UserSystems<std::remove_cvref_t<TSystems>...>{
          .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
   }

   class VVE_API World {
   public:
      explicit World(ECS<> &ecs) noexcept;

      [[nodiscard]] ECS<> &ecs() noexcept;

      [[nodiscard]] const ECS<> &ecs() const noexcept;

   private:
      ECS<> *ecs_;
   };

   class EngineConfig {
   public:
      EngineConfig() = default;

      template <typename... TOptions> explicit EngineConfig(TOptions &&...options) {
         (set(std::forward<TOptions>(options)), ...);
      }

      template <typename TOption> EngineConfig &set(TOption &&option) {
         using TStoredOption = std::remove_cvref_t<TOption>;
         options_[std::type_index(typeid(TStoredOption))] = std::forward<TOption>(option);
         return *this;
      }

      template <typename TOption> [[nodiscard]] std::optional<TOption> tryGet() const {
         const auto entry = options_.find(std::type_index(typeid(TOption)));
         if (entry == options_.end()) {
            return std::nullopt;
         }

         if (const auto *value = std::any_cast<TOption>(&entry->second)) {
            return *value;
         }

         return std::nullopt;
      }

   private:
      std::unordered_map<std::type_index, std::any> options_;
   };

   template <typename TImplementation> class VVE_API EngineFacade {
   public:
      explicit EngineFacade(EngineConfig config = {});

      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit EngineFacade(TOptions &&...options) : EngineFacade(EngineConfig(std::forward<TOptions>(options)...)) {}

      EngineFacade(const EngineFacade &) = delete;
      EngineFacade(EngineFacade &&) = delete;
      EngineFacade &operator=(const EngineFacade &) = delete;
      EngineFacade &operator=(EngineFacade &&) = delete;

      [[nodiscard]] std::expected<void, Error> init();
      [[nodiscard]] std::expected<void, Error> run();
      [[nodiscard]] std::expected<FrameStatus, Error> step();
      [[nodiscard]] std::expected<bool, Error> isInitialized() const noexcept;
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept;
      [[nodiscard]] std::expected<void, Error> loadFile(const std::filesystem::path &file_path);

   private:
      TImplementation implementation_;
   };

   namespace detail {

      template <typename T> struct IsUserSystemsOption : std::false_type {};

      template <UserSystemLike... TSystems> struct IsUserSystemsOption<UserSystems<TSystems...>> : std::true_type {};

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

      template <UserSystemLike... TSystems> struct EngineTypeFromUserSystems<UserSystems<TSystems...>> {
         using type = EngineFacade<DefaultEngineImplementationTemplate<TSystems...>>;
      };

   } // namespace detail

   template <typename TImplementation = detail::DefaultEngineImplementation>
   using Engine = EngineFacade<TImplementation>;

   template <typename... TOptions> [[nodiscard]] auto makeEngine(TOptions &&...options) {
      using TUserSystems = typename detail::FindUserSystemsOption<UserSystems<>, TOptions...>::type;
      using TEngine = typename detail::EngineTypeFromUserSystems<TUserSystems>::type;

      return TEngine(EngineConfig(std::forward<TOptions>(options)...));
   }

   template <typename TImplementation>
   EngineFacade<TImplementation>::EngineFacade(EngineConfig config) : implementation_(std::move(config)) {}

   template <typename TImplementation> [[nodiscard]] std::expected<void, Error> EngineFacade<TImplementation>::init() {
      return implementation_.init();
   }

   template <typename TImplementation> [[nodiscard]] std::expected<void, Error> EngineFacade<TImplementation>::run() {
      return implementation_.run();
   }

   template <typename TImplementation>
   [[nodiscard]] std::expected<FrameStatus, Error> EngineFacade<TImplementation>::step() {
      return implementation_.step();
   }

   template <typename TImplementation>
   [[nodiscard]] std::expected<bool, Error> EngineFacade<TImplementation>::isInitialized() const noexcept {
      return implementation_.isInitialized();
   }

   template <typename TImplementation>
   [[nodiscard]] std::expected<int, Error> EngineFacade<TImplementation>::getVersionMajor() const noexcept {
      return implementation_.getVersionMajor();
   }

   template <typename TImplementation>
   [[nodiscard]] std::expected<void, Error>
   EngineFacade<TImplementation>::loadFile(const std::filesystem::path &file_path) {
      return implementation_.loadFile(file_path);
   }

} // namespace vve
