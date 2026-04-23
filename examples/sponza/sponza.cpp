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

[[nodiscard]] std::optional<std::filesystem::path> resolveScenePath(int argc, char **argv) {
    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        if (std::optional<std::filesystem::path> direct_path =
                firstExistingPath({std::filesystem::path(argv[1])});
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
    return firstExistingPath({
        std::filesystem::path("C:/data/GitHub/main_sponza/NewSponza_Main_glTF_003.gltf"),
        current_directory / "Sponza.gltf",
        current_directory / "Sponza" / "Sponza.gltf",
        current_directory / "sponza" / "Sponza.gltf",
        current_directory / "assets" / "Sponza" / "Sponza.gltf",
        current_directory / "assets" / "sponza" / "Sponza.gltf",
        current_directory / "Sponza.glb",
        current_directory / "Sponza" / "Sponza.glb",
        current_directory / "sponza" / "Sponza.glb",
        current_directory / "assets" / "Sponza" / "Sponza.glb",
        current_directory / "assets" / "sponza" / "Sponza.glb"});
}

[[nodiscard]] std::uint64_t rawHandle(vve::Handle handle) {
    return handle.value();
}

[[nodiscard]] const char *textureSemanticName(vve::v3::TextureSemantic semantic) {
    using vve::v3::TextureSemantic;
    switch (semantic) {
    case TextureSemantic::unknown:
        return "unknown";
    case TextureSemantic::base_color:
        return "base_color";
    case TextureSemantic::normal:
        return "normal";
    case TextureSemantic::metallic_roughness:
        return "metallic_roughness";
    case TextureSemantic::roughness:
        return "roughness";
    case TextureSemantic::metallic:
        return "metallic";
    case TextureSemantic::specular:
        return "specular";
    case TextureSemantic::emissive:
        return "emissive";
    case TextureSemantic::opacity:
        return "opacity";
    case TextureSemantic::ambient_occlusion:
        return "ambient_occlusion";
    }

    return "unknown";
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
                      << " semantic=" << textureSemanticName(texture_ref.semantic)
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

class SponzaLoaderSystem final {
public:
    SponzaLoaderSystem() = default;
    explicit SponzaLoaderSystem(std::filesystem::path scene_path) : scene_path_(std::move(scene_path)) {}

    [[nodiscard]] std::string_view name() const noexcept { return "SponzaLoaderSystem"; }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World &world) {
        if (loaded_) {
            return {};
        }

        if (scene_path_.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        if (const auto load_result = world.loadScene(scene_path_); !load_result) {
            std::cerr << '[' << name() << "] failed to load scene into runtime: " << scene_path_.string() << '\n';
            return std::unexpected(load_result.error());
        }

        loaded_ = true;
        std::cout << '[' << name() << "] scene loaded into runtime: " << scene_path_.string() << '\n';
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
    std::filesystem::path scene_path_{};
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
    const auto scene_path = resolveScenePath(argc, argv);
    if (!scene_path.has_value()) {
        std::cerr << "Unable to locate the Sponza scene.\n";
        std::cerr << "Pass the scene file path as the first argument or set VVE_SPONZA_SCENE.\n";
        return 1;
    }

    vve::v3::AssetSystem asset_system{};
    const auto imported_scene = asset_system.importScene(*scene_path);
    if (!imported_scene) {
        std::cerr << "Failed to import scene: " << scene_path->string() << '\n';
        return 1;
    }

    std::cout << std::fixed << std::setprecision(6);
    printScene(*imported_scene);

    auto engine = vve::makeEngine(
        vve::ApplicationName{"sponza"},
        vve::EnableValidation{true},
        vve::makeUserSystems(SponzaLoaderSystem{*scene_path}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "sponza.main",
                    .title = "VVE Sponza",
                    .width = 1600,
                    .height = 900,
                    .resizable = true,
                    .visible = true}}});

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
