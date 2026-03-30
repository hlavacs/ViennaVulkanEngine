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

    [[nodiscard]] RenderGraph build(
        const FrameContext&,
        const SceneData&,
        const TaskGraph&,
        const ShaderMetadata&) override {
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
            .depends_on = std::move(main_dependencies),
            .debug_name = std::string(selectMainPassName(renderer_))
        });

        if (imgui_enabled_) {
            graph.passes.push_back(RenderPassDesc{
                .handle = RenderPassHandle{detail::makeStableHandle("render.imgui")},
                .kernel = RenderKernelId::imgui,
                .depends_on = {main_pass},
                .debug_name = std::format("ImGui ({})", graphics_backend_.name())
            });
        }

        return graph;
    }

    [[nodiscard]] std::expected<void, vve::Result> render(
        const FrameContext&,
        const RenderGraph&) override {
        return {};
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
