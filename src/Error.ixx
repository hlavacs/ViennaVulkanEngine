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

} // namespace vve
