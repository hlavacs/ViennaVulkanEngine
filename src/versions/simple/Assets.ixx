export module VEEngine.Simple.Assets;
import std;
export import VEEngine.V5:Assets;

/**
	* @file
	* @brief Simple-engine asset aliases backed by the v5 Assimp asset module.
	*
	* Functional objects:
	* - AssetSystem names the reused v5 asset manager surface.
	* - SceneHandle, NodeHandle, MeshHandle, MaterialHandle, TextureHandle, LightHandle, and CameraHandle
	*   name the reused asset and imported-object identities.
	* - Error, ObjectName, Entity, Vector, Vec2, Vec3, Transform, Bounds, VertexCount, and IndexCount name
	*   the descriptor-free result, label, list, math, and count helpers used by the public asset API.
	*
	* The simple engine reuses `VEEngine.V5:Assets` directly for asset access. Loading, purging,
	* object creation, Assimp ownership, descriptor storage, and catalogues remain implemented by v5.
	*/
export namespace vve::simple {

	using Error = vve::v5::Error;				///< Shared operation error type returned by asset functions.
	using ObjectName = vve::v5::ObjectName; ///< Shared imported scene, node, mesh, and material label type.
	using Entity = vve::v5::Entity;			///< Shared ECS identity available to asset-facing callers.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared list container used by asset query results.

	using Vec2 = vve::v5::Vec2;							///< Shared texture-coordinate value type.
	using Vec3 = vve::v5::Vec3;							///< Shared position and normal value type.
	using Transform = vve::v5::Transform;			///< Shared imported node transform value.
	using Bounds = vve::v5::Bounds;					///< Shared mesh object-space bounds value.
	using VertexCount = vve::v5::VertexCount;	///< Shared mesh vertex-count value.
	using IndexCount = vve::v5::IndexCount;		///< Shared mesh index-count value.

	using SceneHandle = vve::v5::SceneHandle;					///< Shared imported scene identity.
	using NodeHandle = vve::v5::NodeHandle;						///< Shared imported scene-node identity.
	using MeshHandle = vve::v5::MeshHandle;						///< Shared imported mesh identity.
	using MaterialHandle = vve::v5::MaterialHandle;			///< Shared imported material identity.
	using TextureHandle = vve::v5::TextureHandle;			///< Shared imported texture identity.
	using LightHandle = vve::v5::LightHandle;					///< Shared imported light identity.
	using CameraHandle = vve::v5::CameraHandle;				///< Shared imported camera identity.
	using AssetSystem = vve::v5::AssetSystem;					///< Shared Assimp-backed asset manager surface.

} // namespace vve::simple
