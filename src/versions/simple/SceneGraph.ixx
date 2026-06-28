export module VEEngine.Simple.SceneGraph;
import std;
export import VEEngine.V5:Assets;
export import VEEngine.V5:Graph;

/**
	* @file
	* @brief Simple-engine scene-graph aliases backed by the v5 asset scene hierarchy.
	*
	* Functional objects:
	* - SceneHandle and NodeHandle name the reused scene and scene-node identities.
	* - Transform names the reused local transform value stored for imported nodes.
	* - SceneTree names the reused v5 hierarchy over scene-node handles.
	* - Error, ObjectName, and Vector name the shared result, label, and list helpers.
	*
	* The simple engine reuses `VEEngine.V5:Assets` directly for the scene hierarchy surface.
	* Scene loading, transform propagation, light and shadow placement, renderer descriptors, and
	* internal catalogues remain outside this alias module.
	*/
export namespace vve::simple {

	using Error = vve::v5::Error;					///< Shared operation error type returned by scene-graph functions.
	using ObjectName = vve::v5::ObjectName;	///< Shared scene and node label type.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared scene-list result container.

	using SceneHandle = vve::v5::SceneHandle; ///< Shared scene identity used by the asset scene hierarchy.
	using NodeHandle = vve::v5::NodeHandle;	 ///< Shared scene-node identity used by the hierarchy tree.
	using Transform = vve::v5::Transform;		 ///< Shared local node transform value.
	using SceneTree = vve::v5::Tree<NodeHandle>; ///< Shared v5 scene hierarchy over node handles.

} // namespace vve::simple
