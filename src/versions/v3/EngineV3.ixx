export module VEEngine.V3;
import VEEngine;
export import VEEngine.V3.Types;
export import VEEngine.V3.Systems;
import std;

namespace vve::v3 {

export std::unique_ptr<vve::detail::EngineImpl> makeEngine(const vve::EngineConfig& config);

} // namespace vve::v3
