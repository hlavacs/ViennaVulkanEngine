#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string_view>

import VEEngine;
import VEEngine.V3;

namespace {

struct Velocity {
    float x{0.0F};
    float y{0.0F};
};

} // namespace

class SimpleGameSystem final {
public:
    [[nodiscard]] std::string_view name() const noexcept {
        return "SimpleGameSystem";
    }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World& world) {
        const auto player_result = world.spawn(vve::Transform{}, Velocity{});
        if (!player_result) {
            return std::unexpected(player_result.error());
        }

        player_ = *player_result;

        std::cout << '[' << name() << "] spawned entity " << player_.value() << '\n';
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> update(
        vve::World& world,
        const vve::v3::FrameContext& frame_context,
        const vve::v3::WindowFrameData&) {
        if (!player_.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

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

        velocity.x = 0.0F;
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

        if (input.wasKeyPressed('R') || input.wasKeyPressed('r')) {
            transform.translation = vve::math::zeroVec3();
        }

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

        if (frame_counter_++ % 120 == 0) {
            const auto main_window = world.findWindow("main");
            const auto player_x = static_cast<int>(std::lround(transform.translation.x));
            const auto player_y = static_cast<int>(std::lround(transform.translation.y));
            std::cout << '[' << name() << "] player=(" << player_x << ", " << player_y << ')';
            if (main_window) {
                std::cout << " window=" << main_window->width << 'x' << main_window->height;
            }
            std::cout << '\n';
        }

        return {};
    }

    vve::Handle player_{};
    std::uint64_t frame_counter_{0};
};

int main() {

    auto engine = vve::makeEngine(
        vve::ApplicationName{"game"},
        vve::EnableValidation{true},
        vve::makeUserSystems(SimpleGameSystem{}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "main",
                    .title = "VVE Game",
                    .width = 1600,
                    .height = 900,
                    .resizable = true,
                    .visible = true
                },
                vve::WindowDesc{
                    .id = "tools",
                    .title = "VVE Tools",
                    .width = 960,
                    .height = 720,
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
    

    if (!engine.init()) {
        return 1;
    }

    while (true) {
        const auto step_result = engine.step();
        if (!step_result) {
            return 1;
        }

        if (*step_result == vve::FrameStatus::should_close) {
            break;
        }
    }

    return 0;
}
