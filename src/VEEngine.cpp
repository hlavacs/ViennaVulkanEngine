module VEEngine;
import VEEngine.V3;
import std;

namespace {

class EngineConcept {
public:
    virtual ~EngineConcept() = default;
    [[nodiscard]] virtual int getVersionMajor() const noexcept = 0;
};

template <typename T>
class EngineModel final : public EngineConcept {
public:
    [[nodiscard]] int getVersionMajor() const noexcept override {
        return engine_.getVersionMajor();
    }

private:
    T engine_{};
};

std::unique_ptr<EngineConcept> makeEngine(VEEngineVersion version) {
    switch (version) {
    case VEEngineVersion::v3:
        return std::make_unique<EngineModel<vve::v3::Engine>>();
    }

    std::unreachable();
}

} // namespace

class VEEngine::Impl {
public:
    explicit Impl(VEEngineVersion version)
        : engine_(makeEngine(version)) {
    }

    [[nodiscard]] int getVersionMajor() const noexcept {
        return engine_->getVersionMajor();
    }

private:
    std::unique_ptr<EngineConcept> engine_;
};

VEEngine::VEEngine(VEEngineVersion version)
    : impl_(std::make_unique<Impl>(version)) {
}

VEEngine::~VEEngine() = default;

int VEEngine::getVersionMajor() const noexcept {
    return impl_->getVersionMajor();
}
