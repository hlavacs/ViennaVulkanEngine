#include <string_view>

import VEEngine;
import VEEngine.V3;

namespace {

class CounterSystem final : public vve::System {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "CounterSystem";
    }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World& world) {
        const auto entity = world.ecs().create();
        if (!entity) {
            return std::unexpected(entity.error());
        }

        return {};
    }
};

} // namespace

int main() {
    auto engine = vve::makeEngine(
        vve::ApplicationName{"user-systems"},
        vve::makeUserSystems(CounterSystem{}));

    const auto version_major = engine.getVersionMajor();
    if (!version_major || *version_major != 3) {
        return 1;
    }

    return 0;
}
