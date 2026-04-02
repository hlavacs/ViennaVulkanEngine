module VEEngine.V3;
import std;
import :Internal;

namespace vve::v3 {

class ImGuiSystemImplementation {
public:
    [[nodiscard]] std::string_view name() const noexcept {
        return "ImGuiSystem";
    }

    [[nodiscard]] std::expected<void, vve::Error> init(
        GraphicsBackend&) {
        initialized_ = true;
        return {};
    }

private:
    bool initialized_{false};
};

template <>
GuiSystemFacade<ImGuiSystemImplementation>::GuiSystemFacade()
    : implementation_(
          new ImGuiSystemImplementation(),
          [](ImGuiSystemImplementation* implementation) {
              delete implementation;
          }) {
}

std::string_view GuiSystemFacade<ImGuiSystemImplementation>::name() const noexcept {
    return implementation_->name();
}

template <>
std::expected<void, vve::Error> GuiSystemFacade<ImGuiSystemImplementation>::init(
    GraphicsBackendFacade<VulkanGraphicsBackendImplementation>& graphics_backend) {
    return implementation_->init(graphics_backend);
}

template class GuiSystemFacade<ImGuiSystemImplementation>;

} // namespace vve::v3
