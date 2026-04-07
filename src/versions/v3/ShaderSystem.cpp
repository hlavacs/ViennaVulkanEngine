module;

#include "FacadeMacros.hpp"

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 shader-reflection implementation.
 *
 * The current implementation produces placeholder reflection data while
 * preserving the shape of the shader metadata API.
 */
namespace vve::v3 {

   /// @brief Converts a public renderer enum into the shader metadata string form.
   [[nodiscard]] std::string_view toRendererName(vve::RendererKind renderer) {
      switch (renderer) {
      case vve::RendererKind::forward_renderer:
         return "forward";
      case vve::RendererKind::deferred_renderer:
         return "deferred";
      case vve::RendererKind::path_tracing:
         return "path_tracing";
      }

      return "unknown";
   }

   /// @brief Converts a public shadow enum into the shader metadata string form.
   [[nodiscard]] std::string_view toShadowName(vve::ShadowKind shadow) {
      switch (shadow) {
      case vve::ShadowKind::none:
         return "none";
      case vve::ShadowKind::shadow_map:
         return "shadow_map";
      case vve::ShadowKind::ray_traced:
         return "ray_traced";
      }

      return "unknown";
   }

   /**
    * @brief Concrete shader-system implementation used by v3.
    */
   class SlangShaderSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "SlangShaderSystem"; }

      /// @brief Reflects a shader path into placeholder shader metadata.
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      reflect(const std::filesystem::path &shader_path, vve::RendererKind renderer, vve::ShadowKind shadow) {
         // The reflection seam currently fabricates metadata from the shader
         ShaderMetadata metadata{}; // path and the requested renderer configuration.
         metadata.handle = ShaderHandle{detail::makeStableHandle(shader_path.string())};
         metadata.shader_name = shader_path.filename().string();
         metadata.stages = {ShaderStage::vertex, ShaderStage::fragment};
         metadata.parameters = {
             ShaderParameter{.name = "FrameConstants", .type_name = "cbuffer", .binding = 0, .set = 0},
             ShaderParameter{.name = "MaterialParams", .type_name = "parameter_block", .binding = 1, .set = 0}};
         metadata.intended_renderer = std::string(toRendererName(renderer));
         metadata.intended_shadow = std::string(toShadowName(shadow));
         return metadata;
      }
   };

   /// @brief Constructs the public shader-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(ShaderSystemFacade, SlangShaderSystemImplementation, (), ())

   /// @brief Returns the shader-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ShaderSystemFacade, SlangShaderSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Reflects a shader through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ShaderSystemFacade, SlangShaderSystemImplementation, reflect,
                               (const std::filesystem::path &shader_path, vve::RendererKind renderer,
                                vve::ShadowKind shadow),
                               (shader_path, renderer, shadow), , std::expected<ShaderMetadata, vve::Error>)

   /// @brief Emits the explicit shader-system facade instantiation for v3.
   template class ShaderSystemFacade<SlangShaderSystemImplementation>;

} // namespace vve::v3
