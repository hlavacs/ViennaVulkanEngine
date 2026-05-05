module;

#ifndef VVE_ENGINE_IMPLEMENTATION_NAMESPACE
#define VVE_ENGINE_IMPLEMENTATION_NAMESPACE v4
#endif

#define VVE_DETAIL_STRINGIFY_IMPL(value) #value
#define VVE_DETAIL_STRINGIFY(value) VVE_DETAIL_STRINGIFY_IMPL(value)

export module VEEngine;
import std;
import VEEngine.V4;
export import :Error;
export import :Math;
export import :Handle;
export import :Types;
export import :Graph;
export import :ECS;
export import :Window;
export import :World;

/// @file
/// @brief Public engine facade; users import this module and use only namespace vve.

export namespace vve {

   inline constexpr std::string_view engineImplementationNamespaceName{
      VVE_DETAIL_STRINGIFY(VVE_ENGINE_IMPLEMENTATION_NAMESPACE)}; ///< Active implementation namespace name.

   using AssetSystem     = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::AssetSystem;     ///< Public asset importer.
   using GuiSystem       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiSystem;       ///< Public GUI descriptor system.
   using GuiWidget       = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiWidget;       ///< Public GUI widget data.
   using GuiWidgetHandle = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::GuiWidgetHandle; ///< GUI widget handle.

   template <typename... TSystems>
   using Engine = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Engine<TSystems...>; ///< Facade engine template.

   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeEngine; ///< Facade engine factory.

   template <typename T> concept GuiWidgetHandleLike =
      TypedHandleLike<T> && std::same_as<std::remove_cvref_t<T>, GuiWidgetHandle>; ///< GUI widget handle contract.

   template <typename T> concept GuiWidgetLike = requires(T widget) {
      typename T::HandleType;
      { widget.handle } -> std::same_as<GuiWidgetHandle &>;
      { widget.label } -> std::same_as<std::string &>;
   }; ///< Contract for GUI widget descriptors.

   template <typename T> concept GuiSystemLike = requires(T gui, GuiWidgetHandle handle, std::string label) {
      { gui.label(label) } -> std::same_as<std::expected<GuiWidgetHandle, Error>>;
      { gui.find(handle) } -> std::same_as<const GuiWidget *>;
      { gui.size() } -> std::convertible_to<std::size_t>;
   }; ///< Contract for the public GUI system.

   template <typename T> concept AssetSystemLike =
      requires(T assets, ObjectName name, std::filesystem::path path) {
         { assets.catalog() } -> std::same_as<ObjectCatalog &>;
         { assets.addScene(name) } -> std::same_as<std::expected<SceneHandle, Error>>;
         { assets.loadScene(path) } -> std::same_as<std::expected<SceneHandle, Error>>;
      }; ///< Contract for the public asset system.

   template <typename TEngine> concept EngineLike = requires(TEngine engine) {
      { engine.versionMajor() } -> std::convertible_to<std::uint32_t>;
      { engine.getVersionMajor() } -> std::same_as<std::expected<int, Error>>;
      { engine.versionName() } -> std::convertible_to<std::string_view>;
      { engine.world() } -> std::same_as<World &>;
      { engine.assets() } -> std::same_as<AssetSystem &>;
      { engine.gui() } -> std::same_as<GuiSystem &>;
      { engine.ecs() } -> std::same_as<ECS &>;
      { engine.init() } -> std::same_as<std::expected<void, Error>>;
      { engine.run() } -> std::same_as<std::expected<void, Error>>;
      { engine.step() } -> std::same_as<std::expected<FrameStatus, Error>>;
   }; ///< Contract for the public engine facade.

   template <typename... TOptions> concept MakeEngineFunctionLike = requires(TOptions... options) {
      { makeEngine(options...) };
   }; ///< Contract for makeEngine(...).

   static_assert(AssetSystemLike<AssetSystem>);
   static_assert(EngineLike<Engine<>>);
   static_assert(GuiSystemLike<GuiSystem>);
   static_assert(GuiWidgetHandleLike<GuiWidgetHandle>);
   static_assert(GuiWidgetLike<GuiWidget>);
   static_assert(MakeEngineFunctionLike<>);
   static_assert(MakeEngineFunctionLike<ApplicationName, MaxFrames>);

} // namespace vve
