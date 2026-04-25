#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Interactive example that drives a small user-system-based game loop.
 */
namespace {

[[nodiscard]] constexpr std::string_view toString(vve::Error error) noexcept {
    switch (error) {
    case vve::Error::not_initialized:
        return "not_initialized";
    case vve::Error::already_initialized:
        return "already_initialized";
    case vve::Error::invalid_argument:
        return "invalid_argument";
    case vve::Error::file_not_found:
        return "file_not_found";
    case vve::Error::io_error:
        return "io_error";
    case vve::Error::unsupported_version:
        return "unsupported_version";
    case vve::Error::internal_error:
        return "internal_error";
    }

    return "unknown_error";
}

/**
 * @brief Example gameplay component storing planar velocity.
 */
struct Velocity {
    /// @brief Horizontal velocity in world units per second.
    float x{0.0F};
    /// @brief Vertical velocity in world units per second.
    float y{0.0F};
};

} // namespace

/**
 * @brief Example game system that reads input and moves a single player entity.
 */
class SimpleGameSystem final {
public:
    /// @brief Returns the system name shown in diagnostics.
    [[nodiscard]] std::string_view name() const noexcept {
        return "SimpleGameSystem";
    }

    /**
     * @brief Creates the example player entity.
     * @param world Game-facing world facade provided by the engine.
     */
    [[nodiscard]] std::expected<void, vve::Error> init(vve::World& world) {
        // The example spawns one entity with a transform and velocity so the
        // update loop can demonstrate world and input access.
        const auto player_result = world.spawn(vve::Transform{}, Velocity{});
        if (!player_result) {
            return std::unexpected(player_result.error());
        }

        player_ = *player_result;

        std::cout << '[' << name() << "] spawned entity " << player_.value() << '\n';
        printWindowInventory(world);
        return {};
    }

    /**
     * @brief Processes one game update.
     * @param world Game-facing world facade used for entity and input access.
     * @param frame_context Timing data for the current frame.
     * @param window_frame Current window snapshot, unused by this example.
     */
    [[nodiscard]] std::expected<void, vve::Error> update(
        vve::World& world,
        const vve::v3::FrameContext& frame_context,
        const vve::v3::WindowFrameData&) {
        if (!player_.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        // Fetch the current velocity and transform copies through the world
        // facade before applying input-driven changes.
        const auto velocity_result = world.getComponent<Velocity>(player_);
        if (!velocity_result) {
            return std::unexpected(velocity_result.error());
        }

        const auto transform_result = world.getTransform(player_);
        if (!transform_result) {
            return std::unexpected(transform_result.error());
        }

        if (!transform_result->has_value() || !velocity_result->has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        auto transform = **transform_result;
        auto velocity = **velocity_result;
        const auto& input = world.input();

        velocity.x = 0.0F; // Reset velocity every frame so movement is purely input driven.
        velocity.y = 0.0F;
        if (input.isKeyDown('A') || input.isKeyDown('a')) {
            velocity.x -= 160.0F;
        }
        if (input.isKeyDown('D') || input.isKeyDown('d')) {
            velocity.x += 160.0F;
        }
        if (input.isKeyDown('W') || input.isKeyDown('w')) {
            velocity.y -= 90.0F;
        }
        if (input.isKeyDown('S') || input.isKeyDown('s')) {
            velocity.y += 90.0F;
        }

        // Reset the player back to the origin when R is pressed.
        if (input.wasKeyPressed('R') || input.wasKeyPressed('r')) {
            transform.translation = vve::math::zeroVec3();
        }

        // Integrate velocity using the frame delta and clamp movement to a
        // screen-like rectangle for the sample.
        transform.translation.x += velocity.x * static_cast<float>(frame_context.delta_seconds);
        transform.translation.y += velocity.y * static_cast<float>(frame_context.delta_seconds);

        constexpr float limit_x = 400.0F;
        constexpr float limit_y = 225.0F;

        transform.translation.x = std::clamp(transform.translation.x, -limit_x, limit_x);
        transform.translation.y = std::clamp(transform.translation.y, -limit_y, limit_y);

        if (const auto set_transform_result = world.setTransform(player_, transform); !set_transform_result) {
            return std::unexpected(set_transform_result.error());
        }

        if (const auto put_velocity_result = world.setComponent(player_, velocity); !put_velocity_result) {
            return std::unexpected(put_velocity_result.error());
        }

        const bool reset_pressed = input.wasKeyPressed('R') || input.wasKeyPressed('r');
        const std::array<char, 4> key_state{
            input.isKeyDown('A') || input.isKeyDown('a') ? 'A' : '-',
            input.isKeyDown('D') || input.isKeyDown('d') ? 'D' : '-',
            input.isKeyDown('W') || input.isKeyDown('w') ? 'W' : '-',
            input.isKeyDown('S') || input.isKeyDown('s') ? 'S' : '-'
        };
        const std::string key_state_string{key_state.data(), key_state.size()};
        log_accumulator_seconds_ += frame_context.delta_seconds;
        const bool periodic_log = log_accumulator_seconds_ >= 1.0;
        const bool key_state_changed = key_state_string != last_logged_key_state_;
        if (periodic_log) {
            log_accumulator_seconds_ = 0.0;
        }

        // Periodically print state so the example remains observable without
        if (key_state_changed || reset_pressed || periodic_log) { // any game-specific UI layer.
            const auto main_window = world.findWindow("main");
            const auto tools_window = world.findWindow("tools");
            const auto player_x = static_cast<int>(std::lround(transform.translation.x));
            const auto player_y = static_cast<int>(std::lround(transform.translation.y));
            std::cout << '[' << name() << "] player=(" << player_x << ", " << player_y << ')'
                      << " velocity=(" << static_cast<int>(std::lround(velocity.x)) << ", "
                      << static_cast<int>(std::lround(velocity.y)) << ')'
                      << " keys=" << key_state_string;
            if (main_window) {
                std::cout << " main=" << main_window->width << 'x' << main_window->height
                          << (main_window->focused ? "[focused]" : "");
            }
            if (tools_window) {
                std::cout << " tools=" << tools_window->width << 'x' << tools_window->height
                          << (tools_window->focused ? "[focused]" : "");
            }
            std::cout << '\n';
            last_logged_key_state_ = key_state_string;
        }

        return {};
    }

private:
    void printWindowInventory(vve::World& world) {
        std::cout << '[' << name() << "] windows:";
        bool printed_any = false;
        for (const auto& window : world.windows()) {
            printed_any = true;
            std::cout << ' ' << window.id << '=' << window.width << 'x' << window.height;
        }
        if (!printed_any) {
            std::cout << " <none>";
        }
        std::cout << '\n';
    }

    /// @brief Handle of the player entity created during initialization.
    vve::Handle player_{};
    /// @brief Tracks time until the next heartbeat log line.
    double log_accumulator_seconds_{0.0};
    /// @brief Stores the last emitted key-state summary so repeated idle frames stay quiet.
    std::string last_logged_key_state_{"----"};
};

/**
 * @brief Runs the interactive game example.
 * @return Process exit code expected by the example launcher.
 */
int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Configure a two-window sample runtime so the example exercises the
    auto engine = vve::makeEngine( // multi-window API shape exposed by the engine.
        vve::ApplicationName{"game"},
        vve::EnableValidation{true},
        vve::makeUserSystems(SimpleGameSystem{}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "main",
                    .title = "VVE Game",
                    .width = 800,
                    .height = 620,
                    .resizable = true,
                    .visible = true
                },
                vve::WindowDesc{
                    .id = "tools",
                    .title = "VVE Tools",
                    .width = 520,
                    .height = 620,
                    .resizable = true,
                    .visible = true
                }
            }
        });

    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }
    std::cout << "VVE " << *version_major << ".0\n";
    

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[game] engine.init failed: " << toString(init_result.error()) << '\n';
        return 1;
    }

    // Drive the engine one frame at a time so the example can react to the
    while (true) { // engine's explicit `FrameStatus` contract.
        const auto step_result = engine.step();
        if (!step_result) {
            std::cerr << "[game] engine.step failed: " << toString(step_result.error()) << '\n';
            return 1;
        }

        if (*step_result == vve::FrameStatus::should_close) {
            break;
        }
    }

    return 0;
}
