module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

[[nodiscard]] RenderKernelId selectMainKernel(vve::RendererKind renderer) {
    switch (renderer) {
        case vve::RendererKind::forward_renderer:
            return RenderKernelId::forward_opaque;
        case vve::RendererKind::deferred_renderer:
            return RenderKernelId::deferred_gbuffer;
        case vve::RendererKind::path_tracing:
            return RenderKernelId::path_trace;
    }

    return RenderKernelId::none;
}

[[nodiscard]] std::string_view selectMainPassName(vve::RendererKind renderer) {
    switch (renderer) {
        case vve::RendererKind::forward_renderer:
            return "Forward Main";
        case vve::RendererKind::deferred_renderer:
            return "Deferred Main";
        case vve::RendererKind::path_tracing:
            return "Path Tracing Main";
    }

    return "Main";
}

[[nodiscard]] std::optional<RenderPassDesc> buildShadowPass(vve::ShadowKind shadow) {
    switch (shadow) {
        case vve::ShadowKind::none:
            return std::nullopt;
        case vve::ShadowKind::shadow_map:
            return RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.shadow_map")},
                .kernel = RenderKernelId::shadow_map,
                .debug_name = "Shadow Map"
            };
        case vve::ShadowKind::ray_traced:
            return RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.ray_traced_shadows")},
                .kernel = RenderKernelId::ray_traced_shadows,
                .debug_name = "Ray Traced Shadows"
            };
    }

    return std::nullopt;
}

class DefaultRenderSystemImplementation {
public:
    DefaultRenderSystemImplementation(
        vve::RendererKind renderer,
        vve::ShadowKind shadow,
        GraphicsBackend& graphics_backend,
        bool imgui_enabled)
        : renderer_(renderer),
          shadow_(shadow),
          graphics_backend_(graphics_backend),
          imgui_enabled_(imgui_enabled) {
    }

    [[nodiscard]] std::string_view name() const noexcept {
        return "RenderSystem";
    }

    [[nodiscard]] RenderGraph buildStaticGraph(WindowHandle window) {
        RenderGraph graph{};
        const auto window_salt = window.value.value();

        if (const auto shadow_pass = buildShadowPass(shadow_)) {
            graph.passes.push_back(*shadow_pass);
        }

        const auto main_pass = RenderPassHandle{detail::makeStableHandle("render.main", window_salt)};
        std::vector<RenderPassHandle> main_dependencies{};
        if (!graph.passes.empty()) {
            main_dependencies.push_back(graph.passes.front().handle);
        }
        graph.passes.push_back(RenderPassDesc{
            .handle = main_pass,
            .kernel = selectMainKernel(renderer_),
            .depends_on = std::move(main_dependencies),
            .debug_name = std::string(selectMainPassName(renderer_))
        });

        const auto post_process_pass = RenderPassHandle{
            detail::makeStableHandle("render.post_process", window_salt)
        };
        graph.passes.push_back(RenderPassDesc{
            .handle = post_process_pass,
            .kernel = RenderKernelId::post_process,
            .depends_on = {main_pass},
            .debug_name = "Post Processing"
        });

        const auto post_post_process_pass = RenderPassHandle{
            detail::makeStableHandle("render.post_post_process", window_salt)
        };
        graph.passes.push_back(RenderPassDesc{
            .handle = post_post_process_pass,
            .kernel = RenderKernelId::post_post_process,
            .depends_on = {post_process_pass},
            .debug_name = "Post Post Processing"
        });

        if (imgui_enabled_) {
            graph.passes.push_back(RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.imgui", window_salt)},
                .kernel = RenderKernelId::imgui,
                .depends_on = {post_post_process_pass},
                .debug_name = std::string("ImGui (") + std::string(graphics_backend_.name()) + ")"
            });
        }

        return graph;
    }

    [[nodiscard]] std::expected<void, vve::Error> cullVisibilityGpu(
        const FrameContext&,
        const SceneData&,
        WindowHandle,
        const RenderGraph&) {
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> buildDrawPackets(
        const FrameContext&,
        const SceneData&,
        WindowHandle,
        const RenderGraph&) {
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> record(
        const FrameContext&,
        const SceneData&,
        WindowHandle,
        const RenderGraph& render_graph) {
        for (const auto& pass : render_graph.passes) {
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> consumeOutput(
        const FrameContext&,
        const SceneData&,
        WindowHandle,
        const RenderGraph&) {
        return {};
    }

    void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData&,
        std::span<const WindowRenderPipeline> render_pipelines) {
        for (const auto& pipeline : render_pipelines) {
            const auto window = pipeline.window;
            const auto* render_graph = &pipeline.graph;
            const auto cull_visibility_gpu_name =
                std::format("task.window.{}.cull_visibility_gpu", pipeline.window_id);
            const auto build_draw_packets_name =
                std::format("task.window.{}.build_draw_packets", pipeline.window_id);
            const auto record_render_graph_name =
                std::format("task.window.{}.record_render_graph", pipeline.window_id);
            const auto consume_frame_output_name =
                std::format("task.window.{}.consume_frame_output", pipeline.window_id);

            const auto cull_visibility_gpu_task = builder.addTask(
                cull_visibility_gpu_name,
                TaskKernelId::cull_visibility_gpu,
                {},
                {TaskGraphBuilder::taskHandleFor("task.upload_resources")},
                {},
                std::string("Cull Visibility GPU (") + pipeline.window_id + ")",
                TaskScope::window,
                pipeline.window);
            const auto build_draw_packets_task = builder.addTask(
                build_draw_packets_name,
                TaskKernelId::build_draw_packets,
                {},
                {cull_visibility_gpu_task},
                {},
                std::string("Build Draw Packets (") + pipeline.window_id + ")",
                TaskScope::window,
                pipeline.window);
            const auto record_render_graph_task = builder.addTask(
                record_render_graph_name,
                TaskKernelId::record_render_graph,
                {},
                {build_draw_packets_task},
                {},
                std::string("Record Render Graph (") + pipeline.window_id + ")",
                TaskScope::window,
                pipeline.window);
            const auto consume_frame_output_task = builder.addTask(
                consume_frame_output_name,
                TaskKernelId::consume_frame_output,
                {},
                {record_render_graph_task},
                {},
                std::string("Consume Frame Output (") + pipeline.window_id + ")",
                TaskScope::window,
                pipeline.window);

            builder.setTaskCallback(
                cull_visibility_gpu_task,
                [this, window, render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                    if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                    }

                    return cullVisibilityGpu(
                        *execution_context.frame_context,
                        *execution_context.scene,
                        window,
                        *render_graph);
                });

            builder.setTaskCallback(
                build_draw_packets_task,
                [this, window, render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                    if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                    }

                    return buildDrawPackets(
                        *execution_context.frame_context,
                        *execution_context.scene,
                        window,
                        *render_graph);
                });

            builder.setTaskCallback(
                record_render_graph_task,
                [this, window, render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                    if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                    }

                    return record(
                        *execution_context.frame_context,
                        *execution_context.scene,
                        window,
                        *render_graph);
                });

            builder.setTaskCallback(
                consume_frame_output_task,
                [this, window, render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Error> {
                    if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                        return std::unexpected(vve::Error::invalid_argument);
                    }

                    return consumeOutput(
                        *execution_context.frame_context,
                        *execution_context.scene,
                        window,
                        *render_graph);
                });
        }
    }

private:
    vve::RendererKind renderer_{vve::RendererKind::forward_renderer};
    vve::ShadowKind shadow_{vve::ShadowKind::none};
    GraphicsBackend& graphics_backend_;
    bool imgui_enabled_{true};
};

template <>
RenderSystemFacade<DefaultRenderSystemImplementation>::RenderSystemFacade(
    vve::RendererKind renderer,
    vve::ShadowKind shadow,
    GraphicsBackendFacade<VulkanGraphicsBackendImplementation>& graphics_backend,
    bool imgui_enabled)
    : implementation_(
          new DefaultRenderSystemImplementation(
              renderer,
              shadow,
              graphics_backend,
              imgui_enabled),
          [](void* implementation) {
              delete static_cast<DefaultRenderSystemImplementation*>(implementation);
          }) {
}

template <>
std::string_view RenderSystemFacade<DefaultRenderSystemImplementation>::name() const noexcept {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())->name();
}

template <>
RenderGraph RenderSystemFacade<DefaultRenderSystemImplementation>::buildStaticGraph(WindowHandle window) {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())->buildStaticGraph(window);
}

template <>
std::expected<void, vve::Error> RenderSystemFacade<DefaultRenderSystemImplementation>::cullVisibilityGpu(
    const FrameContext& frame_context,
    const SceneData& scene,
    WindowHandle window,
    const RenderGraph& render_graph) {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())
        ->cullVisibilityGpu(frame_context, scene, window, render_graph);
}

template <>
std::expected<void, vve::Error> RenderSystemFacade<DefaultRenderSystemImplementation>::buildDrawPackets(
    const FrameContext& frame_context,
    const SceneData& scene,
    WindowHandle window,
    const RenderGraph& render_graph) {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())
        ->buildDrawPackets(frame_context, scene, window, render_graph);
}

template <>
std::expected<void, vve::Error> RenderSystemFacade<DefaultRenderSystemImplementation>::record(
    const FrameContext& frame_context,
    const SceneData& scene,
    WindowHandle window,
    const RenderGraph& render_graph) {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())
        ->record(frame_context, scene, window, render_graph);
}

template <>
std::expected<void, vve::Error> RenderSystemFacade<DefaultRenderSystemImplementation>::consumeOutput(
    const FrameContext& frame_context,
    const SceneData& scene,
    WindowHandle window,
    const RenderGraph& render_graph) {
    return static_cast<DefaultRenderSystemImplementation*>(implementation_.get())
        ->consumeOutput(frame_context, scene, window, render_graph);
}

template <>
void RenderSystemFacade<DefaultRenderSystemImplementation>::registerTasks(
    TaskGraphBuilder& builder,
    const SceneData& scene,
    std::span<const WindowRenderPipeline> render_pipelines) {
    static_cast<DefaultRenderSystemImplementation*>(implementation_.get())
        ->registerTasks(builder, scene, render_pipelines);
}

template class RenderSystemFacade<DefaultRenderSystemImplementation>;

} // namespace vve::v3
