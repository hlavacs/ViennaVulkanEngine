module;

module VEEngine;
import VEEngine.V3;
import std;

template <typename TImplementation>
vve::EngineFacade<TImplementation>::EngineFacade(vve::EngineConfig config)
    : implementation_(std::move(config)) {
}

template <typename TImplementation>
std::expected<void, vve::Error> vve::EngineFacade<TImplementation>::init() {
    return implementation_.init();
}

template <typename TImplementation>
std::expected<void, vve::Error> vve::EngineFacade<TImplementation>::run() {
    return implementation_.run();
}

template <typename TImplementation>
std::expected<vve::FrameStatus, vve::Error> vve::EngineFacade<TImplementation>::step() {
    return implementation_.step();
}

template <typename TImplementation>
std::expected<bool, vve::Error> vve::EngineFacade<TImplementation>::isInitialized() const noexcept {
    return implementation_.isInitialized();
}

template <typename TImplementation>
std::expected<int, vve::Error> vve::EngineFacade<TImplementation>::getVersionMajor() const noexcept {
    return implementation_.getVersionMajor();
}

template <typename TImplementation>
std::expected<void, vve::Error> vve::EngineFacade<TImplementation>::loadFile(
    const std::filesystem::path& file_path) {
    return implementation_.loadFile(file_path);
}

template class vve::EngineFacade<vve::v3::EngineImplementation>;
