export module VEEngine.V4:RenderPass;
import std;
export import :Types;
export import :Graph;

/// @file
/// @brief Render-pass contracts, milestones, and a small dependency graph.

export namespace vve::v4 {

   using RenderPassHandle = TypedHandle<decltype([] {})>; ///< v4 render-pass graph node handle.

   /// @brief Iterable milestone container used as dependency anchors between systems.
   struct RenderMilestone {
      inline static constexpr std::string_view frame_begin{"frame_begin"};             ///< Root of each frame graph.
      inline static constexpr std::string_view depth_prepass{"depth_prepass"};         ///< Camera depth is available.
      inline static constexpr std::string_view shadow_depth{"shadow_depth"};           ///< Shadow-map input is ready.
      inline static constexpr std::string_view raytraced_shadow{"raytraced_shadow"};   ///< Ray-traced shadow input.
      inline static constexpr std::string_view gbuffer{"gbuffer"};                     ///< Deferred G-buffer is ready.
      inline static constexpr std::string_view deferred_lighting{"deferred_lighting"}; ///< Deferred lighting is ready.
      inline static constexpr std::string_view raytraced_scene{"raytraced_scene"};     ///< Ray-traced scene color.
      inline static constexpr std::string_view scene_color{"scene_color"};             ///< Main scene color is ready.
      inline static constexpr std::string_view gui{"gui"};                             ///< GUI overlay has been drawn.
      inline static constexpr std::string_view frame_finished{"frame_finished"};       ///< Frame is ready to present.

      inline static constexpr std::array values{frame_begin, depth_prepass, shadow_depth, raytraced_shadow, gbuffer,
                                                deferred_lighting, raytraced_scene, scene_color, gui, frame_finished};

      [[nodiscard]] constexpr auto begin() const noexcept { return values.begin(); } ///< First milestone iterator.
      [[nodiscard]] constexpr auto end() const noexcept { return values.end(); }     ///< Past-end milestone iterator.
      [[nodiscard]] constexpr std::size_t size() const noexcept { return values.size(); } ///< Milestone count.
      [[nodiscard]] static constexpr std::span<const std::string_view> all() noexcept { return values; } ///< All names.
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
      bool writes_debug_data{};                       ///< Whether this pass writes host-verifiable data.
   };

   using RenderGraph = Graph<RenderPassHandle>; ///< Generic DAG for renderer pass dependencies.

} // namespace vve::v4
