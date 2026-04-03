module;

module VEEngine;
import VEEngine.V3;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

template class vve::EngineFacade<vve::VVE_DEFAULT_ENGINE_NAMESPACE::EngineImplementation>;
