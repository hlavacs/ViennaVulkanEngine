module;

#include <cassert>

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

VEEngine::VEEngine() noexcept
    : impl_(nullptr) {
}

VEEngine::~VEEngine() = default;

int VEEngine::getVersionMajor() const noexcept {
    assert(impl_ != nullptr);
    return impl_->getVersionMajor();
}

std::string VEEngine::loadFile(const std::filesystem::path& file_path) const {
    assert(impl_ != nullptr);
    return impl_->loadFile(file_path);
}
