#include <algorithm>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Example program that imports the Sponza scene and prints its contents.
 */
namespace {

[[nodiscard]] std::optional<std::filesystem::path>
firstExistingPath(const std::vector<std::filesystem::path> &candidates) {
    for (const auto &candidate : candidates) {
        std::error_code error_code{};
        if (std::filesystem::exists(candidate, error_code) && !error_code) {
            return std::filesystem::weakly_canonical(candidate, error_code);
        }
    }

    return std::nullopt;
}

void appendSceneCandidates(std::vector<std::filesystem::path> &candidates, const std::filesystem::path &root) {
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

    for (const auto &directory : scene_directories) {
        for (const auto &file_name : scene_file_names) {
            candidates.push_back(directory / file_name);
        }
    }
}

[[nodiscard]] std::filesystem::path executableDirectory(char **argv) {
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

[[nodiscard]] bool wantsVerboseSceneDump(int argc, char **argv) {
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == nullptr) {
            continue;
        }

        const std::string_view argument{argv[argument_index]};
        if (argument == "--verbose" || argument == "--dump-all") {
            return true;
        }
    }

    if (const char *verbose = std::getenv("VVE_SPONZA_VERBOSE"); verbose != nullptr) {
        const std::string_view value{verbose};
        return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
    }

    return false;
}

[[nodiscard]] bool wantsRuntimeSceneLoad(int argc, char **argv) {
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == nullptr) {
            continue;
        }

        const std::string_view argument{argv[argument_index]};
        if (argument == "--load-runtime-scene") {
            return true;
        }
    }

    if (const char *load_runtime_scene = std::getenv("VVE_SPONZA_LOAD_RUNTIME_SCENE");
        load_runtime_scene != nullptr) {
        const std::string_view value{load_runtime_scene};
        return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
    }

    return false;
}

[[nodiscard]] std::optional<std::filesystem::path> resolveScenePath(int argc, char **argv) {
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == nullptr || argv[argument_index][0] == '\0' || argv[argument_index][0] == '-') {
            continue;
        }

        if (std::optional<std::filesystem::path> direct_path =
                firstExistingPath({std::filesystem::path(argv[argument_index])});
            direct_path.has_value()) {
            return direct_path;
        }
    }

    if (const char *environment_path = std::getenv("VVE_SPONZA_SCENE");
        environment_path != nullptr && environment_path[0] != '\0') {
        if (std::optional<std::filesystem::path> environment_scene =
                firstExistingPath({std::filesystem::path(environment_path)});
            environment_scene.has_value()) {
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

[[nodiscard]] std::uint64_t rawHandle(vve::Handle handle) {
    return handle.value();
}

void printVec2(const vve::math::Vec2 &value) {
    std::cout << '(' << value.x << ", " << value.y << ')';
}

void printVec3(const vve::math::Vec3 &value) {
    std::cout << '(' << value.x << ", " << value.y << ", " << value.z << ')';
}

void printVec4(const vve::math::Vec4 &value) {
    std::cout << '(' << value.x << ", " << value.y << ", " << value.z << ", " << value.w << ')';
}

void printMat4(const vve::math::Mat4 &matrix, const std::string_view indent) {
    for (int row = 0; row < 4; ++row) {
        std::cout << indent << '[' << matrix[0][row] << ", " << matrix[1][row] << ", " << matrix[2][row] << ", "
                  << matrix[3][row] << "]\n";
    }
}

void printTextures(const vve::v3::ImportedScene &scene) {
    std::cout << "Textures (" << scene.textures.size() << ")\n";
    for (std::size_t texture_index = 0; texture_index < scene.textures.size(); ++texture_index) {
        const auto &texture = scene.textures[texture_index];
        std::cout << "  [" << texture_index << "] handle=" << rawHandle(texture.handle.value)
                  << " name=\"" << texture.name << "\" embedded=" << std::boolalpha << texture.embedded
                  << std::noboolalpha << '\n';
        std::cout << "    original_path=\"" << texture.original_path.string() << "\"\n";
        std::cout << "    resolved_path=\"" << texture.resolved_path.string() << "\"\n";
        std::cout << "    embedded_id=\"" << texture.embedded_id << "\"\n";
    }
}

void printMaterials(const vve::v3::ImportedScene &scene) {
    std::cout << "Materials (" << scene.materials.size() << ")\n";
    for (std::size_t material_index = 0; material_index < scene.materials.size(); ++material_index) {
        const auto &material = scene.materials[material_index];
        std::cout << "  [" << material_index << "] handle=" << rawHandle(material.handle.value)
                  << " name=\"" << material.name << "\"\n";
        std::cout << "    base_color_factor=";
        printVec4(material.base_color_factor);
        std::cout << '\n';
        std::cout << "    emissive_factor=";
        printVec3(material.emissive_factor);
        std::cout << '\n';
        std::cout << "    roughness_factor=" << material.roughness_factor
                  << " metallic_factor=" << material.metallic_factor
                  << " normal_scale=" << material.normal_scale
                  << " alpha_cutoff=" << material.alpha_cutoff << '\n';
        std::cout << "    double_sided=" << std::boolalpha << material.double_sided
                  << " alpha_blend=" << material.alpha_blend << std::noboolalpha << '\n';
        std::cout << "    textures (" << material.textures.size() << ")\n";
        for (std::size_t texture_ref_index = 0; texture_ref_index < material.textures.size(); ++texture_ref_index) {
            const auto &texture_ref = material.textures[texture_ref_index];
            std::cout << "      [" << texture_ref_index << "] texture=" << rawHandle(texture_ref.texture.value)
                      << " semantic=" << vve::v3::textureSemanticName(texture_ref.semantic)
                      << " uv_set=" << texture_ref.uv_set << '\n';
        }
    }
}

void printMeshes(const vve::v3::ImportedScene &scene) {
    std::cout << "Meshes (" << scene.meshes.size() << ")\n";
    for (std::size_t mesh_index = 0; mesh_index < scene.meshes.size(); ++mesh_index) {
        const auto &mesh = scene.meshes[mesh_index];
        std::cout << "  [" << mesh_index << "] handle=" << rawHandle(mesh.handle.value)
                  << " name=\"" << mesh.name << "\"\n";
        std::cout << "    source_path=\"" << mesh.source_path.string() << "\"\n";
        std::cout << "    bounds_min=";
        printVec3(mesh.bounds_min);
        std::cout << " bounds_max=";
        printVec3(mesh.bounds_max);
        std::cout << '\n';
        std::cout << "    vertex_count=" << mesh.vertices.size()
                  << " index_count=" << mesh.indices.size() << '\n';

        std::cout << "    submeshes (" << mesh.submeshes.size() << ")\n";
        for (std::size_t submesh_index = 0; submesh_index < mesh.submeshes.size(); ++submesh_index) {
            const auto &submesh = mesh.submeshes[submesh_index];
            std::cout << "      [" << submesh_index << "] index_offset=" << submesh.index_offset
                      << " index_count=" << submesh.index_count
                      << " material=" << rawHandle(submesh.material.value) << '\n';
        }
    }
}

void printNodes(const vve::v3::ImportedScene &scene) {
    std::cout << "Nodes (" << scene.nodes.size() << ")\n";
    for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const auto &node = scene.nodes[node_index];
        std::cout << "  [" << node_index << "] handle=" << rawHandle(node.handle.value)
                  << " parent=" << rawHandle(node.parent.value)
                  << " name=\"" << node.name << "\"\n";
        std::cout << "    local_transform\n";
        printMat4(node.local_transform, "      ");
        std::cout << "    mesh_instances (" << node.mesh_instances.size() << ")\n";
        for (std::size_t mesh_instance_index = 0; mesh_instance_index < node.mesh_instances.size();
             ++mesh_instance_index) {
            const auto &mesh_instance = node.mesh_instances[mesh_instance_index];
            std::cout << "      [" << mesh_instance_index << "] handle=" << rawHandle(mesh_instance.handle)
                      << " mesh=" << rawHandle(mesh_instance.mesh.value);
            if (mesh_instance.material_override.has_value()) {
                std::cout << " material_override=" << rawHandle(mesh_instance.material_override->value);
            } else {
                std::cout << " material_override=<none>";
            }
            std::cout << '\n';
        }
    }
}

void printScene(const vve::v3::ImportedScene &scene) {
    std::cout << "Scene\n";
    std::cout << "  handle=" << rawHandle(scene.handle.value) << '\n';
    std::cout << "  name=\"" << scene.name << "\"\n";
    std::cout << "  source_path=\"" << scene.source_path.string() << "\"\n";
    std::cout << "  texture_count=" << scene.textures.size() << '\n';
    std::cout << "  material_count=" << scene.materials.size() << '\n';
    std::cout << "  mesh_count=" << scene.meshes.size() << '\n';
    std::cout << "  node_count=" << scene.nodes.size() << '\n';
    printTextures(scene);
    printMaterials(scene);
    printMeshes(scene);
    printNodes(scene);
}

void printMainObjects(const vve::v3::ImportedScene &scene) {
    std::cout << "Main objects\n";
    std::cout << "  scene=\"" << scene.name << "\" meshes=" << scene.meshes.size()
              << " materials=" << scene.materials.size()
              << " textures=" << scene.textures.size()
              << " nodes=" << scene.nodes.size() << '\n';

    std::vector<const vve::v3::ImportedSceneNode *> root_nodes{};
    for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const auto &node = scene.nodes[node_index];
        if (node.parent.value.isValid()) {
            continue;
        }

        root_nodes.push_back(&node);
        std::cout << "  root[" << node_index << "] handle=" << rawHandle(node.handle.value)
                  << " name=\"" << node.name << "\" mesh_instances=" << node.mesh_instances.size() << '\n';
    }

    std::size_t top_level_node_count = 0;
    std::cout << "  top_level_nodes\n";
    for (std::size_t node_index = 0; node_index < scene.nodes.size(); ++node_index) {
        const auto &node = scene.nodes[node_index];
        const bool is_top_level = std::ranges::any_of(root_nodes, [&node](const auto *root_node) {
            return root_node != nullptr && node.parent.value.value() == root_node->handle.value.value();
        });
        if (!is_top_level) {
            continue;
        }

        ++top_level_node_count;
        std::cout << "    [" << node_index << "] name=\"" << node.name
                  << "\" mesh_instances=" << node.mesh_instances.size() << '\n';
    }

    if (top_level_node_count == 0) {
        std::cout << "    <none>\n";
    }

    std::cout << "  root_node_count=" << root_nodes.size() << '\n';
    std::cout << "  top_level_node_count=" << top_level_node_count << '\n';
}

class SponzaLoaderSystem final {
public:
    SponzaLoaderSystem() = default;
    explicit SponzaLoaderSystem(std::filesystem::path scene_path, const bool load_runtime_scene = false)
        : scene_path_(std::move(scene_path)), load_runtime_scene_(load_runtime_scene) {}

    [[nodiscard]] std::string_view name() const noexcept { return "SponzaLoaderSystem"; }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World &world) {
        if (loaded_) {
            return {};
        }

        if (scene_path_.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        if (!load_runtime_scene_) {
            std::cout << '[' << name() << "] scene imported for object listing; runtime scene load disabled\n";
            std::cout << '[' << name()
                      << "] pass --load-runtime-scene or set VVE_SPONZA_LOAD_RUNTIME_SCENE=1 to load it into the runtime\n";
            printWindowInventory(world);
            loaded_ = true;
            return {};
        }

        std::cout << '[' << name() << "] loading scene into runtime: " << scene_path_.string() << '\n';
        if (const auto load_result = world.loadScene(scene_path_); !load_result) {
            std::cerr << '[' << name() << "] failed to load scene into runtime: " << scene_path_.string() << '\n';
            return std::unexpected(load_result.error());
        }

        loaded_ = true;
        std::cout << '[' << name() << "] scene loaded into runtime: " << scene_path_.string() << '\n';
        printWindowInventory(world);
        return {};
    }

    [[nodiscard]] std::expected<void, vve::Error> update(
        vve::World &world,
        const vve::v3::FrameContext &frame_context,
        const vve::v3::WindowFrameData &window_frame) {
        (void)world;
        (void)frame_context;
        (void)window_frame;
        return {};
    }

private:
    void printWindowInventory(vve::World &world) {
        std::cout << '[' << name() << "] windows:";
        bool printed_any = false;
        for (const auto &window : world.windows()) {
            printed_any = true;
            std::cout << ' ' << window.id << '=' << window.width << 'x' << window.height;
        }
        if (!printed_any) {
            std::cout << " <none>";
        }
        std::cout << '\n';
    }

    std::filesystem::path scene_path_{};
    bool load_runtime_scene_{false};
    bool loaded_{false};
};

} // namespace

/**
 * @brief Imports the Sponza scene, prints its contents, and opens a render window.
 * @param argc Process argument count.
 * @param argv Process argument vector.
 * @return Process exit code expected by the example launcher.
 */
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const bool verbose_scene_dump = wantsVerboseSceneDump(argc, argv);
    const bool load_runtime_scene = wantsRuntimeSceneLoad(argc, argv);
    const auto scene_path = resolveScenePath(argc, argv);
    if (!scene_path.has_value()) {
        std::cerr << "[sponza] Unable to locate the Sponza scene.\n";
        std::cerr << "[sponza] Pass the scene file path as the first argument or set VVE_SPONZA_SCENE.\n";
        return 1;
    } else {
        std::cout << "[sponza] using scene: " << scene_path->string() << '\n';

        vve::v3::AssetSystem asset_system{};
        const auto imported_scene = asset_system.importScene(*scene_path);
        if (!imported_scene) {
            std::cerr << "[sponza] Failed to import scene: " << scene_path->string() << '\n';
            return 1;
        }

        std::cout << std::fixed << std::setprecision(6);
        printMainObjects(*imported_scene);
        if (verbose_scene_dump) {
            printScene(*imported_scene);
        } else {
            std::cout << "[sponza] Pass --verbose or set VVE_SPONZA_VERBOSE=1 for the full scene dump.\n";
        }
    }

    auto engine = vve::makeEngine(
        vve::ApplicationName{"sponza"},
        vve::EnableValidation{true},
        vve::makeUserSystems(SponzaLoaderSystem{*scene_path, load_runtime_scene}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "sponza.main",
                    .title = "VVE Sponza",
                    .width = 960,
                    .height = 540,
                    .resizable = true,
                    .visible = true}}});

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[sponza] engine.init failed: " << vve::errorName(init_result.error()) << '\n';
        return 1;
    }

    while (true) {
        const auto step_result = engine.step();
        if (!step_result) {
            std::cerr << "[sponza] engine.step failed: " << vve::errorName(step_result.error()) << '\n';
            return 1;
        }

        if (*step_result == vve::FrameStatus::should_close) {
            break;
        }
    }

    return 0;
}
