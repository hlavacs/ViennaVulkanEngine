export module VVEngine:Implementation;
import std;
import VVEngine.Complex;

/// @file
/// @brief Binds the facade to the complex engine.
///
/// This is the only facade file that names an engine implementation. CMake compiles exactly one file from
/// src/implementations/, chosen by VVE_ENGINE_IMPLEMENTATION_NAMESPACE; the wrappers only use the aliases below.
export namespace vve {

	inline constexpr std::string_view engineImplementationNamespaceName{"complex"};	///< Active implementation namespace name.

	namespace detail {
		using AudioSystemImpl = complex::AudioSystem; ///< Audio system wrapped by vve::AudioSystem.
		using EngineImpl = complex::Engine;							///< Engine wrapped by vve::Engine.
		using AssetSystemImpl = complex::AssetSystem;				///< Asset system wrapped by vve::AssetSystem.
		using RenderSystemImpl = complex::RenderSystem;			///< Render system wrapped by vve::RenderSystem.
		using WindowSystemImpl = complex::WindowSystem;			///< Window system wrapped by vve::WindowSystem.
		using WindowImpl = complex::Window;							///< Window wrapped by vve::Window.
		using InputStateImpl = complex::InputState;				///< Input state wrapped by vve::InputState.
		using GuiSystemImpl = complex::GuiSystem;					///< GUI system wrapped by vve::GuiSystem.
		using WindowsImpl = complex::Windows;						///< Startup window list consumed by the engine.
	} // namespace detail

} // namespace vve
