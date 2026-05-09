export module VEEngine:World;
import std;
import VEEngine.V4;
import :ECS;
import :Window;
import :Assets;
import :RenderSystem;
import :Gui;

/**
 * @file
 * @brief Public world contract backed by the selected engine implementation.
 */
export namespace vve {

   namespace detail {

      template <typename T> struct IsWorldReference : std::false_type {};
      template <typename T>
      struct IsWorldReference<std::reference_wrapper<T>> : std::bool_constant<!std::is_pointer_v<T>> {};
      template <typename T> inline constexpr bool isWorldReference = IsWorldReference<T>::value;

      template <typename T> struct WorldReferenceTarget {};
      template <typename T> struct WorldReferenceTarget<std::reference_wrapper<T>> { using type = T; };
      template <typename T> using WorldReferenceTargetT = typename WorldReferenceTarget<T>::type;

      template <typename> inline constexpr bool dependentFalse = false;

   } // namespace detail

   template <typename... TObjects> class World {
   public:
      explicit World(TObjects... objects)
         requires((detail::isWorldReference<TObjects> && ...))
          : objects_{objects...} {}

      World(const World &) = delete;
      World(World &&) noexcept = default;
      World &operator=(const World &) = delete;
      World &operator=(World &&) = delete;

      template <typename TObject> [[nodiscard]] decltype(auto) get() {
         return getFrom<TObject, 0>(objects_);
      }

      template <typename TObject> [[nodiscard]] decltype(auto) get() const {
         return getFrom<TObject, 0>(objects_);
      }

   private:
      template <typename TRequested, std::size_t TIndex, typename TTuple>
      [[nodiscard]] static decltype(auto) getFrom(TTuple &objects) {
         using Tuple = std::remove_reference_t<TTuple>;
         if constexpr (TIndex >= std::tuple_size_v<Tuple>) {
            static_assert(detail::dependentFalse<TRequested>, "Requested type is not stored in this World.");
         } else {
            using Stored = std::remove_cvref_t<std::tuple_element_t<TIndex, Tuple>>;
            using Target = detail::WorldReferenceTargetT<Stored>;
            if constexpr (std::same_as<TRequested, Target>) {
               return std::get<TIndex>(objects).get();
            } else {
               return getFrom<TRequested, TIndex + 1>(objects);
            }
         }
      }

      std::tuple<TObjects...> objects_; ///< Public wrappers and user-supplied system references.
   }; ///< Facade world type.

   template <typename... TSystems> struct UserSystems {
      std::tuple<TSystems...> value{}; ///< User systems stored by value.
   }; ///< User systems bundle.

   struct MakeUserSystems {
      template <typename... TSystems> [[nodiscard]] auto operator()(TSystems &&...systems) const {
         return UserSystems<std::remove_cvref_t<TSystems>...>{
            .value = std::tuple<std::remove_cvref_t<TSystems>...>{std::forward<TSystems>(systems)...}};
      }
   }; ///< Callable facade user-system bundle factory.

   inline constexpr MakeUserSystems makeUserSystems{}; ///< Facade user-system bundle factory.

} // namespace vve
