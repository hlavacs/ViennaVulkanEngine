export module VEEngine.V4:RendererForward;
import std;
export import :RenderPass;

/// @file
/// @brief Concrete forward-renderer identity and default feature choices.

export namespace vve::v4 {

   /// @brief Tiny placeholder for the first concrete renderer implementation.
   class RendererForward {
   public:
      [[nodiscard]] static constexpr std::string_view id() noexcept;
      [[nodiscard]] static constexpr bool usesShadowMaps() noexcept;
      [[nodiscard]] static constexpr std::span<const RenderPassContract> passes() noexcept;
   };

} // namespace vve::v4

namespace vve::v4::detail {

   inline constexpr std::array forward_shadow_dependencies{RenderMilestone::frame_begin}; ///< Shadow pass roots.
   inline constexpr std::array forward_scene_dependencies{RenderMilestone::shadow_depth}; ///< Scene pass inputs.
   inline constexpr std::array forward_pass_contracts{                                    ///< Forward pass sequence.
       RenderPassContract{.id = RenderMilestone::shadow_depth,
                          .depends_on = forward_shadow_dependencies,
                          .shader_file = "ShadowDepth.slang",
                          .vertex_entry = "vveShadowDepthVertexMain",
                          .fragment_entry = "vveShadowDepthFragmentMain",
                          .inputs = "render meshes, light view-projection",
                          .outputs = "shadow depth texture",
                          .writes_debug_data = true},
       RenderPassContract{.id = RenderMilestone::scene_color,
                          .depends_on = forward_scene_dependencies,
                          .shader_file = "Forward.slang",
                          .vertex_entry = "vveForwardVertexMain",
                          .fragment_entry = "vveForwardFragmentMain",
                          .inputs = "render scene, camera, shadow depth texture",
                          .outputs = "scene color target, debug sample buffer",
                          .writes_debug_data = true}};

} // namespace vve::v4::detail

export namespace vve::v4 {

   /// @brief Returns the renderer selector id.
   constexpr std::string_view RendererForward::id() noexcept { return "forward"; }

   /// @brief Reports that this renderer uses shadow maps as its first shadow technique.
   constexpr bool RendererForward::usesShadowMaps() noexcept { return true; }

   /// @brief Returns the renderer-owned render pass contracts.
   constexpr std::span<const RenderPassContract> RendererForward::passes() noexcept {
      return detail::forward_pass_contracts;
   }

} // namespace vve::v4
