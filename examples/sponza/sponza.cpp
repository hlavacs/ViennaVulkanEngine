#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import VEEngine.V4;

/**
 * @file
 * @brief v4 Sponza runtime stub: creates a window and reports the scene path.
 */

namespace {

namespace ve = vve::v4;

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

    [[nodiscard]] std::expected<void, ve::Error> init(ve::World& world) {
        std::cout << '[' << name() << "] scene path: " << scene_path_.string() << '\n';
        std::cout << '[' << name() << "] v4 runtime shell is active\n";
        const auto scene_handle = world.loadScene(scene_path_);
        if (!scene_handle) {
            std::cerr << '[' << name() << "] Assimp import failed: "
                      << ve::errorName(scene_handle.error()) << '\n';
            return std::unexpected(scene_handle.error());
        }

        const auto* catalog = world.objectCatalog();
        if (catalog == nullptr) {
            return std::unexpected(ve::Error::missing_object);
        }
        const auto* scene = catalog->scenes.find(*scene_handle);
        if (scene == nullptr) {
            return std::unexpected(ve::Error::missing_object);
        }

        std::cout << '[' << name() << "] imported scene handle=" << scene_handle->raw().value
                  << " name=" << scene->name.value << '\n';
        printSceneInventory(*catalog, *scene);
        std::cout << '[' << name() << "] v4 resource upload and rendering are not implemented yet\n";
        printWindowInventory(world);
        return {};
    }

    [[nodiscard]] std::expected<void, ve::Error> update(
        ve::World&,
        const ve::FrameContext& frame_context,
        const ve::WindowFrameData&) {
        if (!frame_loop_logged_ && frame_context.frame_index.value > 0) {
            std::cout << '[' << name() << "] frame loop active; close the window to exit\n";
            frame_loop_logged_ = true;
        }
        return {};
    }

private:
    void printWindowInventory(const ve::World& world) const {
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
    }

    void printSceneInventory(const ve::ObjectCatalog& catalog, const ve::SceneDescriptor& scene) const {
        std::cout << '[' << name() << "] counts: nodes=" << scene.nodes.size()
                  << " meshes=" << scene.meshes.size()
                  << " materials=" << scene.materials.size()
                  << " textures=" << scene.textures.size()
                  << " lights=" << scene.lights.size()
                  << " cameras=" << scene.cameras.size() << '\n';
        printMeshes(catalog, scene.meshes);
        printTextures(catalog, scene.textures);
        printLights(catalog, scene.lights);
        printCameras(catalog, scene.cameras);
    }

    void printMeshes(const ve::ObjectCatalog& catalog, const ve::Vector<ve::MeshHandle>& handles) const {
        std::cout << '[' << name() << "] meshes:\n";
        for (const auto handle : handles) {
            const auto* mesh = catalog.meshes.find(handle);
            if (mesh == nullptr) {
                continue;
            }
            std::cout << "  mesh " << mesh->handle.raw().value << " name=" << mesh->name.value
                      << " vertices=" << mesh->vertex_count.value
                      << " indices=" << mesh->index_count.value
                      << " material=" << mesh->material.raw().value << '\n';
        }
    }

    void printTextures(const ve::ObjectCatalog& catalog, const ve::Vector<ve::TextureHandle>& handles) const {
        std::cout << '[' << name() << "] textures:\n";
        for (const auto handle : handles) {
            const auto* texture = catalog.textures.find(handle);
            if (texture == nullptr) {
                continue;
            }
            std::cout << "  texture " << texture->handle.raw().value << " name=" << texture->name.value
                      << " source=" << texture->source.string()
                      << " size=" << texture->extent.width << 'x' << texture->extent.height << '\n';
        }
    }

    void printLights(const ve::ObjectCatalog& catalog, const ve::Vector<ve::LightHandle>& handles) const {
        std::cout << '[' << name() << "] lights:\n";
        for (const auto handle : handles) {
            const auto* light = catalog.lights.find(handle);
            if (light == nullptr) {
                continue;
            }
            std::cout << "  light " << light->handle.raw().value << " name=" << light->name.value
                      << " intensity=" << light->intensity.value << '\n';
        }
    }

    void printCameras(const ve::ObjectCatalog& catalog, const ve::Vector<ve::CameraHandle>& handles) const {
        std::cout << '[' << name() << "] cameras:\n";
        for (const auto handle : handles) {
            const auto* camera = catalog.cameras.find(handle);
            if (camera == nullptr) {
                continue;
            }
            std::cout << "  camera " << camera->handle.raw().value << " name=" << camera->name.value
                      << " near=" << camera->clip.near_plane << " far=" << camera->clip.far_plane << '\n';
        }
    }

    std::filesystem::path scene_path_{}; ///< Resolved Sponza file imported during init().
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

    auto engine = ve::makeEngine(
        ve::ApplicationName{"sponza"},
        ve::makeUserSystems(SponzaRuntimeStubSystem{*scene_path}),
        ve::Windows{
            .value = {
                ve::WindowDesc{
                    .id = "sponza.main",
                    .title = "VVE Sponza",
                    .extent = ve::PixelExtent{.width = 960, .height = 540},
                    .renderer_id = ve::RendererId{.value = "forward"},
                    .resizable = true,
                    .visible = true
                }
            }
        });

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[sponza] engine.init failed: " << ve::errorName(init_result.error()) << '\n';
        return 1;
    }

    if (const auto run_result = engine.run(); !run_result) {
        std::cerr << "[sponza] engine.run failed: " << ve::errorName(run_result.error()) << '\n';
        return 1;
    }

    return 0;
}
