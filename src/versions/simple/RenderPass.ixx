export module VEEngine.Simple:RenderPass;
import std;
export import VEEngine.Simple.RenderPassContract;
export import :Types;
export import :Graph;

/// @file
/// @brief Render-pass contracts, milestones, and a small dependency graph.

export namespace vve::simple {

	struct RenderPassHandleTag {};										///< simple render-pass graph node handle tag.

	using RenderPassHandle = TypedHandle<RenderPassHandleTag>;	///< simple render-pass graph node handle.

	using RenderGraph = Graph<RenderPassHandle>;						///< Generic DAG for renderer pass dependencies.

} // namespace vve::simple
