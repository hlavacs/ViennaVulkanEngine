#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import VEEngine;

/**
 * @file
 * @brief Interactive example that drives a small user-system-based game loop.
 */
namespace {

[[nodiscard]] std::optional<std::filesystem::path>
firstExistingPath(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& candidate : candidates) {
        std::error_code error_code{};
        if (std::filesystem::is_regular_file(candidate, error_code) && !error_code) {
            return std::filesystem::weakly_canonical(candidate, error_code);
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::filesystem::path executableDirectory(char** argv) {
    if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return {};
    }

    std::error_code error_code{};
    const auto executable_path = std::filesystem::absolute(std::filesystem::path(argv[0]), error_code);
    if (error_code) {
        return {};
    }

    return executable_path.parent_path();
}

void appendGameSceneCandidates(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& root) {
    if (root.empty()) {
        return;
    }

    candidates.push_back(root / "assets" / "fox" / "Fox.gltf");
    candidates.push_back(root / "assets" / "sea_keep_lonely_watcher" / "scene.gltf");
}

[[nodiscard]] std::optional<std::filesystem::path> resolveGameScenePath(int argc, char** argv) {
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == nullptr || argv[argument_index][0] == '\0') {
            continue;
        }

        const std::string_view argument{argv[argument_index]};
        if (argument == "--scene" && argument_index + 1 < argc && argv[argument_index + 1] != nullptr) {
            if (auto direct_path = firstExistingPath({std::filesystem::path(argv[argument_index + 1])})) {
                return direct_path;
            }
            ++argument_index;
            continue;
        }

        if (!argument.empty() && argument.front() != '-') {
            if (auto direct_path = firstExistingPath({std::filesystem::path(std::string(argument))})) {
                return direct_path;
            }
        }
    }

    if (const char* environment_path = std::getenv("VVE_GAME_SCENE");
        environment_path != nullptr && environment_path[0] != '\0') {
        if (auto environment_scene = firstExistingPath({std::filesystem::path(environment_path)})) {
            return environment_scene;
        }
    }

    const auto current_directory = std::filesystem::current_path();
    const auto executable_directory = executableDirectory(argv);

    std::vector<std::filesystem::path> candidates{};
    appendGameSceneCandidates(candidates, current_directory);
    appendGameSceneCandidates(candidates, current_directory.parent_path());
    appendGameSceneCandidates(candidates, executable_directory);
    appendGameSceneCandidates(candidates, executable_directory.parent_path().parent_path());
    appendGameSceneCandidates(candidates, executable_directory.parent_path().parent_path().parent_path());
    return firstExistingPath(candidates);
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
    /// @brief Creates the sample system with a scene path reserved for future v4 rendering.
    explicit SimpleGameSystem(std::filesystem::path scene_path = {})
        : scene_path_(std::move(scene_path)) {}

    /// @brief Returns the system name shown in diagnostics.
    [[nodiscard]] std::string_view name() const noexcept {
        return "SimpleGameSystem";
    }

    /**
     * @brief Creates the example player entity.
     * @param world Game-facing world facade provided by the engine.
     */
    template <typename TWorld> [[nodiscard]] std::expected<void, vve::Error> init(TWorld& world) {
        // The example spawns one entity with a transform and velocity so the
        // update loop can demonstrate world and input access.
        const auto player_result = world.spawn(vve::Transform{}, Velocity{});
        if (!player_result) {
            return std::unexpected(player_result.error());
        }

        player_ = *player_result;

        if (scene_path_.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        std::cout << '[' << name() << "] registering scene path: " << scene_path_.string() << '\n';
        if (const auto load_result = world.loadScene(scene_path_); !load_result) {
            return std::unexpected(load_result.error());
        }

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
    template <typename TWorld> [[nodiscard]] std::expected<void, vve::Error> update(
        TWorld& world,
        const vve::FrameContext& frame_context,
        const auto&) {
        if (!player_.valid()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        // Fetch the current velocity and transform copies through the world
        // facade before applying input-driven changes.
        const auto velocity_result = world.template getComponent<Velocity>(player_);
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
        const auto input = world.windowSystem().input();
        const auto delta_seconds = static_cast<float>(frame_context.delta_time.seconds);

        float acceleration_x = 0.0F;
        float acceleration_y = 0.0F;
        if (input.isKeyDown('A') || input.isKeyDown('a')) {
            acceleration_x = -10.0F;
        }
        if (input.isKeyDown('D') || input.isKeyDown('d')) {
            acceleration_x = 10.0F;
        }
        if (input.isKeyDown('W') || input.isKeyDown('w')) {
            acceleration_y = 10.0F;
        }
        if (input.isKeyDown('S') || input.isKeyDown('s')) {
            acceleration_y = -10.0F;
        }
        velocity.x += acceleration_x * delta_seconds;
        velocity.y += acceleration_y * delta_seconds;

        // Reset the player back to the origin when R is pressed.
        if (input.wasKeyPressed('R') || input.wasKeyPressed('r')) {
            transform.translation = vve::Position{};
            velocity = Velocity{};
        }

        transform.translation.value.x += velocity.x * delta_seconds;
        transform.translation.value.y += velocity.y * delta_seconds;

        if (const auto set_transform_result = world.setTransform(player_, transform); !set_transform_result) {
            return std::unexpected(set_transform_result.error());
        }

        if (const auto put_velocity_result = world.template setComponent<Velocity>(player_, velocity);
            !put_velocity_result) {
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
        const bool movement_key_held = key_state_string != "----";

        if (movement_key_held || reset_pressed) {
            const auto main_window = world.windowSystem().findWindow("main");
            const auto tools_window = world.windowSystem().findWindow("tools");
            const auto player_x = static_cast<int>(std::lround(transform.translation.value.x));
            const auto player_y = static_cast<int>(std::lround(transform.translation.value.y));
            std::cout << '[' << name() << "] player=(" << player_x << ", " << player_y << ')'
                      << " velocity=(" << static_cast<int>(std::lround(velocity.x)) << ", "
                      << static_cast<int>(std::lround(velocity.y)) << ')'
                      << " acceleration=(" << static_cast<int>(std::lround(acceleration_x)) << ", "
                      << static_cast<int>(std::lround(acceleration_y)) << ')'
                      << " keys=" << key_state_string;
            if (main_window) {
                const auto extent = main_window->extent();
                std::cout << " main=" << extent.width << 'x' << extent.height
                          << '[' << main_window->rendererId().value << ']'
                          << (main_window->focused() ? "[focused]" : "");
            }
            if (tools_window) {
                const auto extent = tools_window->extent();
                std::cout << " tools=" << extent.width << 'x' << extent.height
                          << '[' << tools_window->rendererId().value << ']'
                          << (tools_window->focused() ? "[focused]" : "");
            }
            std::cout << '\n';
        }

        return {};
    }

private:
    template <typename TWorld> void printWindowInventory(TWorld& world) {
        std::cout << '[' << name() << "] windows:";
        bool printed_any = false;
        for (const auto& window_ref : world.windowSystem().windows()) {
            const auto& window = window_ref.get();
            printed_any = true;
            const auto extent = window.extent();
            std::cout << ' ' << window.id() << '=' << extent.width << 'x' << extent.height
                      << '[' << window.rendererId().value << ']';
        }
        if (!printed_any) {
            std::cout << " <none>";
        }
        std::cout << '\n';
    }

    /// @brief Typed handle of the player entity created during initialization.
    vve::Entity player_{};
    /// @brief Runtime scene loaded so public camera changes are visible.
    std::filesystem::path scene_path_{};
};

/**
 * @brief Runs the interactive game example.
 * @return Process exit code expected by the example launcher.
 */
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const auto scene_path = resolveGameScenePath(argc, argv);
    if (!scene_path) {
        std::cerr << "[game] unable to locate a game scene. Pass --scene <path> or set VVE_GAME_SCENE.\n";
        return 1;
    }

    // Configure a two-window sample runtime so the example exercises the
    auto engine = vve::makeEngine( // multi-window API shape exposed by the engine.
        vve::ApplicationName{"game"},
        vve::makeUserSystems(SimpleGameSystem{*scene_path}),
        vve::WindowSetups{
            vve::WindowSetup{}
                .id("main")
                .title("VVE Game")
                .extent(vve::PixelExtent{.width = 640, .height = 480})
                .position(80, 120)
                .renderer(vve::RendererId{.value = "forward"})
                .resizable(true)
                .visible(true),
            vve::WindowSetup{}
                .id("tools")
                .title("VVE Tools")
                .extent(vve::PixelExtent{.width = 400, .height = 480})
                .position(760, 120)
                .renderer(vve::RendererId{.value = "forward"})
                .resizable(true)
                .visible(true)});

    const auto version_major = engine.getVersionMajor();
    if (!version_major) {
        return 1;
    }
    std::cout << "VVE " << *version_major << ".0\n";
    

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[game] engine.init failed: " << vve::errorName(init_result.error()) << '\n';
        return 1;
    }

    // Drive the engine one frame at a time so the example can react to the
    while (true) { // engine's explicit `FrameStatus` contract.
        const auto step_result = engine.step();
        if (!step_result) {
            std::cerr << "[game] engine.step failed: " << vve::errorName(step_result.error()) << '\n';
            return 1;
        }

        if (*step_result == vve::FrameStatus::should_close) {
            break;
        }
    }

    return 0;
}
