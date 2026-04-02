module;

module VEEngine;
import VEEngine.V3;

vve::World::World(vve::ECS<>& ecs) noexcept
    : ecs_(&ecs) {
}

vve::ECS<>& vve::World::ecs() noexcept {
    return *ecs_;
}

const vve::ECS<>& vve::World::ecs() const noexcept {
    return *ecs_;
}

template class vve::EngineFacade<vve::v3::EngineImplementation>;
