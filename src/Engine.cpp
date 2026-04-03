module;

module VEEngine;
import VEEngine.V3;

#ifndef VVE_DEFAULT_ENGINE_IMPLEMENTATION
#define VVE_DEFAULT_ENGINE_IMPLEMENTATION 3
#endif

vve::World::World(vve::ECS<> &ecs) noexcept : ecs_(&ecs) {}

vve::ECS<> &vve::World::ecs() noexcept { return *ecs_; }

const vve::ECS<> &vve::World::ecs() const noexcept { return *ecs_; }

#if VVE_DEFAULT_ENGINE_IMPLEMENTATION == 3
template class vve::EngineFacade<vve::v3::EngineImplementation>;
#else
#error Unsupported VVE_DEFAULT_ENGINE_IMPLEMENTATION value
#endif
