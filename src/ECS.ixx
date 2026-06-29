export module VEEngine:ECS;
import VEEngine.ECSContainer;

/**
	* @file
	* @brief Public ECS contract re-exported from the facade-owned ECS container.
	*/
export namespace vve {

	using vve::DefaultECSTraits;	///< Facade ECS traits.
	using vve::BasicECS;			///< Facade ECS template.
	using vve::ECS;					///< Default facade ECS.

} // namespace vve
