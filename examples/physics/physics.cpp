#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string_view>

import VEEngine;

/**
 * @file
 * @brief Minimal runtime example that boots the engine and runs the frame loop.
 */

namespace {

class PhysicsShellSystem final {
public:
    [[nodiscard]] std::string_view name() const noexcept {
        return "PhysicsShellSystem";
    }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World& world) {
        std::cout << '[' << name() << "] windows:";
        bool printed_any = false;
        for (const auto& window : world.windows()) {
            printed_any = true;
            std::cout << ' ' << window.id << '=' << window.extent.width << 'x' << window.extent.height
                      << '[' << window.renderer_id.value << ']';
        }
        if (!printed_any) {
            std::cout << " <none>";
        }
        std::cout << '\n';
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> update(
        vve::World& world,
        const vve::FrameContext& frame_context,
        const vve::WindowFrameData&) {
        const auto& input = world.input();
        if (input.isKeyDown('A') || input.isKeyDown('a')) {
            body_x_ -= 120.0 * frame_context.delta_time.seconds;
        }
        if (input.isKeyDown('D') || input.isKeyDown('d')) {
            body_x_ += 120.0 * frame_context.delta_time.seconds;
        }
        if (input.isKeyDown('W') || input.isKeyDown('w')) {
            body_y_ -= 120.0 * frame_context.delta_time.seconds;
        }
        if (input.isKeyDown('S') || input.isKeyDown('s')) {
            body_y_ += 120.0 * frame_context.delta_time.seconds;
        }

        log_accumulator_seconds_ += frame_context.delta_time.seconds;
        if (log_accumulator_seconds_ < 1.0) {
            return {};
        }
        log_accumulator_seconds_ = 0.0;

        const auto main_window = world.findWindow("physics.main");
        std::cout << '[' << name() << ']';
        if (!main_window) {
            std::cout << " physics.main=<missing>\n";
            return {};
        }

        const auto mouse_delta = input.mouseDelta(main_window->handle);
        std::cout << " physics.main=" << main_window->extent.width << 'x' << main_window->extent.height
                  << '[' << main_window->renderer_id.value << ']'
                  << (main_window->focused ? "[focused]" : "")
                  << (main_window->minimized ? "[minimized]" : "")
                  << " body=(" << body_x_ << ", " << body_y_ << ')'
                  << " keys="
                  << (input.isKeyDown('W') || input.isKeyDown('w') ? 'W' : '-')
                  << (input.isKeyDown('A') || input.isKeyDown('a') ? 'A' : '-')
                  << (input.isKeyDown('S') || input.isKeyDown('s') ? 'S' : '-')
                  << (input.isKeyDown('D') || input.isKeyDown('d') ? 'D' : '-')
                  << " mouse_delta=(" << mouse_delta.x << ", " << mouse_delta.y << ")\n";
        return {};
    }

private:
    double log_accumulator_seconds_{0.0};
    double body_x_{0.0};
    double body_y_{0.0};
};

} // namespace

/**
 * @brief Runs the physics sample shell.
 * @return Process exit code expected by the example launcher.
 */
int main(int, char **) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // The physics sample currently exercises only engine startup and runtime
    auto engine = vve::makeEngine( // execution. Physics itself is expected to arrive through a user system.
        vve::ApplicationName{"physics"},
        vve::makeUserSystems(PhysicsShellSystem{}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "physics.main",
                    .title = "VVE Physics Sandbox",
                    .extent = vve::PixelExtent{.width = 800, .height = 450},
                    .renderer_id = vve::RendererId{.value = "forward"},
                    .resizable = true,
                    .visible = true
                }
            }
        });

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[physics] engine.init failed: " << vve::errorName(init_result.error()) << '\n';
        return 1;
    }

    if (const auto run_result = engine.run(); !run_result) {
        std::cerr << "[physics] engine.run failed: " << vve::errorName(run_result.error()) << '\n';
        return 1;
    }

    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }

    return *version_major == 4 ? 0 : 1;
}
