module;

module VEEngine;
import VEEngine.V3;
import std;

namespace {
// File-local helpers stay out of the public module surface and avoid symbol collisions.

std::unique_ptr<ve::Engine> makeEngine(
    ve::EngineVersion version,
    const ve::EngineConfig& config) {
    switch (version) {
        case ve::EngineVersion::v3: return vve::v3::makeEngine(config);
    }

    return nullptr;
}

} // namespace

ve::Engine::Engine(ve::EngineVersion version, ve::EngineConfig config)
    : impl_(makeEngine(version, config)) {
}

ve::Engine::Engine() noexcept
    : impl_(nullptr) {
}

ve::Engine::~Engine() = default;

std::expected<int, ve::Result> ve::Engine::getVersionMajor() const noexcept {
    if (impl_ == nullptr) return std::unexpected(ve::Result::internal_error);
    return impl_->getVersionMajor();
}

std::expected<std::string, ve::Result> ve::Engine::loadFile(
    const std::filesystem::path& file_path) const {
    if (impl_ == nullptr) return std::unexpected(ve::Result::internal_error);
    return impl_->loadFile(file_path);
}
