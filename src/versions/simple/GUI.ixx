export module VEEngine.Simple.GUI;
import std;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine GUI aliases backed by the v5 GUI module.
	*
	* Functional objects:
	* - GuiSystem names the reused v5 GUI registry surface.
	* - GuiWidgetHandle names the reused GUI widget identity.
	* - Error, RenderPassContract, and RenderMilestone name the supporting result and render-pass helpers.
	*
	* The simple engine reuses `VEEngine.V5:Gui` directly for GUI hooks. Widget storage,
	* ImGui draw-data presentation, render-pass wiring, and debug presentation remain implemented by v5.
	*/
export namespace vve::simple {

	using Error = vve::v5::Error;                             ///< Shared operation error type returned by GUI functions.
	using GuiWidgetHandle = vve::v5::GuiWidgetHandle;         ///< Shared GUI widget identity.
	using RenderPassContract = vve::v5::RenderPassContract;   ///< Shared render-pass contract returned by GUI systems.
	using RenderMilestone = vve::v5::RenderMilestone;         ///< Shared render milestone names used by GUI pass contracts.
	using GuiSystem = vve::v5::GuiSystem;                     ///< Shared GUI registry implementation type.

} // namespace vve::simple
