#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string_view>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Minimal runtime example that boots the engine and runs the frame loop.
 */

/**
 * @brief Runs the physics sample shell.
 * @return Process exit code expected by the example launcher.
 */
int main() {
    // The physics sample currently exercises only engine startup and runtime
    auto engine = vve::makeEngine( // execution. Physics itself is expected to arrive through a user system.
        vve::ApplicationName{"physics"},
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "physics.main",
                    .title = "VVE Physics",
                    .width = 1280,
                    .height = 720,
                    .resizable = true,
                    .visible = true
                }
            }
        });

    if (!engine.init()) {
        return 1;
    }

    if (!engine.run()) {
        return 1;
    }

    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    return *version_major == 3 ? 0 : 1;
}
