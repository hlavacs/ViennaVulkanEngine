import VEEngine;
import VEEngine.V3;
import std;

namespace {

struct Position {
    float x{0.0F};
    float y{0.0F};
};

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
        const auto player_result = world.ecs().create();
        if (!player_result) {
            return std::unexpected(player_result.error());
        }

        player_ = *player_result;

        if (const auto add_position_result = world.ecs().addComponent(player_, Position{}); !add_position_result) {
            return std::unexpected(add_position_result.error());
        }

        if (const auto add_velocity_result = world.ecs().addComponent(player_, Velocity{}); !add_velocity_result) {
            return std::unexpected(add_velocity_result.error());
        }

        std::cout << '[' << name() << "] spawned entity " << player_.value() << '\n';
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> update(
        vve::World& world,
        const vve::v3::FrameContext& frame_context,
        const vve::v3::WindowFrameData& window_frame) {
        if (!player_.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        const auto position_result = world.ecs().get<Position>(player_);
        if (!position_result) {
            return std::unexpected(position_result.error());
        }

        const auto velocity_result = world.ecs().get<Velocity>(player_);
        if (!velocity_result) {
            return std::unexpected(velocity_result.error());
        }

        if (!position_result->has_value() || !velocity_result->has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        auto position = **position_result;
        auto velocity = **velocity_result;

        for (const auto& event : window_frame.events) {
            switch (event.type) {
            case vve::v3::WindowEventType::key_down:
                applyKeyState(event.b, true, velocity);
                break;
            case vve::v3::WindowEventType::key_up:
                applyKeyState(event.b, false, velocity);
                break;
            case vve::v3::WindowEventType::close_requested:
                std::cout << '[' << name() << "] close requested for window "
                          << event.window.value.value() << '\n';
                break;
            default:
                break;
            }
        }

        position.x += velocity.x * static_cast<float>(frame_context.delta_seconds);
        position.y += velocity.y * static_cast<float>(frame_context.delta_seconds);

        constexpr float limit_x = 400.0F;
        constexpr float limit_y = 225.0F;

        if (position.x < -limit_x || position.x > limit_x) {
            position.x = std::clamp(position.x, -limit_x, limit_x);
            velocity.x = -velocity.x;
        }

        if (position.y < -limit_y || position.y > limit_y) {
            position.y = std::clamp(position.y, -limit_y, limit_y);
            velocity.y = -velocity.y;
        }

        if (const auto put_position_result = world.ecs().put(player_, position); !put_position_result) {
            return std::unexpected(put_position_result.error());
        }

        if (const auto put_velocity_result = world.ecs().put(player_, velocity); !put_velocity_result) {
            return std::unexpected(put_velocity_result.error());
        }

        if (frame_counter_++ % 120 == 0) {
            std::cout << '[' << name() << "] player=(" << position.x << ", " << position.y << ")\n";
        }

        return {};
    }

private:
    static void applyKeyState(std::int32_t keycode, bool pressed, Velocity& velocity) {
        constexpr float speed_x = 160.0F;
        constexpr float speed_y = 90.0F;

        switch (keycode) {
        case 'a':
        case 'A':
            velocity.x = pressed ? -speed_x : (velocity.x < 0.0F ? 0.0F : velocity.x);
            break;
        case 'd':
        case 'D':
            velocity.x = pressed ? speed_x : (velocity.x > 0.0F ? 0.0F : velocity.x);
            break;
        case 'w':
        case 'W':
            velocity.y = pressed ? -speed_y : (velocity.y < 0.0F ? 0.0F : velocity.y);
            break;
        case 's':
        case 'S':
            velocity.y = pressed ? speed_y : (velocity.y > 0.0F ? 0.0F : velocity.y);
            break;
        default:
            break;
        }
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
