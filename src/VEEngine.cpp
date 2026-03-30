module VEEngine;
import VEEngine.V3;
import std;

namespace {

std::unique_ptr<VEEngine> makeEngine(VEEngineVersion version) {
    switch (version) {
    case VEEngineVersion::v3:
        return vve::v3::makeEngine();
    }

    std::unreachable();
}

} // namespace

VEEngine::VEEngine(VEEngineVersion version)
    : impl_(makeEngine(version)) {
}

VEEngine::VEEngine(ConstructionMode) noexcept {
}

VEEngine::~VEEngine() = default;

int VEEngine::getVersionMajor() const noexcept {
    if (impl_ != nullptr) {
        return impl_->getVersionMajor();
    }

    return 0;
}
