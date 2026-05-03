export module VEEngine.V4:World;
export import :Types;

/// @file
/// @brief v4 compatibility aliases for facade-owned world, input, and window data.

export namespace vve::v4 {

   using ::vve::ApplicationName; ///< Facade application-name option.
   using ::vve::FrameContext;    ///< Facade per-frame timing context.
   using ::vve::InputState;      ///< Facade input snapshot.
   using ::vve::MaxFrames;       ///< Facade max-frame option.
   using ::vve::WindowDesc;      ///< Facade window creation descriptor.
   using ::vve::WindowFrameData; ///< Facade per-frame window snapshot.
   using ::vve::WindowInfo;      ///< Facade runtime window state.
   using ::vve::WindowHandle;    ///< Facade runtime window handle.
   using ::vve::Windows;         ///< Facade startup-window collection.
   using ::vve::World;           ///< Facade world object.

} // namespace vve::v4
