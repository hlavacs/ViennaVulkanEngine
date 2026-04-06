module;

/**
 * @file
 * @brief Legacy world facade implementation unit.
 *
 * The modern public `WorldFacade` implementation is inline in the module
 * interface. This file retains the explicit out-of-line definitions for the
 * non-versioned world facade.
 */
module VEEngine:World;
import std;

/// @brief Creates a world bound to an ECS facade.
vve::World::World(vve::ECS<> &ecs) noexcept : ecs_(&ecs) {}

/// @brief Returns mutable access to the backing ECS facade.
vve::ECS<> &vve::World::ecs() noexcept { return *ecs_; }

/// @brief Returns read-only access to the backing ECS facade.
const vve::ECS<> &vve::World::ecs() const noexcept { return *ecs_; }

/// @brief Creates a new entity in the backing ECS.
std::expected<vve::Handle, vve::Error> vve::World::createEntity() { return ecs().create(); }

/// @brief Creates a new object entity.
std::expected<vve::Handle, vve::Error> vve::World::createObject() { return createEntity(); }

/// @brief Returns whether the entity exists in ECS.
std::expected<bool, vve::Error> vve::World::exists(vve::Handle entity) const { return ecs().exists(entity); }

/// @brief Destroys an entity in ECS.
std::expected<void, vve::Error> vve::World::destroyEntity(vve::Handle entity) { return ecs().erase(entity); }

/// @brief Destroys an object entity.
std::expected<void, vve::Error> vve::World::destroyObject(vve::Handle entity) { return destroyEntity(entity); }
