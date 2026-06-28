export module VEEngine.Simple.Window;
import std;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine window-information aliases backed by the v5 window module.
	*
	* Functional objects:
	* - WindowDesc and Windows name the reused startup window request surface.
	* - WindowInfo and WindowFrameData name the reused per-frame window state surface.
	* - WindowHandle, PixelExtent, RendererId, Entity, and Vector name the supporting data helpers.
	*
	* The simple engine reuses `VEEngine.V5:Window` directly for window information. SDL lifecycle,
	* input handling, renderer work, and window-system ownership remain outside this alias module.
	*/
export namespace vve::simple {

	using WindowHandle = vve::v5::WindowHandle; ///< Shared runtime window identity.
	using PixelExtent = vve::v5::PixelExtent;   ///< Shared pixel size value used by window state.
	using RendererId = vve::v5::RendererId;     ///< Shared renderer selection value stored on windows.
	using Entity = vve::v5::Entity;             ///< Shared ECS identity used for camera association.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared list container used by window snapshots.

	using WindowDesc = vve::v5::WindowDesc;           ///< Shared startup window request data.
	using Windows = vve::v5::Windows;                 ///< Shared startup window collection data.
	using WindowInfo = vve::v5::WindowInfo;           ///< Shared runtime window size, renderer, and camera state.
	using WindowFrameData = vve::v5::WindowFrameData; ///< Shared per-frame window snapshot data.

} // namespace vve::simple
