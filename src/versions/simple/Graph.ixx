export module VEEngine.Simple.Graph;
import std;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine graph aliases backed by the v5 generic graph helper.
	*
	* Functional objects:
	* - Graph aliases the reused v5 named DAG helper for simple-engine hierarchy support.
	* - Tree aliases the reused v5 single-root tree helper used as the scene-graph basis.
	* - Error, ObjectName, and Vector name the result, label, and traversal value surface.
	*
	* The simple engine reuses `VEEngine.V5:Graph` directly. Graph storage, edges, traversal,
	* and JSON helpers remain owned by v5; render graphs, task graphs, and scene logic belong
	* to their separate simple-engine subsystems and are not implemented here.
	*/
export namespace vve::simple {

	using Error = vve::v5::Error;           ///< Shared operation error type returned by graph functions.
	using ObjectName = vve::v5::ObjectName; ///< Shared node label type used by the graph helper.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared traversal result container used by graph views.

	template <typename THandle>
	using Graph = vve::v5::Graph<THandle>; ///< Shared named DAG helper; no simple-engine storage is added here.

	template <typename THandle>
	using Tree = vve::v5::Tree<THandle>; ///< Shared single-root tree helper for later scene-graph composition.

} // namespace vve::simple
