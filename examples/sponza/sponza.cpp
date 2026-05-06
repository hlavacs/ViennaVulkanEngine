#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import VEEngine;

/**
 * @file
 * @brief v4 Sponza runtime stub: creates a window and reports the scene path.
 */

namespace {

[[nodiscard]] std::optional<std::filesystem::path>
firstExistingPath(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& candidate : candidates) {
        std::error_code error_code{};
        if (std::filesystem::exists(candidate, error_code) && !error_code) {
            return std::filesystem::weakly_canonical(candidate, error_code);
        }
    }
    return std::nullopt;
}

void appendSceneCandidates(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& root) {
    if (root.empty()) {
        return;
    }

    const std::vector<std::filesystem::path> scene_file_names{
        "Sponza.gltf",
        "Sponza.glb",
        "NewSponza_Main_glTF_003.gltf"
    };
    const std::vector<std::filesystem::path> scene_directories{
        root,
        root / "Sponza",
        root / "sponza",
        root / "assets" / "Sponza",
        root / "assets" / "sponza",
        root / "main_sponza"
    };

    for (const auto& directory : scene_directories) {
        for (const auto& file_name : scene_file_names) {
            candidates.push_back(directory / file_name);
        }
    }
}

[[nodiscard]] std::filesystem::path executableDirectory(char** argv) {
    if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
        return {};
    }

    std::error_code error_code{};
    const auto executable_path = std::filesystem::absolute(std::filesystem::path(argv[0]), error_code);
    return error_code ? std::filesystem::path{} : executable_path.parent_path();
}

[[nodiscard]] std::optional<std::filesystem::path> resolveScenePath(int argc, char** argv) {
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

    if (const char* environment_path = std::getenv("VVE_SPONZA_SCENE");
        environment_path != nullptr && environment_path[0] != '\0') {
        if (auto environment_scene = firstExistingPath({std::filesystem::path(environment_path)})) {
            return environment_scene;
        }
    }

    const auto current_directory = std::filesystem::current_path();
    const auto executable_directory = executableDirectory(argv);

    std::vector<std::filesystem::path> candidates{};
    appendSceneCandidates(candidates, current_directory);
    appendSceneCandidates(candidates, current_directory.parent_path());
    appendSceneCandidates(candidates, executable_directory);
    appendSceneCandidates(candidates, executable_directory.parent_path().parent_path());
    appendSceneCandidates(candidates, executable_directory.parent_path().parent_path().parent_path());
    return firstExistingPath(candidates);
}

class SponzaRuntimeStubSystem final {
public:
    explicit SponzaRuntimeStubSystem(std::filesystem::path scene_path)
        : scene_path_(std::move(scene_path)) {}

    [[nodiscard]] std::string_view name() const noexcept {
        return "SponzaRuntimeStubSystem";
    }

    template <typename TWorld> [[nodiscard]] std::expected<void, vve::Error> init(TWorld& world) {
        std::cout << '[' << name() << "] scene path: " << scene_path_.string() << '\n';
        std::cout << '[' << name() << "] v4 runtime shell is active\n";
        auto assets = world.assets();
        const auto loaded_scene = assets.loadScene(scene_path_);
        if (!loaded_scene) {
            std::cerr << '[' << name() << "] Assimp import failed: "
                      << vve::errorName(loaded_scene.error()) << '\n';
            return std::unexpected(loaded_scene.error());
        }

        scene_ = *loaded_scene;
        const auto nodes = assets.sceneNodeCount(scene_);
        const auto meshes = assets.sceneMeshCount(scene_);
        const auto materials = assets.sceneMaterialCount(scene_);
        const auto textures = assets.sceneTextureCount(scene_);
        const auto lights = assets.sceneLightCount(scene_);
        const auto cameras = assets.sceneCameraCount(scene_);
        if (!nodes || !meshes || !materials || !textures || !lights || !cameras) {
            return std::unexpected(vve::Error::missing_object);
        }

        std::cout << '[' << name() << "] stored scene handle=" << scene_.value()
                  << " nodes=" << *nodes
                  << " meshes=" << *meshes
                  << " materials=" << *materials
                  << " textures=" << *textures
                  << " lights=" << *lights
                  << " cameras=" << *cameras << '\n';
        std::cout << '[' << name() << "] v4 resource upload and rendering are not implemented yet\n";
        printWindowInventory(world);
        return {};
    }

    template <typename TWorld> [[nodiscard]] std::expected<void, vve::Error> update(
        TWorld&,
        const vve::FrameContext& frame_context,
        const auto&) {
        if (!frame_loop_logged_ && frame_context.frame_index.value > 0) {
            std::cout << '[' << name() << "] frame loop active; close the window to exit\n";
            frame_loop_logged_ = true;
        }
        return {};
    }

private:
    template <typename TWorld> void printWindowInventory(const TWorld& world) const {
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

    std::filesystem::path scene_path_{}; ///< Resolved Sponza file imported during init().
    vve::SceneHandle scene_{};           ///< Scene stored in the facade asset system.
    bool frame_loop_logged_{false};      ///< Keeps the runtime heartbeat to one line.
};

} // namespace

/**
 * @brief Runs the v4 Sponza stub.
 * @return Process exit code expected by the example launcher.
 */
int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const auto scene_path = resolveScenePath(argc, argv);
    if (!scene_path) {
        std::cerr << "[sponza] unable to locate a Sponza scene. Pass --scene <path> or set VVE_SPONZA_SCENE.\n";
        return 1;
    }

    auto engine = vve::makeEngine(
        vve::ApplicationName{"sponza"},
        vve::makeUserSystems(SponzaRuntimeStubSystem{*scene_path}),
        vve::WindowSetups{
            vve::WindowSetup{}
                .id("sponza.main")
                .title("VVE Sponza")
                .extent(vve::PixelExtent{.width = 960, .height = 540})
                .renderer(vve::RendererId{.value = "forward"})
                .resizable(true)
                .visible(true)});

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[sponza] engine.init failed: " << vve::errorName(init_result.error()) << '\n';
        return 1;
    }

    if (const auto run_result = engine.run(); !run_result) {
        std::cerr << "[sponza] engine.run failed: " << vve::errorName(run_result.error()) << '\n';
        return 1;
    }

    return 0;
}
