export module VEEngine:Error;
import std;
import VEEngine.V4;

/**
 * @file
 * @brief Public error contract backed by the selected engine implementation.
 */
export namespace vve {

   using Error = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Error; ///< Facade error vocabulary.
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::errorName;     ///< Facade diagnostic name lookup.

   template <typename T> concept ErrorLike =
      std::same_as<std::remove_cvref_t<T>, Error>; ///< Contract for facade error enums.

   template <typename = void> concept ErrorNameFunctionLike =
      requires(Error error) {
         { errorName(error) } -> std::convertible_to<std::string_view>;
      }; ///< Contract for the errorName(Error) diagnostic function.

} // namespace vve
