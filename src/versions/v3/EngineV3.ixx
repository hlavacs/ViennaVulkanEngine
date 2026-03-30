export module VEEngine.V3;
import VEEngine;
import std;

namespace vve::v3 {

export std::unique_ptr<vve::detail::EngineImpl> makeEngine(const vve::EngineConfig& config);

} // namespace vve::v3
