#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

import VEEngine;
import VEEngine.V3;

/**
 * @file
 * @brief Example program that imports the Sponza scene and prints its contents.
 */
namespace {

constexpr std::int32_t sdlKeyRight = 0x4000004F;
constexpr std::int32_t sdlKeyLeft = 0x40000050;
constexpr std::int32_t sdlKeyDown = 0x40000051;
constexpr std::int32_t sdlKeyUp = 0x40000052;

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
    std::optional<bool> command_line_choice{};
    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        if (argv[argument_index] == nullptr) {
            continue;
        }

        const std::string_view argument{argv[argument_index]};
        if (argument == "--load-runtime-scene") {
            command_line_choice = true;
        } else if (argument == "--list-only" || argument == "--no-runtime-scene") {
            command_line_choice = false;
        }
    }

    if (command_line_choice.has_value()) {
        return *command_line_choice;
    }

    if (const char *load_runtime_scene = std::getenv("VVE_SPONZA_LOAD_RUNTIME_SCENE");
        load_runtime_scene != nullptr) {
        const std::string_view value{load_runtime_scene};
        return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
    }

    return true;
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

struct SceneBounds {
    vve::math::Vec3 minimum{
        std::numeric_limits<vve::math::Scalar>::max(),
        std::numeric_limits<vve::math::Scalar>::max(),
        std::numeric_limits<vve::math::Scalar>::max()};
    vve::math::Vec3 maximum{
        std::numeric_limits<vve::math::Scalar>::lowest(),
        std::numeric_limits<vve::math::Scalar>::lowest(),
        std::numeric_limits<vve::math::Scalar>::lowest()};
    bool valid{false};
};

struct SponzaCameraPlan {
    vve::Camera camera{};
    vve::math::Vec3 target{vve::math::zeroVec3()};
    SceneBounds bounds{};
    vve::math::Scalar radius{vve::math::one()};
    std::string source{"bounds"};
};

void includePoint(SceneBounds &bounds, const vve::math::Vec3 &point) {
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
    bounds.valid = true;
}

[[nodiscard]] vve::math::Vec3 transformPoint(const vve::math::Mat4 &transform, const vve::math::Vec3 &point) {
    const auto x = (transform[0][0] * point.x) + (transform[1][0] * point.y) + (transform[2][0] * point.z) +
                   transform[3][0];
    const auto y = (transform[0][1] * point.x) + (transform[1][1] * point.y) + (transform[2][1] * point.z) +
                   transform[3][1];
    const auto z = (transform[0][2] * point.x) + (transform[1][2] * point.y) + (transform[2][2] * point.z) +
                   transform[3][2];
    const auto w = (transform[0][3] * point.x) + (transform[1][3] * point.y) + (transform[2][3] * point.z) +
                   transform[3][3];
    if (std::abs(w) > static_cast<vve::math::Scalar>(0.00001)) {
        return vve::math::Vec3(x / w, y / w, z / w);
    }

    return vve::math::Vec3(x, y, z);
}

[[nodiscard]] vve::math::Vec3 translationFromTransform(const vve::math::Mat4 &transform) {
    return vve::math::Vec3(transform[3][0], transform[3][1], transform[3][2]);
}

[[nodiscard]] vve::math::Scalar distanceSquared(const vve::math::Vec3 &left, const vve::math::Vec3 &right) {
    const auto x = left.x - right.x;
    const auto y = left.y - right.y;
    const auto z = left.z - right.z;
    return (x * x) + (y * y) + (z * z);
}

[[nodiscard]] vve::math::Vec3 addVec3(const vve::math::Vec3 &left, const vve::math::Vec3 &right) {
    return vve::math::Vec3(left.x + right.x, left.y + right.y, left.z + right.z);
}

[[nodiscard]] vve::math::Vec3 subtractVec3(const vve::math::Vec3 &left, const vve::math::Vec3 &right) {
    return vve::math::Vec3(left.x - right.x, left.y - right.y, left.z - right.z);
}

[[nodiscard]] vve::math::Vec3 scaleVec3(const vve::math::Vec3 &value, const vve::math::Scalar scale) {
    return vve::math::Vec3(value.x * scale, value.y * scale, value.z * scale);
}

[[nodiscard]] vve::math::Vec3 crossVec3(const vve::math::Vec3 &left, const vve::math::Vec3 &right) {
    return vve::math::Vec3(
        (left.y * right.z) - (left.z * right.y),
        (left.z * right.x) - (left.x * right.z),
        (left.x * right.y) - (left.y * right.x));
}

[[nodiscard]] vve::math::Scalar dotVec3(const vve::math::Vec3 &left, const vve::math::Vec3 &right) {
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

[[nodiscard]] vve::math::Vec3 normalizeVec3(const vve::math::Vec3 &value,
                                            const vve::math::Vec3 &fallback) {
    const auto squared_length = distanceSquared(value, vve::math::zeroVec3());
    if (squared_length <= static_cast<vve::math::Scalar>(0.000001)) {
        return fallback;
    }

    return scaleVec3(value, vve::math::one() / std::sqrt(squared_length));
}

[[nodiscard]] vve::math::Vec3 rotateVec3AroundAxis(const vve::math::Vec3 &value,
                                                   const vve::math::Vec3 &axis,
                                                   const vve::math::Scalar radians) {
    const auto normalized_axis = normalizeVec3(axis, vve::math::Vec3(vve::math::zero(), vve::math::one(),
                                                                     vve::math::zero()));
    const auto sine = std::sin(radians);
    const auto cosine = std::cos(radians);
    const auto scaled_value = scaleVec3(value, cosine);
    const auto scaled_cross = scaleVec3(crossVec3(normalized_axis, value), sine);
    const auto scaled_axis = scaleVec3(normalized_axis, dotVec3(normalized_axis, value) *
                                                            (vve::math::one() - cosine));
    return addVec3(addVec3(scaled_value, scaled_cross), scaled_axis);
}

[[nodiscard]] bool isMoveKeyDown(const vve::InputState &input, const char upper, const char lower) {
    return input.isKeyDown(upper) || input.isKeyDown(lower);
}

void includeTransformedMeshBounds(SceneBounds &bounds, const vve::v3::ImportedMesh &mesh,
                                  const vve::math::Mat4 &world_transform) {
    if (mesh.vertices.empty()) {
        return;
    }

    const auto &minimum = mesh.bounds_min;
    const auto &maximum = mesh.bounds_max;
    const std::array corners{
        vve::math::Vec3(minimum.x, minimum.y, minimum.z),
        vve::math::Vec3(maximum.x, minimum.y, minimum.z),
        vve::math::Vec3(minimum.x, maximum.y, minimum.z),
        vve::math::Vec3(maximum.x, maximum.y, minimum.z),
        vve::math::Vec3(minimum.x, minimum.y, maximum.z),
        vve::math::Vec3(maximum.x, minimum.y, maximum.z),
        vve::math::Vec3(minimum.x, maximum.y, maximum.z),
        vve::math::Vec3(maximum.x, maximum.y, maximum.z)};

    for (const auto &corner : corners) {
        includePoint(bounds, transformPoint(world_transform, corner));
    }
}

[[nodiscard]] std::optional<SceneBounds> computeSceneBounds(const vve::v3::ImportedScene &scene) {
    std::unordered_map<vve::Handle::value_type, const vve::v3::ImportedMesh *> meshes{};
    meshes.reserve(scene.meshes.size());
    for (const auto &mesh : scene.meshes) {
        meshes.emplace(mesh.handle.value.value(), &mesh);
    }

    SceneBounds bounds{};
    std::unordered_map<vve::Handle::value_type, vve::math::Mat4> world_transforms{};
    world_transforms.reserve(scene.nodes.size());
    for (const auto &node : scene.nodes) {
        auto parent_transform = vve::math::identityMat4();
        if (node.parent.value.isValid()) {
            if (const auto parent = world_transforms.find(node.parent.value.value());
                parent != world_transforms.end()) {
                parent_transform = parent->second;
            }
        }

        const auto world_transform = vve::math::multiply(parent_transform, node.local_transform);
        world_transforms.emplace(node.handle.value.value(), world_transform);
        for (const auto &mesh_instance : node.mesh_instances) {
            const auto mesh = meshes.find(mesh_instance.mesh.value.value());
            if (mesh != meshes.end() && mesh->second != nullptr) {
                includeTransformedMeshBounds(bounds, *mesh->second, world_transform);
            }
        }
    }

    if (!bounds.valid) {
        return std::nullopt;
    }

    return bounds;
}

[[nodiscard]] std::optional<vve::math::Vec3> findNodeWorldPosition(const vve::v3::ImportedScene &scene,
                                                                   const std::string_view node_name) {
    std::unordered_map<vve::Handle::value_type, vve::math::Mat4> world_transforms{};
    world_transforms.reserve(scene.nodes.size());
    for (const auto &node : scene.nodes) {
        auto parent_transform = vve::math::identityMat4();
        if (node.parent.value.isValid()) {
            if (const auto parent = world_transforms.find(node.parent.value.value());
                parent != world_transforms.end()) {
                parent_transform = parent->second;
            }
        }

        const auto world_transform = vve::math::multiply(parent_transform, node.local_transform);
        world_transforms.emplace(node.handle.value.value(), world_transform);
        if (node.name == node_name) {
            return translationFromTransform(world_transform);
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<SponzaCameraPlan> makeSponzaCameraPlan(const vve::v3::ImportedScene &scene) {
    const auto bounds = computeSceneBounds(scene);
    if (!bounds.has_value()) {
        return std::nullopt;
    }

    const auto center = vve::math::Vec3(
        (bounds->minimum.x + bounds->maximum.x) * static_cast<vve::math::Scalar>(0.5),
        (bounds->minimum.y + bounds->maximum.y) * static_cast<vve::math::Scalar>(0.5),
        (bounds->minimum.z + bounds->maximum.z) * static_cast<vve::math::Scalar>(0.5));
    const auto extent = vve::math::Vec3(bounds->maximum.x - bounds->minimum.x,
                                        bounds->maximum.y - bounds->minimum.y,
                                        bounds->maximum.z - bounds->minimum.z);
    const auto diagonal = std::sqrt((extent.x * extent.x) + (extent.y * extent.y) + (extent.z * extent.z));
    const auto radius = std::max(diagonal * static_cast<vve::math::Scalar>(0.5), vve::math::one());
    const auto camera_position = vve::math::Vec3(
        center.x,
        center.y + (radius * static_cast<vve::math::Scalar>(0.35)),
        center.z + (radius * static_cast<vve::math::Scalar>(1.8)));
    const auto near_plane = std::max(radius * static_cast<vve::math::Scalar>(0.001),
                                     static_cast<vve::math::Scalar>(0.05));
    const auto far_plane = std::max(radius * static_cast<vve::math::Scalar>(6.0),
                                    static_cast<vve::math::Scalar>(100.0));
    const auto authored_camera = findNodeWorldPosition(scene, "PhysCamera001");
    const auto authored_target = findNodeWorldPosition(scene, "PhysCamera001.Target");
    if (authored_camera.has_value() && authored_target.has_value() &&
        distanceSquared(*authored_camera, *authored_target) > static_cast<vve::math::Scalar>(0.001)) {
        return SponzaCameraPlan{
            .camera = vve::Camera::lookAt(*authored_camera, *authored_target,
                                          vve::math::Vec3(vve::math::zero(), vve::math::one(), vve::math::zero()),
                                          static_cast<vve::math::Scalar>(1.02),
                                          static_cast<vve::math::Scalar>(0.1),
                                          std::max(radius * static_cast<vve::math::Scalar>(8.0),
                                                   static_cast<vve::math::Scalar>(100.0))),
            .target = *authored_target,
            .bounds = *bounds,
            .radius = radius,
            .source = "PhysCamera001"};
    }

    return SponzaCameraPlan{
        .camera = vve::Camera::lookAt(camera_position, center,
                                      vve::math::Vec3(vve::math::zero(), vve::math::one(), vve::math::zero()),
                                      static_cast<vve::math::Scalar>(0.75), near_plane, far_plane),
        .target = center,
        .bounds = *bounds,
        .radius = radius,
        .source = "bounds"};
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

void printLights(const vve::v3::ImportedScene &scene) {
    std::cout << "Lights (" << scene.lights.size() << ")\n";
    for (std::size_t light_index = 0; light_index < scene.lights.size(); ++light_index) {
        const auto &light = scene.lights[light_index];
        std::cout << "  [" << light_index << "] handle=" << rawHandle(light.handle.value)
                  << " node=" << rawHandle(light.node.value)
                  << " type=" << vve::v3::sceneLightTypeName(light.type)
                  << " name=\"" << light.name << "\"\n";
        std::cout << "    color=";
        printVec3(light.color);
        std::cout << " intensity=" << light.intensity
                  << " range=" << light.range
                  << " inner_cone_cos=" << light.inner_cone_cos
                  << " outer_cone_cos=" << light.outer_cone_cos << '\n';
    }
}

void printCameras(const vve::v3::ImportedScene &scene) {
    std::cout << "Cameras (" << scene.cameras.size() << ")\n";
    for (std::size_t camera_index = 0; camera_index < scene.cameras.size(); ++camera_index) {
        const auto &camera = scene.cameras[camera_index];
        std::cout << "  [" << camera_index << "] handle=" << rawHandle(camera.handle.value)
                  << " node=" << rawHandle(camera.node.value)
                  << " name=\"" << camera.name << "\""
                  << " fov=" << camera.camera.vertical_fov_radians
                  << " near=" << camera.camera.near_plane
                  << " far=" << camera.camera.far_plane << '\n';
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
    std::cout << "  light_count=" << scene.lights.size() << '\n';
    std::cout << "  camera_count=" << scene.cameras.size() << '\n';
    printTextures(scene);
    printMaterials(scene);
    printMeshes(scene);
    printNodes(scene);
    printLights(scene);
    printCameras(scene);
}

void printMainObjects(const vve::v3::ImportedScene &scene) {
    std::cout << "Main objects\n";
    std::cout << "  scene=\"" << scene.name << "\" meshes=" << scene.meshes.size()
              << " materials=" << scene.materials.size()
              << " textures=" << scene.textures.size()
              << " nodes=" << scene.nodes.size()
              << " lights=" << scene.lights.size()
              << " cameras=" << scene.cameras.size() << '\n';

    std::cout << "  lights\n";
    const auto shown_light_count = std::min<std::size_t>(scene.lights.size(), 8U);
    for (std::size_t light_index = 0; light_index < shown_light_count; ++light_index) {
        const auto &light = scene.lights[light_index];
        std::cout << "    [" << light_index << "] type=" << vve::v3::sceneLightTypeName(light.type)
                  << " name=\"" << light.name << "\" node=" << rawHandle(light.node.value) << '\n';
    }
    if (scene.lights.size() > shown_light_count) {
        std::cout << "    ... " << (scene.lights.size() - shown_light_count) << " more\n";
    } else if (scene.lights.empty()) {
        std::cout << "    <none>\n";
    }

    std::cout << "  cameras\n";
    for (std::size_t camera_index = 0; camera_index < scene.cameras.size(); ++camera_index) {
        const auto &camera = scene.cameras[camera_index];
        std::cout << "    [" << camera_index << "] name=\"" << camera.name
                  << "\" node=" << rawHandle(camera.node.value) << '\n';
    }
    if (scene.cameras.empty()) {
        std::cout << "    <none>\n";
    }

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
    explicit SponzaLoaderSystem(std::shared_ptr<const vve::v3::ImportedScene> imported_scene,
                                std::optional<SponzaCameraPlan> camera_plan,
                                const bool load_runtime_scene = true)
        : imported_scene_(std::move(imported_scene)),
          scene_path_(imported_scene_ == nullptr ? std::filesystem::path{} : imported_scene_->source_path),
          camera_plan_(std::move(camera_plan)),
          load_runtime_scene_(load_runtime_scene) {}

    [[nodiscard]] std::string_view name() const noexcept { return "SponzaLoaderSystem"; }

    [[nodiscard]] std::expected<void, vve::Error> init(vve::World &world) {
        if (loaded_) {
            return {};
        }

        if (imported_scene_ == nullptr || !imported_scene_->handle.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
        }

        if (!load_runtime_scene_) {
            std::cout << '[' << name() << "] scene imported for object listing; runtime scene load disabled\n";
            std::cout << '[' << name()
                      << "] omit --list-only or set VVE_SPONZA_LOAD_RUNTIME_SCENE=1 to load it into the runtime\n";
            printWindowInventory(world);
            loaded_ = true;
            return {};
        }

        std::cout << '[' << name() << "] loading imported scene into runtime: " << scene_path_.string() << '\n';
        if (const auto load_result = world.loadImportedScene(*imported_scene_); !load_result) {
            std::cerr << '[' << name() << "] failed to load scene into runtime: " << scene_path_.string() << '\n';
            return std::unexpected(load_result.error());
        }

        if (camera_plan_.has_value()) {
            const auto camera_entity = world.spawn(vve::CameraComponent{
                .camera = camera_plan_->camera,
                .window_id = "sponza.main"});
            if (!camera_entity) {
                return std::unexpected(camera_entity.error());
            }

            if (const auto camera_result = world.setActiveCamera(*camera_entity); !camera_result) {
                return std::unexpected(camera_result.error());
            }

            std::cout << '[' << name() << "] camera source=" << camera_plan_->source
                      << " radius=" << camera_plan_->radius
                      << " near=" << camera_plan_->camera.near_plane
                      << " far=" << camera_plan_->camera.far_plane << '\n';
            camera_entity_ = *camera_entity;
            camera_position_ = camera_plan_->camera.position;
            camera_target_ = camera_plan_->target;
            camera_fov_ = camera_plan_->camera.vertical_fov_radians;
            camera_near_ = camera_plan_->camera.near_plane;
            camera_far_ = camera_plan_->camera.far_plane;
            movement_speed_ = std::max(camera_plan_->radius * static_cast<vve::math::Scalar>(0.35),
                                       static_cast<vve::math::Scalar>(4.0));
            rotation_speed_ = static_cast<vve::math::Scalar>(1.5);
        } else {
            std::cout << '[' << name() << "] no scene bounds available; using the engine default camera\n";
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
        (void)window_frame;
        if (!frame_loop_logged_ && frame_context.frame_index > 0) {
            std::cout << '[' << name() << "] frame loop active; scene resources requested without per-frame limits\n";
            frame_loop_logged_ = true;
        }

        if (const auto camera_result = updateCameraFromInput(world, frame_context); !camera_result) {
            return std::unexpected(camera_result.error());
        }

        return {};
    }

private:
    [[nodiscard]] std::expected<void, vve::Error> updateCameraFromInput(
        vve::World &world,
        const vve::v3::FrameContext &frame_context) {
        if (!load_runtime_scene_ || !camera_entity_.isValid()) {
            return {};
        }

        const auto &input = world.input();
        const bool forward_pressed = isMoveKeyDown(input, 'W', 'w');
        const bool backward_pressed = isMoveKeyDown(input, 'S', 's');
        const bool left_pressed = isMoveKeyDown(input, 'A', 'a');
        const bool right_pressed = isMoveKeyDown(input, 'D', 'd');
        const bool look_up_pressed = input.isKeyDown(sdlKeyUp);
        const bool look_down_pressed = input.isKeyDown(sdlKeyDown);
        const bool look_left_pressed = input.isKeyDown(sdlKeyLeft);
        const bool look_right_pressed = input.isKeyDown(sdlKeyRight);

        std::string key_state{};
        key_state.reserve(8);
        key_state.push_back(forward_pressed ? 'W' : '-');
        key_state.push_back(backward_pressed ? 'S' : '-');
        key_state.push_back(left_pressed ? 'A' : '-');
        key_state.push_back(right_pressed ? 'D' : '-');
        key_state.push_back(look_up_pressed ? '^' : '-');
        key_state.push_back(look_down_pressed ? 'v' : '-');
        key_state.push_back(look_left_pressed ? '<' : '-');
        key_state.push_back(look_right_pressed ? '>' : '-');

        const auto forward_axis = (forward_pressed ? 1 : 0) - (backward_pressed ? 1 : 0);
        const auto right_axis = (right_pressed ? 1 : 0) - (left_pressed ? 1 : 0);
        const auto pitch_axis = (look_up_pressed ? 1 : 0) - (look_down_pressed ? 1 : 0);
        const auto yaw_axis = (look_left_pressed ? 1 : 0) - (look_right_pressed ? 1 : 0);
        const bool movement_key_held = forward_axis != 0 || right_axis != 0;
        const bool rotation_key_held = pitch_axis != 0 || yaw_axis != 0;

        if (movement_key_held || rotation_key_held) {
            const auto up = vve::math::Vec3(vve::math::zero(), vve::math::one(), vve::math::zero());
            const auto look_vector = subtractVec3(camera_target_, camera_position_);
            const auto look_distance = std::max(std::sqrt(distanceSquared(camera_position_, camera_target_)),
                                                static_cast<vve::math::Scalar>(0.001));
            const auto seconds = static_cast<vve::math::Scalar>(
                std::clamp(frame_context.delta_seconds, 0.0, 0.1));
            auto forward = normalizeVec3(look_vector,
                                         vve::math::Vec3(vve::math::zero(), vve::math::zero(), -vve::math::one()));
            auto right = normalizeVec3(crossVec3(forward, up),
                                       vve::math::Vec3(vve::math::one(), vve::math::zero(), vve::math::zero()));

            if (movement_key_held) {
                const auto forward_offset = scaleVec3(forward, static_cast<vve::math::Scalar>(forward_axis) *
                                                                   movement_speed_ * seconds);
                const auto right_offset = scaleVec3(right, static_cast<vve::math::Scalar>(right_axis) *
                                                             movement_speed_ * seconds);
                const auto offset = addVec3(forward_offset, right_offset);
                camera_position_ = addVec3(camera_position_, offset);
                camera_target_ = addVec3(camera_target_, offset);
            }

            if (rotation_key_held) {
                if (yaw_axis != 0) {
                    const auto yaw_radians = static_cast<vve::math::Scalar>(yaw_axis) * rotation_speed_ * seconds;
                    forward = normalizeVec3(rotateVec3AroundAxis(forward, up, yaw_radians), forward);
                }

                right = normalizeVec3(crossVec3(forward, up),
                                      vve::math::Vec3(vve::math::one(), vve::math::zero(), vve::math::zero()));

                if (pitch_axis != 0) {
                    const auto pitch_radians = static_cast<vve::math::Scalar>(pitch_axis) *
                                               rotation_speed_ * seconds;
                    const auto pitched_forward = normalizeVec3(rotateVec3AroundAxis(forward, right, pitch_radians),
                                                               forward);
                    if (std::abs(dotVec3(pitched_forward, up)) < static_cast<vve::math::Scalar>(0.98)) {
                        forward = pitched_forward;
                    }
                }

                camera_target_ = addVec3(camera_position_, scaleVec3(forward, look_distance));
            }

            const auto camera = vve::Camera::lookAt(camera_position_, camera_target_, up,
                                                    camera_fov_, camera_near_, camera_far_);
            const auto set_component = world.setComponent(
                camera_entity_,
                vve::CameraComponent{.camera = camera, .window_id = "sponza.main"});
            if (!set_component) {
                return std::unexpected(set_component.error());
            }

            if (const auto set_active = world.setActiveCamera(camera_entity_); !set_active) {
                return std::unexpected(set_active.error());
            }
        }

        camera_log_accumulator_seconds_ += frame_context.delta_seconds;
        const bool periodic_log = camera_log_accumulator_seconds_ >= 1.0;
        const bool key_state_changed = key_state != last_logged_camera_key_state_;
        if (periodic_log) {
            camera_log_accumulator_seconds_ = 0.0;
        }

        if (movement_key_held || rotation_key_held || key_state_changed || periodic_log) {
            std::cout << '[' << name() << "] camera keys=" << key_state
                      << " move_speed=" << movement_speed_
                      << " turn_speed=" << rotation_speed_ << '\n';
            last_logged_camera_key_state_ = key_state;
        }

        return {};
    }

    void printWindowInventory(vve::World &world) {
        std::cout << '[' << name() << "] windows:";
        bool printed_any = false;
        for (const auto &window : world.windows()) {
            printed_any = true;
            std::cout << ' ' << window.id << '=' << window.width << 'x' << window.height
                      << '[' << window.renderer_id << ']';
        }
        if (!printed_any) {
            std::cout << " <none>";
        }
        std::cout << '\n';
    }

    std::shared_ptr<const vve::v3::ImportedScene> imported_scene_{};
    std::filesystem::path scene_path_{};
    std::optional<SponzaCameraPlan> camera_plan_{};
    vve::Handle camera_entity_{};
    vve::math::Vec3 camera_position_{vve::math::zeroVec3()};
    vve::math::Vec3 camera_target_{vve::math::zeroVec3()};
    vve::math::Scalar camera_fov_{static_cast<vve::math::Scalar>(1.0471975511965976)};
    vve::math::Scalar camera_near_{static_cast<vve::math::Scalar>(0.1)};
    vve::math::Scalar camera_far_{static_cast<vve::math::Scalar>(10000.0)};
    vve::math::Scalar movement_speed_{static_cast<vve::math::Scalar>(4.0)};
    vve::math::Scalar rotation_speed_{static_cast<vve::math::Scalar>(1.5)};
    bool load_runtime_scene_{true};
    bool loaded_{false};
    bool frame_loop_logged_{false};
    double camera_log_accumulator_seconds_{0.0};
    std::string last_logged_camera_key_state_{"--------"};
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
    std::shared_ptr<const vve::v3::ImportedScene> imported_scene{};
    std::optional<SponzaCameraPlan> camera_plan{};
    if (!scene_path.has_value()) {
        std::cerr << "[sponza] Unable to locate the Sponza scene.\n";
        std::cerr << "[sponza] Pass the scene file path as the first argument or set VVE_SPONZA_SCENE.\n";
        return 1;
    } else {
        std::cout << "[sponza] using scene: " << scene_path->string() << '\n';

        vve::v3::AssetSystem asset_system{};
        auto imported_scene_result = asset_system.importScene(*scene_path);
        if (!imported_scene_result) {
            std::cerr << "[sponza] Failed to import scene: " << scene_path->string() << '\n';
            return 1;
        }
        imported_scene = std::make_shared<vve::v3::ImportedScene>(std::move(*imported_scene_result));

        std::cout << std::fixed << std::setprecision(6);
        camera_plan = makeSponzaCameraPlan(*imported_scene);
        if (camera_plan.has_value()) {
            std::cout << "[sponza] bounds min=";
            printVec3(camera_plan->bounds.minimum);
            std::cout << " max=";
            printVec3(camera_plan->bounds.maximum);
            std::cout << " radius=" << camera_plan->radius << '\n';
        } else {
            std::cout << "[sponza] unable to compute scene bounds; camera will use engine defaults\n";
        }
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
        vve::makeUserSystems(SponzaLoaderSystem{imported_scene, camera_plan, load_runtime_scene}),
        vve::Windows{
            .value = {
                vve::WindowDesc{
                    .id = "sponza.main",
                    .title = "VVE Sponza",
                    .width = 960,
                    .height = 540,
                    .renderer_id = "forward",
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
