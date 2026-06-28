export module VEEngine.Simple.WindowSystem;
import std;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine window-system aliases backed by the v5 window module.
	*
	* Functional objects:
	* - WindowSystem names the reused v5 SDL-backed window manager surface.
	* - InputState names the reused keyboard and mouse state owned by the manager.
	* - Window, WindowDesc, Windows, WindowInfo, and WindowFrameData name the reused window data surface.
	* - Error, WindowHandle, PixelExtent, RendererId, Entity, and Vector name the supporting data helpers.
	*
	* The simple engine reuses `VEEngine.V5:Window` directly for window management. SDL lifecycle,
	* window ownership, input polling, rendering, and camera selection remain implemented by v5.
	*/
export namespace vve::simple {

	using Error = vve::v5::Error;               ///< Shared operation error type returned by window-system functions.
	using WindowHandle = vve::v5::WindowHandle; ///< Shared runtime window identity.
	using PixelExtent = vve::v5::PixelExtent;   ///< Shared pixel size value used by window state.
	using RendererId = vve::v5::RendererId;     ///< Shared renderer selection value stored on windows.
	using Entity = vve::v5::Entity;             ///< Shared ECS identity used for camera association.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared list container used by window-system snapshots.

	using InputState = vve::v5::InputState;           ///< Shared keyboard and mouse state owned by the window system.
	using Window = vve::v5::Window;                   ///< Shared owned platform window implementation type.
	using WindowSystem = vve::v5::WindowSystem;       ///< Shared SDL-backed window manager implementation type.
	using WindowDesc = vve::v5::WindowDesc;           ///< Shared startup window request data.
	using Windows = vve::v5::Windows;                 ///< Shared startup window collection data.
	using WindowInfo = vve::v5::WindowInfo;           ///< Shared runtime window size, renderer, and camera state.
	using WindowFrameData = vve::v5::WindowFrameData; ///< Shared per-frame window snapshot data.

} // namespace vve::simple
