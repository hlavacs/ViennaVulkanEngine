module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

export module VEEngine:Graph;
export import :V4Graph;

/**
 * @file
 * @brief Public graph and tree topology facades backed by the active engine version.
 */
export namespace vve {

   template <typename THandle>
   using HandleHash = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::HandleHash<THandle>; ///< Active handle hasher.
   template <typename THandle>
   using BasicTree = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::BasicTree<THandle>; ///< Active tree facade.
   template <typename THandle>
   using Graph = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Graph<THandle>; ///< Active graph facade.

} // namespace vve
