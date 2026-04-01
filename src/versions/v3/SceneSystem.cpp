module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class SceneSystem final : public ISceneSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "SceneSystem";
    }

    [[nodiscard]] std::expected<SceneData, vve::Result> instantiate(
        const ImportedScene& scene) override {
        SceneData instance{};
        instance.handle = scene.handle;
        instance.nodes.reserve(scene.nodes.size());
        for (const auto& node : scene.nodes) {
            instance.nodes.push_back(SceneNodeDesc{
                .handle = node.handle,
                .parent = node.parent,
                .name = node.name,
                .local_transform = node.local_transform
            });
        }
        return instance;
    }

    [[nodiscard]] std::expected<void, vve::Result> updateTransforms(
        const FrameContext&,
        SceneData& scene) override {
        for (auto& node : scene.nodes) {
            node.local_transform = node.local_transform;
        }

        return {};
    }

    [[nodiscard]] std::expected<void, vve::Result> cullVisibility(
        const FrameContext&,
        const SceneData&) override {
        return {};
    }
};

} // namespace

std::unique_ptr<ISceneSystem> detail::createSceneSystem() {
    return std::make_unique<SceneSystem>();
}

} // namespace vve::v3
