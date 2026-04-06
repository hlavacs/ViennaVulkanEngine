module;

module VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Explicit template instantiation unit for the public engine facade.
 *
 * Keeping the default engine facade instantiation in its own translation unit
 * reduces repeated code generation for consumers importing the module.
 */

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

/// @brief Instantiates the default engine facade for the selected engine namespace.
template class vve::EngineFacade<vve::VVE_DEFAULT_ENGINE_NAMESPACE::EngineImplementation>;
