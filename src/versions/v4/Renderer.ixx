export module VEEngine.V4:Renderer;
import std;
export import :Shaders;

/// @file
/// @brief Renderer-selection stubs; actual Vulkan renderers are intentionally absent.

export namespace vve::v4 {

   /// @brief Renderer choice descriptor returned by the factory.
   struct RendererDescriptor {
      using HandleType = RendererHandle; ///< Descriptor handle type.
      RendererHandle handle{};      ///< Stable renderer descriptor handle.
      RendererId id{.value = "forward"}; ///< Renderer id chosen by the application.
      bool shadow_maps{true};       ///< Whether this renderer intends to use shadow maps.
   };

   /// @brief Minimal factory; later it will choose concrete renderer implementations by id.
   class RendererFactory {
   public:
      /// @brief Creates the default forward-renderer descriptor.
      [[nodiscard]] RendererDescriptor createForwardRenderer() const {
         return RendererDescriptor{.handle = makeCounterHandle<RendererHandle>(),
                                   .id = RendererId{.value = "forward"},
                                   .shadow_maps = true};
      }

   };

} // namespace vve::v4
