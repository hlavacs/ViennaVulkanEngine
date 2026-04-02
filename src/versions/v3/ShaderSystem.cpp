module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

[[nodiscard]] std::string_view toRendererName(vve::RendererKind renderer) {
    switch (renderer) {
        case vve::RendererKind::forward_renderer:
            return "forward";
        case vve::RendererKind::deferred_renderer:
            return "deferred";
        case vve::RendererKind::path_tracing:
            return "path_tracing";
    }

    return "unknown";
}

[[nodiscard]] std::string_view toShadowName(vve::ShadowKind shadow) {
    switch (shadow) {
        case vve::ShadowKind::none:
            return "none";
        case vve::ShadowKind::shadow_map:
            return "shadow_map";
        case vve::ShadowKind::ray_traced:
            return "ray_traced";
    }

    return "unknown";
}

class SlangShaderSystem final : public IShaderSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "SlangShaderSystem";
    }

    [[nodiscard]] std::expected<ShaderMetadata, vve::Error> reflect(
        const std::filesystem::path& shader_path,
        vve::RendererKind renderer,
        vve::ShadowKind shadow) override {
        ShaderMetadata metadata{};
        metadata.handle = ShaderHandle{detail::makeStableHandle(shader_path.string())};
        metadata.shader_name = shader_path.filename().string();
        metadata.stages = {ShaderStage::vertex, ShaderStage::fragment};
        metadata.parameters = {
            ShaderParameter{.name = "FrameConstants", .type_name = "cbuffer", .binding = 0, .set = 0},
            ShaderParameter{.name = "MaterialParams", .type_name = "parameter_block", .binding = 1, .set = 0}
        };
        metadata.intended_renderer = std::string(toRendererName(renderer));
        metadata.intended_shadow = std::string(toShadowName(shadow));
        return metadata;
    }
};

} // namespace

std::unique_ptr<IShaderSystem> detail::createShaderSystem() {
    return std::make_unique<SlangShaderSystem>();
}

} // namespace vve::v3
