#include <string_view>

import std;
import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Regression test for user-system engine specialization.
 *
 * The test verifies that a user system can be supplied through `makeEngine()`
 * and that the returned engine is still backed by the expected v3 runtime.
 */
namespace {

/**
 * @brief Minimal user system used to exercise user-system detection.
 */
class CounterSystem final {
public:
    /// @brief Returns the system name used by engine task registration.
    [[nodiscard]] std::string_view name() const noexcept {
        return "CounterSystem";
    }

    /**
     * @brief Optional initialization hook used by the engine.
     * @param world Game-facing world facade passed to user systems.
     */
    [[nodiscard]] std::expected<void, vve::Error> init(vve::World& world) {
        // Creating an entity proves that the world facade is wired up before
        const auto entity = world.createEntity(); // user-system initialization executes.
        if (!entity) {
            return std::unexpected(entity.error());
        }

        return {};
    }
};

} // namespace

/**
 * @brief Executes the user-system integration regression.
 * @return Process exit code expected by the lightweight test runner.
 */
int main() {
    // Creating the engine through `makeUserSystems()` should specialize the
    auto engine = vve::makeEngine( // engine type without changing the selected runtime version.
        vve::ApplicationName{"user-systems"},
        vve::makeUserSystems(CounterSystem{}));

    const auto version_major = engine.getVersionMajor();
    if (!version_major || *version_major != 3) {
        return 1;
    }

    return 0;
}
