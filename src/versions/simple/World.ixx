export module VEEngine.Simple.World;
import std;
export import VEEngine;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine world aliases backed by the public World facade and v5 subsystems.
	*
	* Functional objects:
	* - World names the reused facade view that forwards access to referenced subsystems.
	* - ECS, AssetSystem, GuiSystem, Engine, WindowSystem, and RenderSystem name the forwarded v5 systems.
	* - Error, handle, frame, and Vector names provide the supporting types exposed by those systems.
	*
	* The simple engine adds no world storage or forwarding logic here. The public World facade keeps
	* references only, while concrete subsystem behavior remains implemented by `VEEngine.V5`.
	*/
export namespace vve::simple {

	template <typename... TObjects>
	using World = vve::World<TObjects...>; ///< Shared facade world view over referenced subsystem wrappers.

	using Error = vve::v5::Error;                         ///< Shared operation error type returned by subsystems.
	using Entity = vve::v5::Entity;                       ///< Shared ECS entity identity available through World.
	using SceneHandle = vve::v5::SceneHandle;             ///< Shared scene identity used by asset access.
	using NodeHandle = vve::v5::NodeHandle;               ///< Shared scene-node identity used by asset access.
	using MeshHandle = vve::v5::MeshHandle;               ///< Shared mesh identity used by asset access.
	using MaterialHandle = vve::v5::MaterialHandle;       ///< Shared material identity used by asset access.
	using TextureHandle = vve::v5::TextureHandle;         ///< Shared texture identity used by asset access.
	using LightHandle = vve::v5::LightHandle;             ///< Shared light identity used by asset access.
	using CameraHandle = vve::v5::CameraHandle;           ///< Shared imported-camera identity used by asset access.
	using WindowHandle = vve::v5::WindowHandle;           ///< Shared runtime window identity.
	using GuiWidgetHandle = vve::v5::GuiWidgetHandle;     ///< Shared GUI widget identity.
	using ApplicationName = vve::v5::ApplicationName;     ///< Shared engine application-name option.
	using EngineConfig = vve::v5::EngineConfig;           ///< Shared compact engine configuration.
	using FrameContext = vve::v5::FrameContext;           ///< Shared per-frame timing context.
	using FrameStatus = vve::v5::FrameStatus;             ///< Shared engine step status.
	using MaxFrames = vve::v5::MaxFrames;                 ///< Shared frame-cap option.

	template <typename T, std::size_t SegmentSize = 256>
	using Vector = vve::v5::Vector<T, SegmentSize>; ///< Shared list container returned by subsystem queries.

	template <typename T>
	using VectorConstRange = vve::v5::VectorConstRange<T>; ///< Shared read-only vector range helper.

	using ECS = vve::v5::ECS;                   ///< Shared entity/component storage reachable from World.
	using AssetSystem = vve::v5::AssetSystem;   ///< Shared asset manager reachable from World.
	using GuiSystem = vve::v5::GuiSystem;       ///< Shared GUI hook registry reachable from World.
	using Engine = vve::v5::Engine;             ///< Shared concrete engine type owning forwarded subsystems.
	using WindowSystem = vve::v5::WindowSystem; ///< Shared window manager reachable from World.
	using RenderSystem = vve::v5::RenderSystem; ///< Shared renderer facade currently included in World views.

} // namespace vve::simple
