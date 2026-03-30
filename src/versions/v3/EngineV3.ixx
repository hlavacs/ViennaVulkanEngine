export module VEEngine.V3;
import VEEngine;
import std;

namespace vve::v3 {

export std::unique_ptr<vve::Engine> makeEngine(const vve::EngineConfig& config);

} // namespace vve::v3
