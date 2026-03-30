module VEEngine;
import :V3;

VEEngine::~VEEngine() = default;

class VEEngineDelegator::Impl {
public:
    [[nodiscard]] int getVersionMajor() const noexcept {
        return engine_.getVersionMajor();
    }

private:
    vve::v3::VEEngine engine_{};
};

VEEngineDelegator::VEEngineDelegator()
    : impl_(std::make_unique<Impl>()) {
}

VEEngineDelegator::~VEEngineDelegator() = default;

int VEEngineDelegator::getVersionMajor() const noexcept {
    return impl_->getVersionMajor();
}
