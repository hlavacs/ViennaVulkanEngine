export module VEEngine:Implementation;
import std;
import VEEngine.Simple;

/// @file
/// @brief Binds the facade to the simple engine.
///
/// This is the only facade file that names an engine implementation. CMake compiles exactly one file from
/// src/implementations/, chosen by VVE_ENGINE_IMPLEMENTATION_NAMESPACE; the wrappers only use the aliases below.
export namespace vve {

	inline constexpr std::string_view engineImplementationNamespaceName{"simple"};	///< Active implementation namespace name.

	namespace detail {
		using EngineImpl = simple::Engine;							///< Engine wrapped by vve::Engine.
		using AssetSystemImpl = simple::AssetSystem;				///< Asset system wrapped by vve::AssetSystem.
		using RenderSystemImpl = simple::RenderSystem;			///< Render system wrapped by vve::RenderSystem.
		using WindowSystemImpl = simple::WindowSystem;			///< Window system wrapped by vve::WindowSystem.
		using WindowImpl = simple::Window;							///< Window wrapped by vve::Window.
		using InputStateImpl = simple::InputState;				///< Input state wrapped by vve::InputState.
		using GuiSystemImpl = simple::GuiSystem;					///< GUI system wrapped by vve::GuiSystem.
		using WindowsImpl = simple::Windows;						///< Startup window list consumed by the engine.
		using WindowDescImpl = simple::WindowDesc;				///< Startup window descriptor consumed by the engine.
		using WindowFrameDataImpl = simple::WindowFrameData;	///< Per-frame window snapshot produced by the engine.
	} // namespace detail

} // namespace vve
