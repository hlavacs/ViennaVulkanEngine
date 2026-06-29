export module VEEngine.Simple:SceneGraph;
export import :Types;
export import :Graph;

export namespace vve::simple {
	using SceneTree = Tree<NodeHandle>; ///< Simple scene hierarchy over simple node handles.
}
