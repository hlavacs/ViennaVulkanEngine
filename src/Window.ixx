export module VEEngine:Window;
import std;
import VEEngine.V4;
import :Types;

/**
 * @file
 * @brief Public window/input contract backed by the selected engine implementation.
 */
export namespace vve {

   using InputState      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::InputState;      ///< Facade input snapshot.
   using WindowDesc      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowDesc;      ///< Facade window descriptor.
   using WindowFrameData = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowFrameData; ///< Per-frame window snapshot.
   using WindowInfo      = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::WindowInfo;      ///< Runtime window state.
   using Windows         = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Windows;         ///< Startup window collection.

   template <typename T> concept InputStateLike =
      requires(T input, WindowHandle window, Vec2 position, Vec2 delta, std::int32_t keycode) {
         input.beginFrame();
         input.holdKey(keycode);
         input.pressKey(keycode);
         input.releaseKey(keycode);
         input.setMousePosition(window, position);
         input.addMouseDelta(window, delta);
         input.addMouseWheelDelta(window, delta);
         { input.isKeyDown(keycode) } -> std::same_as<bool>;
         { input.wasKeyPressed(keycode) } -> std::same_as<bool>;
         { input.wasKeyReleased(keycode) } -> std::same_as<bool>;
         { input.mousePosition(window) } -> std::same_as<std::optional<Vec2>>;
         { input.mouseDelta(window) } -> std::same_as<Vec2>;
         { input.mouseWheelDelta(window) } -> std::same_as<Vec2>;
      }; ///< Contract for the public input snapshot class.

   template <typename T> concept WindowDescLike = requires(T value) {
      { value.id } -> std::same_as<std::string &>;
      { value.title } -> std::same_as<std::string &>;
      { value.extent } -> std::same_as<PixelExtent &>;
      { value.x } -> std::same_as<std::optional<int> &>;
      { value.y } -> std::same_as<std::optional<int> &>;
      { value.renderer_id } -> std::same_as<RendererId &>;
      { value.resizable } -> std::same_as<bool &>;
      { value.visible } -> std::same_as<bool &>;
   }; ///< Contract for window creation descriptors.

   template <typename T> concept WindowInfoLike = requires(T value) {
      { value.handle } -> std::same_as<WindowHandle &>;
      { value.id } -> std::same_as<std::string &>;
      { value.title } -> std::same_as<std::string &>;
      { value.extent } -> std::same_as<PixelExtent &>;
      { value.renderer_id } -> std::same_as<RendererId &>;
      { value.camera } -> std::same_as<std::optional<Entity> &>;
      { value.focused } -> std::same_as<bool &>;
      { value.minimized } -> std::same_as<bool &>;
      { value.should_close } -> std::same_as<bool &>;
   }; ///< Contract for runtime window records.

   template <typename T> concept WindowFrameDataLike = requires(T value) {
      { value.windows } -> std::same_as<Vector<WindowInfo> &>;
   }; ///< Contract for per-frame window snapshots.

   template <typename T> concept WindowsLike = requires(T value) {
      { value.value } -> std::same_as<Vector<WindowDesc> &>;
   }; ///< Contract for startup window collections.

} // namespace vve
