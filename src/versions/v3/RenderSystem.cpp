module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

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
                .phase = RenderTaskPhase::main,
                .debug_name = "Shadow Map"
            };
        case vve::ShadowKind::ray_traced:
            return RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.ray_traced_shadows")},
                .kernel = RenderKernelId::ray_traced_shadows,
                .phase = RenderTaskPhase::main,
                .debug_name = "Ray Traced Shadows"
            };
    }

    return std::nullopt;
}

class RenderSystem final : public IRenderSystem {
public:
    RenderSystem(
        vve::RendererKind renderer,
        vve::ShadowKind shadow,
        IGraphicsBackend& graphics_backend,
        bool imgui_enabled)
        : renderer_(renderer),
          shadow_(shadow),
          graphics_backend_(graphics_backend),
          imgui_enabled_(imgui_enabled) {
    }

    [[nodiscard]] std::string_view name() const noexcept override {
        return "RenderSystem";
    }

    [[nodiscard]] RenderGraph buildStaticGraph() override {
        RenderGraph graph{};

        if (const auto shadow_pass = buildShadowPass(shadow_)) {
            graph.passes.push_back(*shadow_pass);
        }

        const auto main_pass = RenderPassHandle{detail::makeStableHandle("render.main")};
        std::vector<RenderPassHandle> main_dependencies{};
        if (!graph.passes.empty()) {
            main_dependencies.push_back(graph.passes.front().handle);
        }
        graph.passes.push_back(RenderPassDesc{
            .handle = main_pass,
            .kernel = selectMainKernel(renderer_),
            .phase = RenderTaskPhase::main,
            .depends_on = std::move(main_dependencies),
            .debug_name = std::string(selectMainPassName(renderer_))
        });

        const auto post_process_pass = RenderPassHandle{detail::makeStableHandle("render.post_process")};
        graph.passes.push_back(RenderPassDesc{
            .handle = post_process_pass,
            .kernel = RenderKernelId::post_process,
            .phase = RenderTaskPhase::post_process,
            .depends_on = {main_pass},
            .debug_name = "Post Processing"
        });

        const auto post_post_process_pass = RenderPassHandle{
            detail::makeStableHandle("render.post_post_process")
        };
        graph.passes.push_back(RenderPassDesc{
            .handle = post_post_process_pass,
            .kernel = RenderKernelId::post_post_process,
            .phase = RenderTaskPhase::post_process,
            .depends_on = {post_process_pass},
            .debug_name = "Post Post Processing"
        });

        if (imgui_enabled_) {
            graph.passes.push_back(RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.imgui")},
                .kernel = RenderKernelId::imgui,
                .phase = RenderTaskPhase::post_process,
                .depends_on = {post_post_process_pass},
                .debug_name = std::string("ImGui (") + std::string(graphics_backend_.name()) + ")"
            });
        }

        return graph;
    }

    [[nodiscard]] std::expected<void, vve::Result> cullVisibilityGpu(
        const FrameContext&,
        const SceneData&,
        const RenderGraph&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> buildDrawPackets(
        const FrameContext&,
        const SceneData&,
        const RenderGraph&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> record(
        const FrameContext&,
        const SceneData&,
        const RenderGraph& render_graph,
        RenderTaskPhase phase) override {
        for (const auto& pass : render_graph.passes) {
            if (pass.phase != phase) {
                continue;
            }
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> consumeOutput(
        const FrameContext&,
        const SceneData&,
        const RenderGraph&) override {
        return {};
    }

    void registerTasks(
        TaskGraphBuilder& builder,
        const SceneData&,
        const RenderGraph& render_graph) override {
        const auto cull_visibility_gpu_task = builder.addTask(
            "task.cull_visibility_gpu",
            TaskKernelId::cull_visibility_gpu,
            {},
            {TaskGraphBuilder::taskHandleFor("task.cull_visibility_cpu")},
            {},
            "Cull Visibility GPU");
        const auto build_draw_packets_task = builder.addTask(
            "task.build_draw_packets",
            TaskKernelId::build_draw_packets,
            {},
            {cull_visibility_gpu_task},
            {},
            "Build Draw Packets");
        const auto record_render_graph_task = builder.addTask(
            "task.record_render_graph",
            TaskKernelId::record_render_graph,
            {},
            {build_draw_packets_task},
            {},
            "Record Render Graph");
        const auto record_post_processing_task = builder.addTask(
            "task.record_post_processing",
            TaskKernelId::record_post_processing,
            {},
            {record_render_graph_task},
            {},
            "Record Post Processing");
        const auto consume_frame_output_task = builder.addTask(
            "task.consume_frame_output",
            TaskKernelId::consume_frame_output,
            {},
            {record_post_processing_task},
            {},
            "Consume Frame Output");

        builder.setTaskCallback(
            cull_visibility_gpu_task,
            [this, &render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return cullVisibilityGpu(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph);
            });

        builder.setTaskCallback(
            build_draw_packets_task,
            [this, &render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return buildDrawPackets(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph);
            });

        builder.setTaskCallback(
            record_render_graph_task,
            [this, &render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return record(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph,
                    RenderTaskPhase::main);
            });

        builder.setTaskCallback(
            record_post_processing_task,
            [this, &render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return record(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph,
                    RenderTaskPhase::post_process);
            });

        builder.setTaskCallback(
            consume_frame_output_task,
            [this, &render_graph](const TaskExecutionContext& execution_context) -> std::expected<void, vve::Result> {
                if (execution_context.frame_context == nullptr || execution_context.scene == nullptr) {
                    return std::unexpected(vve::Result::invalid_argument);
                }

                return consumeOutput(
                    *execution_context.frame_context,
                    *execution_context.scene,
                    render_graph);
            });
    }

private:
    vve::RendererKind renderer_{vve::RendererKind::forward_renderer};
    vve::ShadowKind shadow_{vve::ShadowKind::none};
    IGraphicsBackend& graphics_backend_;
    bool imgui_enabled_{true};
};

} // namespace

std::unique_ptr<IRenderSystem> detail::createRenderSystem(
    vve::RendererKind renderer,
    vve::ShadowKind shadow,
    IGraphicsBackend& graphics_backend,
    bool imgui_enabled) {
    return std::make_unique<RenderSystem>(renderer, shadow, graphics_backend, imgui_enabled);
}

} // namespace vve::v3
