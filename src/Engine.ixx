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
export import :World;
export import :Handle;
export import :Math;
export import :Error;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

/**
 * @file
 * @brief Public engine facade and configuration surface.
 *
 * This module is the main consumer entry point for engine construction,
 * initialization, and frame stepping. Version-specific implementation detail
 * stays behind the selected engine namespace alias.
 */
export namespace vve::v3 {

   template <typename... TUserSystems> class BasicEngineImplementation;

   /// @brief Default concrete engine implementation for the active v3 namespace.
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

   /// @brief Graphics backend requested by the caller.
   enum class GraphicsApi {
      vulkan, ///< Vulkan backend selection.
      direct3d12, ///< Direct3D 12 backend selection.
      metal ///< Metal backend selection.
   };

   /// @brief High-level renderer pipeline family requested by the caller.
   enum class RendererKind {
      forward_renderer, ///< Forward-renderer configuration.
      deferred_renderer, ///< Deferred-renderer configuration.
      path_tracing ///< Path-tracing configuration.
   };

   /// @brief Shadowing strategy requested by the caller.
   enum class ShadowKind {
      none, ///< No shadowing.
      shadow_map, ///< Shadow-map-based shadows.
      ray_traced ///< Ray-traced shadows.
   };

   /// @brief Result of a single engine step.
   enum class FrameStatus {
      continue_running = 0, ///< Engine should continue processing more frames.
      should_close ///< Engine requested shutdown.
   };

   /// @brief Human-readable application name shown to runtime subsystems.
   struct ApplicationName {
      std::string value;	///< Human-readable application name.
   };

   /// @brief Enables additional validation in supporting subsystems.
   struct EnableValidation {
      bool value = false;	///< Enables additional validation when supported.
   };

   /// @brief Selects the preferred graphics API for runtime creation.
   struct PreferredGraphicsApi {
      GraphicsApi value = GraphicsApi::vulkan;	///< Requested graphics API.
   };

   /// @brief Selects the preferred renderer family.
   struct PreferredRenderer {
      RendererKind value = RendererKind::forward_renderer;	///< Requested renderer kind.
   };

   /// @brief Selects the preferred shadowing mode.
   struct PreferredShadow {
      ShadowKind value = ShadowKind::none;	///< Requested shadow mode.
   };

   /// @brief Enables or disables Dear ImGui integration.
   struct EnableImGui {
      bool value = true;	///< Enables Dear ImGui integration.
   };

   /**
    * @brief Window creation descriptor.
    *
    * The public engine API treats windows as explicit runtime resources rather
    * than implicit global state.
    */
   struct WindowDesc {
      std::string id{"main"};									///< Stable window identifier.
      std::string title{"Vienna Vulkan Engine V3"};	///< Human-readable window title.
      std::uint32_t width{1280};								///< Initial window width in pixels.
      std::uint32_t height{720};								///< Initial window height in pixels.
      bool resizable{true};									///< Whether the platform window may be resized.
      bool visible{true};										///< Whether the window is visible on creation.
   };

   /// @brief Collection wrapper used to configure all initial windows.
   struct Windows {
      std::vector<WindowDesc> value{};	///< List of windows created at engine startup.
   };

   /// @brief Minimal concept required for user-provided frame systems.
   template <typename T>
   concept UserSystemLike = requires(const std::remove_cvref_t<T> &system) {
      { system.name() } -> std::convertible_to<std::string_view>;
   };

   /// @brief Heterogeneous container for user systems supplied at engine creation.
   template <UserSystemLike... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{};	///< Tuple storing the supplied user-system instances.
   };

   /// @brief Convenience helper that preserves user system value types.
   template <UserSystemLike... TSystems> [[nodiscard]] auto makeUserSystems(TSystems &&...systems) {
      return UserSystems<std::remove_cvref_t<TSystems>...>{
          .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
   }

   /**
    * @brief Type-indexed option bag used to build an engine instance.
    *
    * Each option type is unique by its concrete C++ type. Later calls to
    * `set()` replace earlier options of the same type.
    */
   class EngineConfig {
   public:
      /// @brief Constructs an empty configuration that uses engine defaults.
      EngineConfig() = default; 

      /// @brief Constructs a configuration by applying each provided option in order.
      template <typename... TOptions> explicit EngineConfig(TOptions &&...options) {
         (set(std::forward<TOptions>(options)), ...);
      }

      /// @brief Stores or replaces an option keyed by its concrete type.
      template <typename TOption> EngineConfig &set(TOption &&option) {
         using TStoredOption = std::remove_cvref_t<TOption>;
         options_[std::type_index(typeid(TStoredOption))] = std::forward<TOption>(option);
         return *this;
      }

      /// @brief Returns an option copy when the requested type has been configured.
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

   /**
    * @brief Public engine lifecycle facade.
    *
    * This facade owns the selected engine implementation and forwards
    * initialization and frame loop control through a stable API.
    */
   template <typename TImplementation> class VVE_API EngineFacade {
   public:
      /// @brief Creates an engine from a pre-built configuration object.
      explicit EngineFacade(EngineConfig config = {});

      /// @brief Creates an engine by collecting typed option arguments into `EngineConfig`.
      template <typename... TOptions>
         requires(sizeof...(TOptions) > 0)
      explicit EngineFacade(TOptions &&...options) : EngineFacade(EngineConfig(std::forward<TOptions>(options)...)) {}

      EngineFacade(const EngineFacade &) = delete;
      EngineFacade(EngineFacade &&) = delete;
      EngineFacade &operator=(const EngineFacade &) = delete;
      EngineFacade &operator=(EngineFacade &&) = delete;

      /// @brief Initializes runtime subsystems and prepares the engine for execution.
      [[nodiscard]] std::expected<void, Error> init();
      /// @brief Runs the main loop until the implementation requests shutdown or returns an error.
      [[nodiscard]] std::expected<void, Error> run();
      /// @brief Executes one frame worth of engine work.
      [[nodiscard]] std::expected<FrameStatus, Error> step();
      /// @brief Returns whether initialization completed successfully.
      [[nodiscard]] std::expected<bool, Error> isInitialized() const noexcept;
      /// @brief Returns the major version number of the active engine implementation.
      [[nodiscard]] std::expected<int, Error> getVersionMajor() const noexcept;

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

   /// @brief Default engine facade alias for the selected engine namespace.
   template <typename TImplementation = detail::DefaultEngineImplementation>
   using Engine = EngineFacade<TImplementation>;

   /**
    * @brief Builds an engine facade from typed option arguments.
    *
    * If a `UserSystems<...>` option is present, the returned engine type is
    * specialized so those systems participate in the frame task graph.
    */
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

} // namespace vve
