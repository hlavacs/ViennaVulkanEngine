#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>

import VEEngine;
import VEEngine.V4;

/**
 * @file
 * @brief Deterministic light/shadow verification scene for the future v4 forward renderer.
 */
namespace {

struct DebugPlane {
    vve::Vec3 center{};     ///< Plane center in world space.
    vve::Vec2 half_extent{}; ///< Half size along local X/Z axes.
};

struct DebugCuboid {
    vve::Vec3 minimum{}; ///< World-space AABB minimum.
    vve::Vec3 maximum{}; ///< World-space AABB maximum.
};

struct DebugDirectionalLight {
    vve::Direction direction_to_light{}; ///< Direction from shaded point toward the light.
    vve::LinearColor color{};            ///< Linear RGB light color.
    vve::LightIntensity intensity{};     ///< Direct-light intensity.
    vve::LinearColor ambient{};          ///< Ambient term.
};

struct DebugSamplePoint {
    std::string_view name{}; ///< Stable sample name used by verification scripts.
    vve::Vec3 position{};    ///< World-space sample position.
    vve::Vec3 normal{};      ///< World-space sample normal.
};

struct DebugSampleResult {
    DebugSamplePoint sample{};   ///< Input sample definition.
    float n_dot_l{};             ///< Lambert cosine term.
    float shadow_factor{};       ///< 0 means shadowed, 1 means lit.
    vve::Vec3 final_lighting{};  ///< Ambient plus direct lighting.
    vve::Vec3 final_color{};     ///< White material lit by final_lighting.
};

[[nodiscard]] float dot(vve::Vec3 lhs, vve::Vec3 rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] vve::Vec3 add(vve::Vec3 lhs, vve::Vec3 rhs) {
    return vve::Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] vve::Vec3 scale(vve::Vec3 value, float factor) {
    return vve::Vec3{value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] float length(vve::Vec3 value) {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] vve::Vec3 normalize(vve::Vec3 value) {
    const auto vector_length = length(value);
    return vector_length <= 1.0e-6F ? vve::Vec3{} : scale(value, 1.0F / vector_length);
}

[[nodiscard]] bool intersectsCuboid(vve::Vec3 origin, vve::Vec3 direction, const DebugCuboid &cuboid) {
    constexpr auto epsilon = 1.0e-4F;
    auto near_t = 0.0F;
    auto far_t = 10000.0F;

    const auto update_axis = [&](float ray_origin, float ray_direction, float minimum, float maximum) {
        if (std::abs(ray_direction) <= epsilon) { return ray_origin >= minimum && ray_origin <= maximum; }

        auto first = (minimum - ray_origin) / ray_direction;
        auto second = (maximum - ray_origin) / ray_direction;
        if (first > second) { std::swap(first, second); }
        near_t = std::max(near_t, first);
        far_t = std::min(far_t, second);
        return near_t <= far_t;
    };

    if (!update_axis(origin.x, direction.x, cuboid.minimum.x, cuboid.maximum.x)) { return false; }
    if (!update_axis(origin.y, direction.y, cuboid.minimum.y, cuboid.maximum.y)) { return false; }
    if (!update_axis(origin.z, direction.z, cuboid.minimum.z, cuboid.maximum.z)) { return false; }
    return far_t > epsilon;
}

[[nodiscard]] DebugSampleResult evaluateSample(const DebugSamplePoint &sample,
                                               const DebugDirectionalLight &light,
                                               const DebugCuboid &cuboid) {
    const auto normal = normalize(sample.normal);
    const auto direction_to_light = normalize(light.direction_to_light.value);
    const auto n_dot_l = std::max(0.0F, dot(normal, direction_to_light));
    const auto ray_origin = add(sample.position, scale(normal, 0.002F));
    const auto shadowed = intersectsCuboid(ray_origin, direction_to_light, cuboid);
    const auto shadow_factor = shadowed ? 0.0F : 1.0F;
    const auto direct = scale(light.color.value, light.intensity.value * n_dot_l * shadow_factor);
    const auto final_lighting = add(light.ambient.value, direct);

    return DebugSampleResult{.sample = sample,
                             .n_dot_l = n_dot_l,
                             .shadow_factor = shadow_factor,
                             .final_lighting = final_lighting,
                             .final_color = final_lighting};
}

void writeVec3(std::ofstream &file, std::string_view name, vve::Vec3 value) {
    file << name << '=' << value.x << ',' << value.y << ',' << value.z << '\n';
}

[[nodiscard]] std::filesystem::path outputPath(int argc, char **argv) {
    for (auto index = 1; index + 1 < argc; ++index) {
        if (std::string_view{argv[index]} == "--output") { return std::filesystem::path{argv[index + 1]}; }
    }
    return std::filesystem::path{"bin/debug/verify/light_shadow_reference.txt"};
}

class LightShadowDebugSystem final {
public:
    explicit LightShadowDebugSystem(std::filesystem::path output_path) : output_path_{std::move(output_path)} {}

    [[nodiscard]] std::string_view name() const noexcept { return "LightShadowDebugSystem"; }

    template <typename TWorld> [[nodiscard]] std::expected<void, vve::Error> init(TWorld &world) {
        auto ecs = world.template get<vve::ECS>();
        const auto plane_entity = ecs.create();
        const auto cuboid_entity = ecs.create();
        const auto light_entity = ecs.create();
        const auto camera_entity = ecs.create();
        if (const auto result = ecs.add(plane_entity, vve::Transform{}); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(plane_entity, plane_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(cuboid_entity, vve::Transform{}); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(cuboid_entity, cuboid_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(light_entity, vve::Transform{}); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(light_entity, light_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(camera_entity, camera_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto camera_result = world.template get<vve::WindowSystem>().setActiveCamera(camera_entity);
            !camera_result) {
            return std::unexpected(camera_result.error());
        }

        auto render_scene = buildRenderScene();
        if (!render_scene) { return std::unexpected(render_scene.error()); }
        render_scene_ = std::move(*render_scene);

        const auto output_directory = output_path_.parent_path();
        if (!output_directory.empty()) { std::filesystem::create_directories(output_directory); }

        std::ofstream file{output_path_};
        if (!file) { return std::unexpected(vve::Error::invalid_argument); }
        file << std::fixed << std::setprecision(6);
        file << "vve_light_shadow_debug_scene=1\n";
        file << "coordinate_system=right_handed_y_up\n";
        file << "plane_entity=" << plane_entity.value() << '\n';
        file << "cuboid_entity=" << cuboid_entity.value() << '\n';
        file << "light_entity=" << light_entity.value() << '\n';
        file << "camera_entity=" << camera_entity.value() << '\n';
        writeVec3(file, "plane.center", plane_.center);
        file << "plane.half_extent=" << plane_.half_extent.x << ',' << plane_.half_extent.y << '\n';
        writeVec3(file, "cuboid.minimum", cuboid_.minimum);
        writeVec3(file, "cuboid.maximum", cuboid_.maximum);
        writeVec3(file, "light.direction_to_light", normalize(light_.direction_to_light.value));
        writeVec3(file, "light.color", light_.color.value);
        file << "light.intensity=" << light_.intensity.value << '\n';
        writeVec3(file, "light.ambient", light_.ambient.value);
        writeVec3(file, "camera.position", camera_.position.value);
        writeVec3(file, "camera.target", camera_target_);
        file << "camera.fov_y=" << camera_.fov_y.radians << '\n';
        file << "camera.clip=" << camera_.clip.near_plane << ',' << camera_.clip.far_plane << '\n';
        file << "render_scene.mesh_count=" << render_scene_.meshCount() << '\n';
        file << "render_scene.material_count=" << render_scene_.materialCount() << '\n';
        file << "render_scene.instance_count=" << render_scene_.instanceCount() << '\n';
        file << "render_scene.camera=" << render_scene_.camera().has_value() << '\n';
        file << "render_scene.directional_light=" << render_scene_.directionalLight().has_value() << '\n';
        writeRenderSceneMeshes(file);

        for (const auto &sample : samples_) {
            const auto result = evaluateSample(sample, light_, cuboid_);
            file << "sample." << result.sample.name << ".n_dot_l=" << result.n_dot_l << '\n';
            file << "sample." << result.sample.name << ".shadow_factor=" << result.shadow_factor << '\n';
            writeVec3(file, "sample." + std::string{result.sample.name} + ".position", result.sample.position);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".normal", result.sample.normal);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".final_lighting", result.final_lighting);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".final_color", result.final_color);
        }

        std::cout << '[' << name() << "] wrote " << output_path_.string() << '\n';
        return {};
    }

private:
    /// @brief Builds the CPU render scene that the future Vulkan pass will upload.
    [[nodiscard]] std::expected<vve::v4::RenderScene, vve::Error> buildRenderScene() const {
        auto scene = vve::v4::RenderScene{};
        const auto plane_material = scene.addMaterial(vve::v4::RenderMaterial{
            .base_color = vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}}});
        const auto cuboid_material = scene.addMaterial(vve::v4::RenderMaterial{
            .base_color = vve::LinearColor{.value = vve::Vec3{0.80F, 0.72F, 0.62F}}});
        const auto plane_mesh = scene.addPlaneMesh(plane_.half_extent);
        const auto cuboid_mesh = scene.addCuboidMesh(cuboid_.minimum, cuboid_.maximum);

        const auto plane = scene.addInstance(plane_mesh, plane_material,
                                             vve::Transform{.translation = vve::Position{.value = plane_.center}});
        if (!plane) { return std::unexpected(plane.error()); }
        const auto cuboid = scene.addInstance(cuboid_mesh, cuboid_material);
        if (!cuboid) { return std::unexpected(cuboid.error()); }

        scene.setCamera(vve::v4::RenderCamera{.camera = camera_, .target_extent = render_extent_});
        scene.setDirectionalLight(vve::v4::RenderDirectionalLight{.direction_to_light = light_.direction_to_light,
                                                                  .color = light_.color,
                                                                  .intensity = light_.intensity,
                                                                  .ambient = light_.ambient});
        return scene;
    }

    /// @brief Writes renderable mesh sizes into the verification text file.
    void writeRenderSceneMeshes(std::ofstream &file) const {
        std::size_t index{};
        for (const auto &instance : render_scene_.instances()) {
            const auto *mesh = render_scene_.findMesh(instance.mesh);
            if (mesh == nullptr) { continue; }
            file << "render_scene.instance." << index << ".vertices=" << mesh->vertices.size() << '\n';
            file << "render_scene.instance." << index << ".indices=" << mesh->indices.size() << '\n';
            ++index;
        }
    }

    DebugPlane plane_{.center = vve::Vec3{0.0F, 0.0F, 0.0F}, .half_extent = vve::Vec2{3.0F, 3.0F}};
    DebugCuboid cuboid_{.minimum = vve::Vec3{-0.225F, 0.0F, -0.225F},
                        .maximum = vve::Vec3{0.225F, 2.0F, 0.225F}};
    DebugDirectionalLight light_{.direction_to_light = vve::Direction{.value = vve::Vec3{-0.55F, 0.85F, 0.35F}},
                                 .color = vve::LinearColor{.value = vve::Vec3{1.0F, 0.94F, 0.84F}},
                                 .intensity = vve::LightIntensity{.value = 1.25F},
                                 .ambient = vve::LinearColor{.value = vve::Vec3{0.08F, 0.09F, 0.10F}}};
    vve::Vec3 camera_target_{0.0F, 0.75F, 0.0F};
    vve::Camera camera_{vve::Camera::lookAt(vve::Position{.value = vve::Vec3{-3.0F, 1.8F, 3.2F}},
                                            vve::Position{.value = camera_target_},
                                            vve::Direction{.value = vve::Vec3{0.0F, 1.0F, 0.0F}},
                                            vve::FovY{.radians = 0.82F},
                                            vve::ClipPlanes{.near_plane = 0.05F, .far_plane = 30.0F})};
    std::array<DebugSamplePoint, 3> samples_{
        DebugSamplePoint{.name = "plane_lit",
                         .position = vve::Vec3{-1.70F, 0.0F, 1.30F},
                         .normal = vve::Vec3{0.0F, 1.0F, 0.0F}},
        DebugSamplePoint{.name = "plane_shadow",
                         .position = vve::Vec3{0.75F, 0.0F, -0.45F},
                         .normal = vve::Vec3{0.0F, 1.0F, 0.0F}},
        DebugSamplePoint{.name = "cuboid_lit_side",
                         .position = vve::Vec3{-0.225F, 1.0F, 0.0F},
                         .normal = vve::Vec3{-1.0F, 0.0F, 0.0F}}};
    vve::PixelExtent render_extent_{.width = 960, .height = 540}; ///< CPU render target size.
    vve::v4::RenderScene render_scene_{};                         ///< CPU scene prepared for GPU upload.
    std::filesystem::path output_path_{};
};

} // namespace

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    auto engine = vve::makeEngine(
        vve::ApplicationName{"light-shadow-debug"},
        vve::MaxFrames{.value = vve::FrameCount{.value = 1}},
        vve::makeUserSystems(LightShadowDebugSystem{outputPath(argc, argv)}),
        vve::WindowSetups{vve::WindowSetup{}
                              .id("light-shadow-debug.main")
                              .title("VVE Light Shadow Debug")
                              .extent(vve::PixelExtent{.width = 960, .height = 540})
                              .renderer(vve::RendererId{.value = "forward"})
                              .resizable(false)
                              .visible(false)});

    if (const auto init_result = engine.init(); !init_result) {
        std::cerr << "[light_shadow_debug] engine.init failed: " << vve::errorName(init_result.error()) << '\n';
        return 1;
    }
    return 0;
}
