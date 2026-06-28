export module VEEngine.Simple.LightShadowDebug;
export import VEEngine.V5:RenderSystem;

/**
	* @file
	* @brief Simple-engine light and shadow debug aliases backed by the v5 render-system module.
	*
	* Functional objects:
	* - RenderDirectionalLight, RenderPointLight, and RenderSpotLight name the reused light debug data.
	* - RenderDebugSample and RenderShadowDepthSample name the reused CPU-visible verification samples.
	*
	* The simple engine reuses `VEEngine.V5:RenderSystem` directly for light and shadow debug data.
	* Slang, SDL3, rendering, PNG output, and sample generation remain implemented outside this module.
	*/
export namespace vve::simple {

	using RenderDirectionalLight = vve::v5::RenderDirectionalLight;       ///< Shared directional-light debug data.
	using RenderPointLight = vve::v5::RenderPointLight;                   ///< Shared point-light debug data.
	using RenderSpotLight = vve::v5::RenderSpotLight;                     ///< Shared spot-light debug data.
	using RenderDebugSample = vve::v5::RenderDebugSample;                 ///< Shared per-vertex light/shadow debug sample.
	using RenderShadowDepthSample = vve::v5::RenderShadowDepthSample;     ///< Shared shadow-depth verification sample.

} // namespace vve::simple
