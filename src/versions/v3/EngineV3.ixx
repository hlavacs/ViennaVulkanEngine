export module VEEngine.V3;
import VEEngine;
import std;

namespace vve::v3 {

export std::unique_ptr<ve::Engine> makeEngine(const ve::EngineConfig& config);

} // namespace vve::v3
