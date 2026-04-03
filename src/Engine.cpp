module;

module VEEngine;
import VEEngine.V3;

#ifndef VVE_DEFAULT_ENGINE_NAMESPACE
#define VVE_DEFAULT_ENGINE_NAMESPACE v3
#endif

vve::World::World(vve::ECS<> &ecs) noexcept : ecs_(&ecs) {}

vve::ECS<> &vve::World::ecs() noexcept { return *ecs_; }

const vve::ECS<> &vve::World::ecs() const noexcept { return *ecs_; }

template class vve::EngineFacade<vve::VVE_DEFAULT_ENGINE_NAMESPACE::EngineImplementation>;
