export module VEEngine.Entity;
import VEEngine.Handle;

/// @file
/// @brief Public ECS entity handle vocabulary shared by facade modules.

export namespace vve {

	struct EntityTag {};													///< Facade ECS entity handle tag.
	using Entity = TypedHandle<EntityTag>;								///< Facade ECS entity.

} // namespace vve
