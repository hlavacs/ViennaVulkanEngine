export module VEEngine.V4:RenderPass;
import std;

/// @file
/// @brief Shared render-pass contracts and milestone ids used by all v4 systems.

export namespace vve::v4 {

   /// @brief Stable milestone ids used as dependency anchors between systems.
   namespace RenderMilestone {
      inline constexpr std::string_view frame_begin{"frame_begin"};             ///< Root of each frame graph.
      inline constexpr std::string_view depth_prepass{"depth_prepass"};         ///< Camera depth is available.
      inline constexpr std::string_view shadow_depth{"shadow_depth"};           ///< Shadow-map shadow input is ready.
      inline constexpr std::string_view raytraced_shadow{"raytraced_shadow"};   ///< Ray-traced shadow input is ready.
      inline constexpr std::string_view gbuffer{"gbuffer"};                     ///< Deferred G-buffer is available.
      inline constexpr std::string_view deferred_lighting{"deferred_lighting"}; ///< Deferred lighting is available.
      inline constexpr std::string_view raytraced_scene{"raytraced_scene"};     ///< RT scene color is available.
      inline constexpr std::string_view scene_color{"scene_color"};             ///< Main scene color is ready.
      inline constexpr std::string_view gui{"gui"};                             ///< GUI overlay has been drawn.
      inline constexpr std::string_view present{"present"};                     ///< Back buffer is ready to present.
   } // namespace RenderMilestone

   /// @brief One planned render pass and the educational data needed to verify it.
   struct RenderPassContract {
      std::string_view id{};                         ///< Pass selector id.
      std::span<const std::string_view> depends_on{}; ///< Pass ids that must complete first.
      std::string_view shader_file{};                ///< Slang source file used by the pass.
      std::string_view vertex_entry{};               ///< Vertex shader entry point.
      std::string_view fragment_entry{};             ///< Fragment shader entry point.
      std::string_view inputs{};                     ///< Human-readable pass inputs.
      std::string_view outputs{};                    ///< Human-readable pass outputs.
      bool writes_debug_data{};                      ///< Whether this pass writes host-verifiable data.
   };

   /// @brief One flat pass list supplied by a renderer or another engine system.
   struct RenderPassList {
      std::span<const RenderPassContract> passes{}; ///< Passes supplied by one producer.
   };

} // namespace vve::v4
