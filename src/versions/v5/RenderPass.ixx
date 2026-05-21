export module VEEngine.V5:RenderPass;
import std;
export import :Types;
export import :Graph;

/// @file
/// @brief Render-pass contracts, milestones, and a small dependency graph.

export namespace vve::v5 {

   struct RenderPassHandleTag {}; ///< v5 render-pass graph node handle tag.

   using RenderPassHandle = TypedHandle<RenderPassHandleTag>; ///< v5 render-pass graph node handle.

   /// @brief Iterable milestone container used as dependency anchors between systems.
   struct RenderMilestone {
      [[nodiscard]] static constexpr std::string_view frame_begin() noexcept { return "frame_begin"; }
      [[nodiscard]] static constexpr std::string_view depth_prepass() noexcept { return "depth_prepass"; }
      [[nodiscard]] static constexpr std::string_view shadow_depth() noexcept { return "shadow_depth"; }
      [[nodiscard]] static constexpr std::string_view raytraced_shadow() noexcept { return "raytraced_shadow"; }
      [[nodiscard]] static constexpr std::string_view gbuffer() noexcept { return "gbuffer"; }
      [[nodiscard]] static constexpr std::string_view deferred_lighting() noexcept { return "deferred_lighting"; }
      [[nodiscard]] static constexpr std::string_view raytraced_scene() noexcept { return "raytraced_scene"; }
      [[nodiscard]] static constexpr std::string_view scene_color() noexcept { return "scene_color"; }
      [[nodiscard]] static constexpr std::string_view gui() noexcept { return "gui"; }
      [[nodiscard]] static constexpr std::string_view frame_finished() noexcept { return "frame_finished"; }
      [[nodiscard]] static constexpr std::array<std::string_view, 10> all() noexcept {
         return {frame_begin(), depth_prepass(), shadow_depth(), raytraced_shadow(), gbuffer(), deferred_lighting(),
                 raytraced_scene(), scene_color(), gui(), frame_finished()};
      }

      std::array<std::string_view, 10> values{all()}; ///< Ordered milestone names.

      [[nodiscard]] constexpr auto begin() const noexcept { return values.begin(); } ///< First milestone iterator.
      [[nodiscard]] constexpr auto end() const noexcept { return values.end(); }     ///< Past-end milestone iterator.
      [[nodiscard]] constexpr std::size_t size() const noexcept { return values.size(); } ///< Milestone count.
   };

   /// @brief One planned render node and the educational data needed to verify it.
   struct RenderPassContract {
      std::string_view name{};                        ///< Pass or milestone name.
      std::span<const std::string_view> depends_on{}; ///< Node names that must complete first.
      std::string_view shader_file{};                 ///< Slang source file used by the pass.
      std::string_view vertex_entry{};                ///< Vertex shader entry point.
      std::string_view fragment_entry{};              ///< Fragment shader entry point.
      std::string_view inputs{};                      ///< Human-readable pass inputs.
      std::string_view outputs{};                     ///< Human-readable pass outputs.
      bool milestone{};                               ///< True for meta nodes that anchor real passes.
   };

   using RenderGraph = Graph<RenderPassHandle>; ///< Generic DAG for renderer pass dependencies.

} // namespace vve::v5
