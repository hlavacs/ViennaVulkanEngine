export module VEEngine.V5:RendererForward;
import std;
export import :RenderPass;

/// @file
/// @brief Concrete forward-renderer identity and default feature choices.

export namespace vve::v5 {

   /// @brief Tiny placeholder for the first concrete renderer implementation.
   class RendererForward {
   public:
      [[nodiscard]] static constexpr std::string_view id() noexcept;
      [[nodiscard]] static constexpr bool usesShadowMaps() noexcept;
      [[nodiscard]] static constexpr std::span<const RenderPassContract> passes() noexcept;
   };

} // namespace vve::v5

namespace vve::v5::detail {

   inline constexpr std::string_view forward_shadow_map_pass{"forward.shadow_map_pass"}; ///< Real shadow-map pass.
   inline constexpr std::string_view forward_color_pass{"forward.color_pass"};           ///< Real color pass.

   inline constexpr std::array forward_shadow_pass_deps{RenderMilestone::frame_begin()};   ///< Shadow pass root.
   inline constexpr std::array forward_shadow_done_deps{forward_shadow_map_pass};        ///< Shadow milestone input.
   inline constexpr std::array forward_color_pass_deps{RenderMilestone::shadow_depth()};   ///< Color pass input.
   inline constexpr std::array forward_color_done_deps{forward_color_pass};              ///< Color milestone input.
   inline constexpr std::array forward_finished_deps{RenderMilestone::scene_color()};      ///< Presentation input.

   inline constexpr std::array forward_pass_contracts{                                   ///< Forward graph wiring.
       RenderPassContract{.name = RenderMilestone::frame_begin(),
                          .outputs = "root of the frame render graph",
                          .milestone = true},
       RenderPassContract{.name = forward_shadow_map_pass,
                          .depends_on = forward_shadow_pass_deps,
                          .shader_file = "ShadowDepth.slang",
                          .vertex_entry = "vveShadowDepthVertexMain",
                          .fragment_entry = "vveShadowDepthFragmentMain",
                          .inputs = "render meshes, light view-projection",
                          .outputs = "shadow depth texture",
                          .writes_debug_data = true},
       RenderPassContract{.name = RenderMilestone::shadow_depth(),
                          .depends_on = forward_shadow_done_deps,
                          .outputs = "shadow depth texture is ready",
                          .milestone = true},
       RenderPassContract{.name = forward_color_pass,
                          .depends_on = forward_color_pass_deps,
                          .shader_file = "Forward.slang",
                          .vertex_entry = "vveForwardVertexMain",
                          .fragment_entry = "vveForwardFragmentMain",
                          .inputs = "render scene, camera, shadow depth texture",
                          .outputs = "scene color target, debug sample buffer",
                          .writes_debug_data = true},
       RenderPassContract{.name = RenderMilestone::scene_color(),
                          .depends_on = forward_color_done_deps,
                          .outputs = "scene color target is ready",
                          .milestone = true},
       RenderPassContract{.name = RenderMilestone::frame_finished(),
                          .depends_on = forward_finished_deps,
                          .outputs = "frame can be presented",
                          .milestone = true}};

} // namespace vve::v5::detail

export namespace vve::v5 {

   /// @brief Returns the renderer selector id.
   constexpr std::string_view RendererForward::id() noexcept { return "forward"; }

   /// @brief Reports that this renderer uses shadow maps as its first shadow technique.
   constexpr bool RendererForward::usesShadowMaps() noexcept { return true; }

   /// @brief Returns the renderer-owned render pass contracts.
   constexpr std::span<const RenderPassContract> RendererForward::passes() noexcept {
      return detail::forward_pass_contracts;
   }

} // namespace vve::v5
