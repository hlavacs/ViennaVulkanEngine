export module VEEngine.Error;
import std;
#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5.Error;
#else
import VEEngine.V4.Error;
#endif

/**
 * @file
 * @brief Public error contract backed by the selected engine implementation.
 */
export namespace vve {

   using Error = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Error; ///< Facade error vocabulary.
   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::errorName;     ///< Facade diagnostic name lookup.

} // namespace vve
