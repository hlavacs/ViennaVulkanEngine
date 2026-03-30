module;

module VEEngine;
import VEEngine.V3;
import std;

namespace {

std::unique_ptr<VEEngine> makeEngine(
    VeEngineVersion version,
    const VEEngineConfig& config) {
    switch (version) {
        case VeEngineVersion::v3: return vve::v3::makeEngine(config);
    }

    return nullptr;
}

} // namespace

VEEngine::VEEngine(VeEngineVersion version, VEEngineConfig config)
    : impl_(makeEngine(version, config)) {
}

VEEngine::VEEngine() noexcept : impl_(nullptr) {
}

VEEngine::~VEEngine() = default;

std::expected<int, VeResult> VEEngine::getVersionMajor() const noexcept {
    if (impl_ == nullptr) return std::unexpected(VeResult::internal_error); 
    return impl_->getVersionMajor();
}

std::expected<std::string, VeResult> VEEngine::loadFile(
    const std::filesystem::path& file_path) const {
    if (impl_ == nullptr) return std::unexpected(VeResult::internal_error); 
    return impl_->loadFile(file_path);
}
