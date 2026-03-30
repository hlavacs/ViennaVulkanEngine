module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class VulkanGraphicsBackend final : public IGraphicsBackend {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "VulkanGraphicsBackend";
    }

    [[nodiscard]] vve::GraphicsApi api() const noexcept override {
        return vve::GraphicsApi::vulkan;
    }

    [[nodiscard]] std::expected<void, vve::Result> init() override {
        initialized_ = true;
        return {};
    }

private:
    bool initialized_{false};
};

} // namespace

std::expected<std::unique_ptr<IGraphicsBackend>, vve::Result> detail::createGraphicsBackend(
    vve::GraphicsApi api) {
    switch (api) {
        case vve::GraphicsApi::vulkan:
            return std::make_unique<VulkanGraphicsBackend>();
        case vve::GraphicsApi::direct3d12:
        case vve::GraphicsApi::metal:
            return std::unexpected(vve::Result::unsupported_version);
    }

    return std::unexpected(vve::Result::internal_error);
}

} // namespace vve::v3
