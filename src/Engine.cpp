module;

module VEEngine;
import VEEngine.V3;
import std;

namespace {
// File-local helpers stay out of the public module surface and avoid symbol collisions.

std::unique_ptr<vve::Engine> makeEngine(
    vve::EngineVersion version,
    const vve::EngineConfig& config) {
    switch (version) {
        case vve::EngineVersion::v3: return vve::v3::makeEngine(config);
    }

    return nullptr;
}

} // namespace

vve::Engine::Engine(vve::EngineVersion version, vve::EngineConfig config)
    : impl_(makeEngine(version, config)) {
}

vve::Engine::Engine() noexcept
    : impl_(nullptr) {
}

vve::Engine::~Engine() = default;

std::expected<int, vve::Result> vve::Engine::getVersionMajor() const noexcept {
    if (impl_ == nullptr) return std::unexpected(vve::Result::internal_error);
    return impl_->getVersionMajor();
}

std::expected<std::string, vve::Result> vve::Engine::loadFile(
    const std::filesystem::path& file_path) const {
    if (impl_ == nullptr) return std::unexpected(vve::Result::internal_error);
    return impl_->loadFile(file_path);
}
