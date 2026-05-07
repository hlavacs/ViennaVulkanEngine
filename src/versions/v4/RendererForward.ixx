export module VEEngine.V4:RendererForward;
import std;

/// @file
/// @brief Concrete forward-renderer identity and default feature choices.

export namespace vve::v4 {

   /// @brief One planned renderer pass and the educational data needed to verify it.
   struct RendererPassContract {
      std::string_view id{};             ///< Pass selector id.
      std::string_view depends_on{};     ///< Single predecessor pass id; empty means root pass.
      std::string_view shader_file{};    ///< Slang source file used by the pass.
      std::string_view vertex_entry{};   ///< Vertex shader entry point.
      std::string_view fragment_entry{}; ///< Fragment shader entry point.
      std::string_view inputs{};         ///< Human-readable pass inputs.
      std::string_view outputs{};        ///< Human-readable pass outputs.
      bool writes_debug_data{};          ///< Whether this pass writes host-verifiable data.
   };

   /// @brief Tiny placeholder for the first concrete renderer implementation.
   class RendererForward {
   public:
      [[nodiscard]] static constexpr std::string_view id() noexcept { return "forward"; }

      [[nodiscard]] static constexpr bool usesShadowMaps() noexcept { return true; }

      [[nodiscard]] static constexpr std::span<const RendererPassContract> passes() noexcept {
         return pass_contracts;
      }

   private:
      inline static constexpr std::array pass_contracts{
          RendererPassContract{.id = "shadow_depth",
                               .depends_on = "",
                               .shader_file = "ShadowDepth.slang",
                               .vertex_entry = "vveShadowDepthVertexMain",
                               .fragment_entry = "vveShadowDepthFragmentMain",
                               .inputs = "render meshes, light view-projection",
                               .outputs = "shadow depth texture",
                               .writes_debug_data = true},
          RendererPassContract{.id = "forward_color",
                               .depends_on = "shadow_depth",
                               .shader_file = "Forward.slang",
                               .vertex_entry = "vveForwardVertexMain",
                               .fragment_entry = "vveForwardFragmentMain",
                               .inputs = "render scene, camera, shadow depth texture",
                               .outputs = "color target, debug sample buffer",
                               .writes_debug_data = true}};

   };

} // namespace vve::v4
