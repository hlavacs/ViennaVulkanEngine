module;

module VEEngine:World;
import std;

vve::World::World(vve::ECS<> &ecs) noexcept : ecs_(&ecs) {}

vve::ECS<> &vve::World::ecs() noexcept { return *ecs_; }

const vve::ECS<> &vve::World::ecs() const noexcept { return *ecs_; }

std::expected<vve::Handle, vve::Error> vve::World::createEntity() { return ecs().create(); }

std::expected<vve::Handle, vve::Error> vve::World::createObject() { return createEntity(); }

std::expected<bool, vve::Error> vve::World::exists(vve::Handle entity) const { return ecs().exists(entity); }

std::expected<void, vve::Error> vve::World::destroyEntity(vve::Handle entity) { return ecs().erase(entity); }

std::expected<void, vve::Error> vve::World::destroyObject(vve::Handle entity) { return destroyEntity(entity); }
