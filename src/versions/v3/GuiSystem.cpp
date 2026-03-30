module VEEngine.V3;
import std;
#include "V3Internal.hpp"

namespace vve::v3 {

namespace {

class ImGuiSystem final : public IGuiSystem {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "ImGuiSystem";
    }

    [[nodiscard]] std::expected<void, vve::Result> init(
        IGraphicsBackend&) override {
        initialized_ = true;
        return {};
    }

private:
    bool initialized_{false};
};

} // namespace

std::unique_ptr<IGuiSystem> detail::createGuiSystem() {
    return std::make_unique<ImGuiSystem>();
}

} // namespace vve::v3
