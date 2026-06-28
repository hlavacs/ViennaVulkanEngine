export module VEEngine.Simple.Engine;
import std;
export import VEEngine;
export import VEEngine.V5;

/**
	* @file
	* @brief Simple-engine engine aliases backed by the public Engine facade and v5 owner.
	*
	* Functional objects:
	* - Engine names the reused v5 concrete owner for startup, frame stepping, and shutdown.
	* - FacadeEngine names the public facade engine whose world() member returns a World view.
	* - MakeEngine and makeEngine name the public factory entry for facade engine construction.
	* - Option and status names provide the startup and frame contracts accepted by the engine.
	*
	* The simple engine adds no owner storage, frame loop, subsystem construction, or Vulkan logic here.
	* Those responsibilities remain in `VEEngine` and `VEEngine.V5`.
	*/
export namespace vve::simple {

	inline constexpr std::string_view engineImplementationNamespaceName{"simple"}; ///< Simple-engine facade name.

	using Engine = vve::v5::Engine;                    ///< Shared concrete v5 engine owner.
	using MakeEngine = vve::MakeEngine;                ///< Shared facade engine factory type.
	using Error = vve::v5::Error;                      ///< Shared operation error type returned by engine calls.
	using ApplicationName = vve::v5::ApplicationName;  ///< Shared engine application-name option.
	using EngineConfig = vve::v5::EngineConfig;        ///< Shared compact engine configuration.
	using FrameContext = vve::v5::FrameContext;        ///< Shared per-frame timing context.
	using FrameStatus = vve::v5::FrameStatus;          ///< Shared engine step status.
	using MaxFrames = vve::v5::MaxFrames;              ///< Shared frame-cap option.
	using FrameCount = vve::v5::FrameCount;            ///< Shared frame-count value used by frame options.
	using DeltaTime = vve::v5::DeltaTime;              ///< Shared frame delta value used by frame context.
	using UserSystemTasks = vve::v5::UserSystemTasks;  ///< Shared v5 task-name option derived from user systems.

	template <typename... TSystems>
	using FacadeEngine = vve::Engine<TSystems...>; ///< Shared public engine facade with world() access.

	template <typename... TSystems>
	using UserSystems = vve::UserSystems<TSystems...>; ///< Shared user-system bundle consumed by makeEngine.

	using vve::makeEngine;      ///< Shared public factory for facade engines.
	using vve::makeUserSystems; ///< Shared public factory for user-system bundles.

} // namespace vve::simple
