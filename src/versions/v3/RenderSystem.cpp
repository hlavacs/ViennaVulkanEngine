module;

#include "FacadeMacros.hpp"

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 render-system implementation.
 *
 * This file assembles the per-window render graph and provides the render
 * tasks that bridge scene data into backend-facing render work.
 */
namespace vve::v3 {

   /**
    * @brief Builds the optional shadow pass for the selected shadowing mode.
    * @param shadow Requested shadowing strategy.
    * @return Render-pass description when a shadow pass is required.
    */
   [[nodiscard]] std::optional<RenderPassDesc> buildShadowPass(vve::ShadowKind shadow) {
      switch (shadow) {
      case vve::ShadowKind::none:
         return std::nullopt;
      case vve::ShadowKind::shadow_map:
         return RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.shadow_map")},
                               .kernel = RenderKernelId::shadow_map,
                               .debug_name = "Shadow Map"};
      case vve::ShadowKind::ray_traced:
         return RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.ray_traced_shadows")},
                               .kernel = RenderKernelId::ray_traced_shadows,
                               .debug_name = "Ray Traced Shadows"};
      }

      return std::nullopt;
   }

   /**
    * @brief Concrete render-system implementation used by v3.
    *
    * The implementation owns renderer selection policy and translates that
    * policy into both a static render graph and the frame tasks that execute
    * the per-window render pipeline.
    */
   class DefaultRenderSystemImplementation {
   public:
      /**
       * @brief Creates the render system for the selected renderer configuration.
       * @param renderer Requested renderer family.
       * @param shadow Requested shadow mode.
       * @param graphics_backend Active graphics backend used for diagnostics and future backend work.
       * @param imgui_enabled Whether an ImGui render pass should be appended.
       */
      DefaultRenderSystemImplementation(vve::RendererKind renderer, vve::ShadowKind shadow,
                                        GraphicsBackend &graphics_backend, bool imgui_enabled)
          : shadow_(shadow), graphics_backend_(graphics_backend), imgui_enabled_(imgui_enabled) {
         (void)renderer;
      }

      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "RenderSystem"; }

      /**
       * @brief Builds the static render graph for one window.
       * @param window Window receiving the render pipeline.
       * @param renderer Backend renderer selected for the window.
       * @return Immutable render graph used by later scheduling and graph-dump code.
       */
      [[nodiscard]] RenderGraph buildStaticGraph(WindowHandle window, const RendererDesc &renderer) {
         RenderGraph graph{};
         const auto window_salt = window.value.value();

         // Shadow work, when enabled, becomes the first prerequisite of the
         // main pass so later passes can remain renderer-agnostic.
         if (const auto shadow_pass = buildShadowPass(shadow_)) {
            graph.passes.push_back(*shadow_pass);
         }

         const auto main_pass = RenderPassHandle{detail::makeStableHandle("render.main", window_salt)};
         Vector<RenderPassHandle> main_dependencies{};
         if (!graph.passes.empty()) {
            main_dependencies.push_back(graph.passes.front().handle);
         }
         // The main pass represents the renderer-specific primary shading stage.
         graph.passes.push_back(RenderPassDesc{.handle = main_pass,
                                               .kernel = renderer.main_kernel,
                                               .depends_on = std::move(main_dependencies),
                                               .debug_name = renderer.display_name});

         // Post-processing remains explicit in the graph so graph dumps and
         // later backend integration can reason about ordering cleanly.
         const auto post_process_pass = RenderPassHandle{detail::makeStableHandle("render.post_process", window_salt)};
         graph.passes.push_back(RenderPassDesc{.handle = post_process_pass,
                                               .kernel = RenderKernelId::post_process,
                                               .depends_on = {main_pass},
                                               .debug_name = "Post Processing"});

         const auto post_post_process_pass =
             RenderPassHandle{detail::makeStableHandle("render.post_post_process", window_salt)};
         graph.passes.push_back(RenderPassDesc{.handle = post_post_process_pass,
                                               .kernel = RenderKernelId::post_post_process,
                                               .depends_on = {post_process_pass},
                                               .debug_name = "Post Post Processing"});

         if (imgui_enabled_) {
            // GUI rendering is modeled as an optional trailing pass so it can
            // depend on the fully composed scene image.
            graph.passes.push_back(
                RenderPassDesc{.handle = RenderPassHandle{detail::makeStableHandle("render.imgui", window_salt)},
                               .kernel = RenderKernelId::imgui,
                               .depends_on = {post_post_process_pass},
                               .debug_name = std::string("ImGui (") + std::string(graphics_backend_.name()) + ")"});
         }

         return graph;
      }

      /// @brief Performs placeholder GPU visibility work for one window graph.
      [[nodiscard]] std::expected<void, vve::Error> cullVisibilityGpu(const FrameContext &, const SceneData &,
                                                                      WindowHandle, const RenderGraph &) {
         return {};
      }

      /// @brief Performs placeholder draw-packet generation for one window graph.
      [[nodiscard]] std::expected<void, vve::Error> buildDrawPackets(const FrameContext &, const SceneData &,
                                                                     WindowHandle, const RenderGraph &) {
         return {};
      }

      /// @brief Records placeholder render work for the supplied render graph.
      [[nodiscard]] std::expected<void, vve::Error> record(const FrameContext &, const SceneData &, WindowHandle,
                                                           const RenderGraph &render_graph) {
         for (const auto &pass : render_graph.passes) {
            // Iterating the passes keeps the placeholder implementation aligned
            // with the future shape where each pass will emit backend commands.
         }

         return {};
      }

      /// @brief Consumes the produced frame output for a window.
      [[nodiscard]] std::expected<void, vve::Error> consumeOutput(const FrameContext &, const SceneData &, WindowHandle,
                                                                  const RenderGraph &) {
         return {};
      }

      /**
       * @brief Registers render tasks for each active window pipeline.
       * @param builder Shared frame task-graph builder.
       * @param scene Runtime scene data for the current graph build.
       * @param render_pipelines Per-window render graphs to wire into the task graph.
       */
      void registerTasks(TaskGraphBuilder &builder, const SceneData &,
                         VectorConstRange<WindowRenderPipeline> render_pipelines) {
         for (const auto &pipeline : render_pipelines) {
            const auto window = pipeline.window;
            const auto *render_graph = &pipeline.graph;
            const auto cull_visibility_gpu_name = std::format("task.window.{}.cull_visibility_gpu", pipeline.window_id);
            const auto build_draw_packets_name = std::format("task.window.{}.build_draw_packets", pipeline.window_id);
            const auto record_render_graph_name = std::format("task.window.{}.record_render_graph", pipeline.window_id);
            const auto consume_frame_output_name =
                std::format("task.window.{}.consume_frame_output", pipeline.window_id);

            // Render work is serialized explicitly per window so later DAG
            // compilation does not have to infer pass ordering heuristically.
            const auto cull_visibility_gpu_task = builder.addTask(
                cull_visibility_gpu_name, TaskKernelId::cull_visibility_gpu,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return cullVisibilityGpu(frame_context, scene, window, *render_graph);
                }),
                {TaskGraphBuilder::taskHandleFor("task.upload_resources")}, {},
                std::string("Cull Visibility GPU (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
            const auto build_draw_packets_task = builder.addTask(
                build_draw_packets_name, TaskKernelId::build_draw_packets,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return buildDrawPackets(frame_context, scene, window, *render_graph);
                }),
                {cull_visibility_gpu_task}, {},
                std::string("Build Draw Packets (") + pipeline.window_id + ")", TaskPhase::render, TaskScope::window,
                pipeline.window);
            const auto record_render_graph_task = builder.addTask(
                record_render_graph_name, TaskKernelId::record_render_graph,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return record(frame_context, scene, window, *render_graph);
                }),
                {build_draw_packets_task}, {},
                std::string("Record Render Graph (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
            const auto consume_frame_output_task = builder.addTask(
                consume_frame_output_name, TaskKernelId::consume_frame_output,
                detail::requireFrameScene([this, window, render_graph](const FrameContext &frame_context,
                                                                       const SceneData &scene) {
                   return consumeOutput(frame_context, scene, window, *render_graph);
                }),
                {record_render_graph_task}, {},
                std::string("Consume Frame Output (") + pipeline.window_id + ")", TaskPhase::render,
                TaskScope::window, pipeline.window);
         }
      }

   private:
      vve::ShadowKind shadow_{vve::ShadowKind::none};                   ///< Selected shadowing strategy.
      GraphicsBackend &graphics_backend_;                               ///< Backend facade used by render diagnostics and later GPU work.
      bool imgui_enabled_{true};                                        ///< Whether the GUI pass should be appended.
   };

   /// @brief Constructs the public render-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(
       RenderSystemFacade, DefaultRenderSystemImplementation,
       (vve::RendererKind renderer, vve::ShadowKind shadow,
        GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend, bool imgui_enabled),
       (renderer, shadow, graphics_backend, imgui_enabled))

   /// @brief Returns the render-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Builds the static render graph through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, buildStaticGraph,
                               (WindowHandle window, const RendererDesc &renderer), (window, renderer), , RenderGraph)

   /// @brief Performs GPU visibility work through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, cullVisibilityGpu,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Builds draw packets through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, buildDrawPackets,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Records render work through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, record,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Consumes frame output through the public render-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, consumeOutput,
                               (const FrameContext &frame_context, const SceneData &scene, WindowHandle window,
                                const RenderGraph &render_graph),
                               (frame_context, scene, window, render_graph), , std::expected<void, vve::Error>)

   /// @brief Registers render tasks through the public render-system facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(RenderSystemFacade, DefaultRenderSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder, const SceneData &scene,
                                     VectorConstRange<WindowRenderPipeline> render_pipelines),
                                    (builder, scene, render_pipelines), )

   /// @brief Emits the explicit render-system facade instantiation for v3.
   template class RenderSystemFacade<DefaultRenderSystemImplementation>;

} // namespace vve::v3
