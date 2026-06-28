export module VEEngine.Simple.ECS;
import std;
export import VEEngine.V5:ECS;

/**
	* @file
	* @brief Simple-engine ECS aliases backed by the v5 entity-component helper.
	*
	* Functional objects:
	* - Entity names the shared facade-visible entity identity.
	* - DefaultECSTraits, BasicECS, and ECS name the reused v5 registry surface.
	* - Error and Vector name the result and view types used by the ECS operations.
	*
	* The simple engine reuses the v5 ECS directly. Entity lifetime and arbitrary component
	* storage stay in `VEEngine.V5:ECS`; scene graph links, asset ownership, and renderer state
	* remain separate subsystem responsibilities.
	*/
export namespace vve::simple {

	using Entity = vve::v5::Entity;						  ///< Shared entity identity used as the ECS component key.
	using Error = vve::v5::Error;							  ///< Shared operation error type returned by ECS functions.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared vector result type used by ECS views.

	using DefaultECSTraits = vve::v5::DefaultECSTraits; ///< Shared ECS policy traits for registry construction.

	template <typename TTraits = DefaultECSTraits>
	using BasicECS = vve::v5::BasicECS<TTraits>; ///< Shared registry with create, erase, component, and view operations.

	using ECS = vve::v5::ECS; ///< Default shared ECS registry type for simple-engine entity/component storage.

} // namespace vve::simple
