#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

import VEEngine;

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

struct DebugSpotLight {
    vve::Position position{};         ///< World-space light position.
    vve::Direction direction{};       ///< Direction from light toward the scene.
    vve::LinearColor color{};         ///< Linear RGB light color.
    vve::LightIntensity intensity{};  ///< Direct-light intensity.
    vve::LightRange range{};          ///< Influence range.
    vve::SpotConeAngle cone{};        ///< Outer cone angle.
};

struct DebugPointLight {
    vve::Position position{};        ///< World-space light position.
    vve::LinearColor color{};        ///< Linear RGB light color.
    vve::LightIntensity intensity{}; ///< Direct-light intensity.
    vve::LightRange range{};         ///< Influence range.
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
    vve::Vec3 point_lighting{};  ///< Point-light contribution.
    vve::Vec3 spot_lighting{};   ///< Spot-light contribution.
    vve::Vec3 final_lighting{};  ///< Ambient plus direct lighting.
    vve::Vec3 final_color{};     ///< White material lit by final_lighting.
};

[[nodiscard]] float dot(vve::Vec3 lhs, vve::Vec3 rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

[[nodiscard]] vve::Vec3 add(vve::Vec3 lhs, vve::Vec3 rhs) {
    return vve::Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] vve::Vec3 subtract(vve::Vec3 lhs, vve::Vec3 rhs) {
    return vve::Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
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

[[nodiscard]] vve::Vec3 spotLighting(vve::Vec3 position, vve::Vec3 normal, const DebugSpotLight &light) {
    const auto offset = subtract(light.position.value, position);
    const auto distance = length(offset);
    if (distance <= 1.0e-6F || light.range.value <= 1.0e-6F) { return vve::Vec3{}; }
    const auto direction_to_light = scale(offset, 1.0F / distance);
    const auto from_light = scale(direction_to_light, -1.0F);
    const auto cone = std::cos(std::max(0.01F, light.cone.radians));
    const auto spot = std::clamp((dot(from_light, normalize(light.direction.value)) - cone) /
                                 std::max(1.0e-6F, 1.0F - cone), 0.0F, 1.0F);
    const auto n_dot_l = std::max(0.0F, dot(normal, direction_to_light));
    const auto distance_factor = std::max(0.0F, 1.0F - (distance / light.range.value));
    return scale(light.color.value, light.intensity.value * n_dot_l * distance_factor * distance_factor * spot);
}

[[nodiscard]] vve::Vec3 pointLighting(vve::Vec3 position, vve::Vec3 normal, const DebugPointLight &light) {
    const auto offset = subtract(light.position.value, position);
    const auto distance = length(offset);
    if (distance <= 1.0e-6F || light.range.value <= 1.0e-6F) { return vve::Vec3{}; }
    const auto direction_to_light = scale(offset, 1.0F / distance);
    const auto n_dot_l = std::max(0.0F, dot(normal, direction_to_light));
    const auto distance_factor = std::max(0.0F, 1.0F - (distance / light.range.value));
    return scale(light.color.value, light.intensity.value * n_dot_l * distance_factor * distance_factor);
}

[[nodiscard]] DebugSampleResult evaluateSample(const DebugSamplePoint &sample,
                                               const DebugDirectionalLight &light,
                                               const DebugPointLight &point_light,
                                               const DebugSpotLight &spot_light,
                                               const DebugCuboid &cuboid) {
    const auto normal = normalize(sample.normal);
    const auto direction_to_light = normalize(light.direction_to_light.value);
    const auto n_dot_l = std::max(0.0F, dot(normal, direction_to_light));
    const auto ray_origin = add(sample.position, scale(normal, 0.002F));
    const auto shadowed = intersectsCuboid(ray_origin, direction_to_light, cuboid);
    const auto shadow_factor = shadowed ? 0.0F : 1.0F;
    const auto direct = scale(light.color.value, light.intensity.value * n_dot_l * shadow_factor);
    const auto point = pointLighting(sample.position, normal, point_light);
    const auto spot = spotLighting(sample.position, normal, spot_light);
    const auto final_lighting = add(add(add(light.ambient.value, direct), point), spot);

    return DebugSampleResult{.sample = sample,
                             .n_dot_l = n_dot_l,
                             .shadow_factor = shadow_factor,
                             .point_lighting = point,
                             .spot_lighting = spot,
                             .final_lighting = final_lighting,
                             .final_color = final_lighting};
}

void writeVec3(std::ofstream &file, std::string_view name, vve::Vec3 value) {
    file << name << '=' << value.x << ',' << value.y << ',' << value.z << '\n';
}

void writeVec4(std::ofstream &file, std::string_view name, vve::Vec4 value) {
    file << name << '=' << value.x << ',' << value.y << ',' << value.z << ',' << value.w << '\n';
}

[[nodiscard]] vve::Mat4 cameraClipFromWorld(const vve::Camera &camera, vve::PixelExtent extent) {
    const auto height = static_cast<vve::Scalar>(extent.height == 0 ? 1 : extent.height);
    const auto aspect = static_cast<vve::Scalar>(extent.width) / height;
    const auto projection = vve::math::perspectiveVulkan(camera.fov_y.radians, aspect,
                                                         camera.clip.near_plane, camera.clip.far_plane);
    return vve::math::multiply(projection, camera.view_transform);
}

[[nodiscard]] vve::Vec4 clipPosition(const vve::Mat4 &clip_from_world, vve::Vec3 position) {
    return vve::math::multiply(clip_from_world, vve::Vec4{position.x, position.y, position.z, 1.0F});
}

[[nodiscard]] vve::Vec3 ndcPosition(vve::Vec4 clip) {
    const auto inverse_w = std::abs(clip.w) > 1.0e-6F ? 1.0F / clip.w : 0.0F;
    return vve::Vec3{clip.x * inverse_w, clip.y * inverse_w, clip.z * inverse_w};
}

void writeCameraPoint(std::ofstream &file, std::string_view name,
                      const vve::Mat4 &clip_from_world, vve::Vec3 position) {
    const auto prefix = std::string{name};
    const auto clip = clipPosition(clip_from_world, position);
    const auto ndc = ndcPosition(clip);
    writeVec3(file, prefix + ".world", position);
    writeVec4(file, prefix + ".clip", clip);
    writeVec3(file, prefix + ".ndc", ndc);
    file << prefix << ".depth=" << ndc.z << '\n';
    file << prefix << ".inside_clip=" << (std::abs(ndc.x) <= 1.0F && std::abs(ndc.y) <= 1.0F &&
                                          ndc.z >= 0.0F && ndc.z <= 1.0F) << '\n';
}

void writeRenderDebugSample(std::ofstream &file, std::string_view name, const vve::RenderDebugSample &sample) {
    const auto prefix = std::string{name};
    file << prefix << ".vertex_id=" << sample.vertex_id << '\n';
    writeVec3(file, prefix + ".world", sample.world);
    writeVec4(file, prefix + ".clip", sample.clip);
    writeVec4(file, prefix + ".light_clip", sample.light_clip);
    writeVec4(file, prefix + ".spot_light_clip", sample.spot_light_clip);
    writeVec4(file, prefix + ".point_light_clip", sample.point_light_clip);
    writeVec3(file, prefix + ".ndc", sample.ndc);
    writeVec3(file, prefix + ".light_ndc", sample.light_ndc);
    writeVec3(file, prefix + ".spot_light_ndc", sample.spot_light_ndc);
    writeVec3(file, prefix + ".point_light_ndc", sample.point_light_ndc);
    writeVec3(file, prefix + ".normal", sample.normal);
    writeVec3(file, prefix + ".direction_to_light", sample.direction_to_light);
    writeVec3(file, prefix + ".ambient_lighting", sample.ambient_lighting);
    writeVec3(file, prefix + ".direct_lighting", sample.direct_lighting);
    writeVec3(file, prefix + ".point_lighting", sample.point_lighting);
    writeVec3(file, prefix + ".spot_lighting", sample.spot_lighting);
    writeVec3(file, prefix + ".final_lighting", sample.final_lighting);
    file << prefix << ".depth=" << sample.depth << '\n';
    file << prefix << ".light_depth=" << sample.light_depth << '\n';
    file << prefix << ".spot_light_depth=" << sample.spot_light_depth << '\n';
    file << prefix << ".point_light_depth=" << sample.point_light_depth << '\n';
    file << prefix << ".sampled_shadow_depth=" << sample.sampled_shadow_depth << '\n';
    file << prefix << ".shadow_depth_delta=" << sample.shadow_depth_delta << '\n';
    file << prefix << ".shadow_bias=" << sample.shadow_bias << '\n';
    file << prefix << ".shadow_factor=" << sample.shadow_factor << '\n';
    file << prefix << ".sampled_spot_shadow_depth=" << sample.sampled_spot_shadow_depth << '\n';
    file << prefix << ".spot_shadow_depth_delta=" << sample.spot_shadow_depth_delta << '\n';
    file << prefix << ".spot_shadow_bias=" << sample.spot_shadow_bias << '\n';
    file << prefix << ".spot_shadow_factor=" << sample.spot_shadow_factor << '\n';
    file << prefix << ".sampled_point_shadow_depth=" << sample.sampled_point_shadow_depth << '\n';
    file << prefix << ".point_shadow_depth_delta=" << sample.point_shadow_depth_delta << '\n';
    file << prefix << ".point_shadow_bias=" << sample.point_shadow_bias << '\n';
    file << prefix << ".point_shadow_factor=" << sample.point_shadow_factor << '\n';
    file << prefix << ".point_shadow_face=" << sample.point_shadow_face << '\n';
    file << prefix << ".n_dot_l=" << sample.n_dot_l << '\n';
    file << prefix << ".inside_light=" << sample.inside_light << '\n';
    file << prefix << ".inside_spot_light=" << sample.inside_spot_light << '\n';
    file << prefix << ".inside_point_light=" << sample.inside_point_light << '\n';
    file << prefix << ".valid=" << sample.valid << '\n';
}

void writeShadowDepthSample(std::ofstream &file, std::string_view name,
                            const vve::RenderShadowDepthSample &sample) {
    const auto prefix = std::string{name};
    file << prefix << ".triangle_id=" << sample.triangle_id << '\n';
    file << prefix << ".face_index=" << sample.face_index << '\n';
    writeVec3(file, prefix + ".world", sample.world);
    writeVec3(file, prefix + ".light_ndc", sample.light_ndc);
    file << prefix << ".pixel=" << sample.pixel_x << ',' << sample.pixel_y << '\n';
    file << prefix << ".expected_depth=" << sample.expected_depth << '\n';
    file << prefix << ".gpu_depth=" << sample.gpu_depth << '\n';
    file << prefix << ".error=" << sample.error << '\n';
    file << prefix << ".has_gpu=" << sample.has_gpu << '\n';
    file << prefix << ".valid=" << sample.valid << '\n';
}

[[nodiscard]] std::filesystem::path outputPath(int argc, char **argv) {
    for (auto index = 1; index + 1 < argc; ++index) {
        if (std::string_view{argv[index]} == "--output") { return std::filesystem::path{argv[index + 1]}; }
    }
    return std::filesystem::path{"bin/debug/verify/light_shadow_reference.txt"};
}

[[nodiscard]] bool inspectMode(int argc, char **argv) {
    return std::any_of(argv + 1, argv + argc, [](const char *argument) {
        return std::string_view{argument} == "--inspect";
    });
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
        const auto spot_light_entity = ecs.create();
        const auto point_light_entity = ecs.create();
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
        if (const auto result = ecs.add(spot_light_entity, vve::Transform{.translation = spot_light_.position});
            !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(spot_light_entity, spot_light_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(point_light_entity, vve::Transform{.translation = point_light_.position});
            !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(point_light_entity, point_light_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto result = ecs.add(camera_entity, camera_); !result) {
            return std::unexpected(result.error());
        }
        if (const auto camera_result = world.template get<vve::WindowSystem>().setActiveCamera(camera_entity);
            !camera_result) {
            return std::unexpected(camera_result.error());
        }

        auto &render_system = world.template get<vve::RenderSystem>();
        if (auto result = buildRenderScene(render_system); !result) { return std::unexpected(result.error()); }

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
        file << "spot_light_entity=" << spot_light_entity.value() << '\n';
        file << "point_light_entity=" << point_light_entity.value() << '\n';
        file << "camera_entity=" << camera_entity.value() << '\n';
        writeVec3(file, "plane.center", plane_.center);
        file << "plane.half_extent=" << plane_.half_extent.x << ',' << plane_.half_extent.y << '\n';
        writeVec3(file, "cuboid.minimum", cuboid_.minimum);
        writeVec3(file, "cuboid.maximum", cuboid_.maximum);
        writeVec3(file, "light.direction_to_light", normalize(light_.direction_to_light.value));
        writeVec3(file, "light.color", light_.color.value);
        file << "light.intensity=" << light_.intensity.value << '\n';
        writeVec3(file, "light.ambient", light_.ambient.value);
        writeVec3(file, "spot_light.position", spot_light_.position.value);
        writeVec3(file, "spot_light.direction", normalize(spot_light_.direction.value));
        writeVec3(file, "spot_light.color", spot_light_.color.value);
        file << "spot_light.intensity=" << spot_light_.intensity.value << '\n';
        file << "spot_light.range=" << spot_light_.range.value << '\n';
        file << "spot_light.cone=" << spot_light_.cone.radians << '\n';
        writeVec3(file, "point_light.position", point_light_.position.value);
        writeVec3(file, "point_light.color", point_light_.color.value);
        file << "point_light.intensity=" << point_light_.intensity.value << '\n';
        file << "point_light.range=" << point_light_.range.value << '\n';
        writeVec3(file, "camera.position", camera_.position.value);
        writeVec3(file, "camera.target", camera_target_);
        file << "camera.fov_y=" << camera_.fov_y.radians << '\n';
        file << "camera.clip=" << camera_.clip.near_plane << ',' << camera_.clip.far_plane << '\n';
        file << "camera.depth_range=0,1\n";
        file << "camera.depth_test=less\n";
        const auto clip_from_world = cameraClipFromWorld(camera_, render_extent_);
        const auto plane_vertex = add(plane_.center, vve::Vec3{-plane_.half_extent.x, 0.0F, -plane_.half_extent.y});
        const auto cuboid_vertex = cuboid_.maximum;
        writeCameraPoint(file, "camera_debug.plane_vertex", clip_from_world, plane_vertex);
        writeCameraPoint(file, "camera_debug.cuboid_vertex", clip_from_world, cuboid_vertex);
        file << "camera_debug.cuboid_depth_less_than_plane="
             << (ndcPosition(clipPosition(clip_from_world, cuboid_vertex)).z <
                 ndcPosition(clipPosition(clip_from_world, plane_vertex)).z) << '\n';
        file << "render_scene.mesh_count=" << render_system.sceneMeshCount() << '\n';
        file << "render_scene.material_count=" << render_system.sceneMaterialCount() << '\n';
        file << "render_scene.instance_count=" << render_system.sceneInstanceCount() << '\n';
        file << "render_scene.vertex_count=" << render_system.sceneVertexCount() << '\n';
        file << "render_scene.index_count=" << render_system.sceneIndexCount() << '\n';
        file << "render_scene.camera=" << render_system.hasSceneCamera() << '\n';
        file << "render_scene.directional_light=" << render_system.hasSceneDirectionalLight() << '\n';
        file << "render_scene.point_light=" << render_system.hasScenePointLight() << '\n';
        file << "render_scene.spot_light=" << render_system.hasSceneSpotLight() << '\n';

        for (const auto &sample : samples_) {
            const auto result = evaluateSample(sample, light_, point_light_, spot_light_, cuboid_);
            file << "sample." << result.sample.name << ".n_dot_l=" << result.n_dot_l << '\n';
            file << "sample." << result.sample.name << ".shadow_factor=" << result.shadow_factor << '\n';
            writeVec3(file, "sample." + std::string{result.sample.name} + ".position", result.sample.position);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".normal", result.sample.normal);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".point_lighting",
                      result.point_lighting);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".spot_lighting",
                      result.spot_lighting);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".final_lighting", result.final_lighting);
            writeVec3(file, "sample." + std::string{result.sample.name} + ".final_color", result.final_color);
        }

        std::cout << '[' << name() << "] wrote " << output_path_.string() << '\n';
        return {};
    }

private:
    /// @brief Builds the CPU render scene that the future Vulkan pass will upload.
    [[nodiscard]] std::expected<void, vve::Error> buildRenderScene(vve::RenderSystem &render_system) const {
        render_system.clearScene();
        const auto plane_color = vve::LinearColor{.value = vve::Vec3{0.55F, 0.55F, 0.55F}};
        const auto cuboid_color = vve::LinearColor{.value = vve::Vec3{0.80F, 0.72F, 0.62F}};
        const auto plane_transform = vve::Transform{.translation = vve::Position{.value = plane_.center}};
        if (const auto result = render_system.addPlane(plane_.half_extent, plane_color, plane_transform); !result) {
            return result;
        }
        if (const auto result = render_system.addCuboid(cuboid_.minimum, cuboid_.maximum, cuboid_color); !result) {
            return result;
        }
        render_system.setCamera(camera_, render_extent_);
        render_system.setDirectionalLight(light_.direction_to_light, light_.color, light_.intensity, light_.ambient);
        render_system.setPointLight(point_light_.position, point_light_.color,
                                    point_light_.intensity, point_light_.range);
        render_system.setSpotLight(spot_light_.position, spot_light_.direction, spot_light_.color,
                                   spot_light_.intensity, spot_light_.range, spot_light_.cone);
        return {};
    }

    DebugPlane plane_{.center = vve::Vec3{0.0F, 0.0F, 0.0F}, .half_extent = vve::Vec2{3.0F, 3.0F}};
    DebugCuboid cuboid_{.minimum = vve::Vec3{-0.225F, 0.0F, -0.225F},
                        .maximum = vve::Vec3{0.225F, 2.0F, 0.225F}};
    DebugDirectionalLight light_{.direction_to_light = vve::Direction{.value = vve::Vec3{0.55F, 0.85F, 0.35F}},
                                 .color = vve::LinearColor{.value = vve::Vec3{1.0F, 0.94F, 0.84F}},
                                 .intensity = vve::LightIntensity{.value = 1.25F},
                                 .ambient = vve::LinearColor{.value = vve::Vec3{0.08F, 0.09F, 0.10F}}};
    DebugSpotLight spot_light_{.position = vve::Position{.value = vve::Vec3{-2.35F, 2.05F, 1.65F}},
                               .direction = vve::Direction{.value = vve::Vec3{2.25F, -1.35F, -1.75F}},
                               .color = vve::LinearColor{.value = vve::Vec3{1.0F, 0.72F, 0.42F}},
                               .intensity = vve::LightIntensity{.value = 5.0F},
                               .range = vve::LightRange{.value = 6.0F},
                               .cone = vve::SpotConeAngle{.radians = 0.78F}};
    DebugPointLight point_light_{.position = vve::Position{.value = vve::Vec3{2.40F, 1.35F, 1.10F}},
                                 .color = vve::LinearColor{.value = vve::Vec3{0.45F, 0.70F, 1.0F}},
                                 .intensity = vve::LightIntensity{.value = 7.0F},
                                 .range = vve::LightRange{.value = 6.0F}};
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
                         .position = vve::Vec3{-0.75F, 0.0F, -0.45F},
                         .normal = vve::Vec3{0.0F, 1.0F, 0.0F}},
        DebugSamplePoint{.name = "cuboid_lit_side",
                         .position = vve::Vec3{0.225F, 1.0F, 0.0F},
                         .normal = vve::Vec3{1.0F, 0.0F, 0.0F}}};
    vve::PixelExtent render_extent_{.width = 960, .height = 540}; ///< CPU render target size.
    std::filesystem::path output_path_{};
};

} // namespace

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const auto reference_path = outputPath(argc, argv);
    const auto inspect = inspectMode(argc, argv);
    auto engine = vve::makeEngine(
        vve::ApplicationName{"light-shadow-debug"},
        vve::MaxFrames{.value = vve::FrameCount{.value = inspect ? 0U : 1U}},
        vve::makeUserSystems(LightShadowDebugSystem{reference_path}),
        vve::WindowSetups{vve::WindowSetup{}
                              .id("light-shadow-debug.main")
                              .title("VVE Light Shadow Debug")
                              .extent(vve::PixelExtent{.width = 960, .height = 540})
                              .renderer(vve::RendererId{.value = "forward"})
                              .resizable(false)
                              .visible(true)});

    if (const auto run_result = engine.run(); !run_result) {
        std::cerr << "[light_shadow_debug] engine.run failed: " << vve::errorName(run_result.error()) << '\n';
        return 1;
    }
    auto world = engine.world();
    auto &render_system = world.template get<vve::RenderSystem>();
    bool gpu_verified{};
    bool all_gpu_samples_match{true};
    bool shadow_verified{};
    bool spot_shadow_verified{};
    bool point_shadow_verified{};
    bool all_shadow_samples_match{true};
    bool all_spot_shadow_samples_match{true};
    bool all_point_shadow_samples_match{true};
    if (std::ofstream file{reference_path, std::ios::app}) {
        const auto clear_color = render_system.lastClearColor();
        file << "engine.rendered_frames=" << render_system.renderedFrameCount() << '\n';
        file << "engine.prepared_gpu_targets=" << render_system.preparedGpuTargetCount() << '\n';
        file << "engine.frame_presented=" << render_system.presentedFrameCount() << '\n';
        file << "engine.triangle_drawn=" << render_system.triangleDrawCount() << '\n';
        file << "engine.triangle_vertex_count=" << render_system.triangleVertexCount() << '\n';
        file << "engine.scene_uploaded=" << render_system.sceneUploadCount() << '\n';
        file << "engine.scene_meshes_drawn=" << render_system.sceneMeshDrawCount() << '\n';
        file << "engine.scene_instances_drawn=" << render_system.sceneInstanceDrawCount() << '\n';
        file << "engine.scene_vertex_count=" << render_system.sceneDrawVertexCount() << '\n';
        file << "engine.scene_index_count=" << render_system.sceneDrawIndexCount() << '\n';
        file << "engine.clear_color=" << clear_color[0] << ',' << clear_color[1] << ','
             << clear_color[2] << ',' << clear_color[3] << '\n';
        file << "gpu_debug.sample_count=" << render_system.sceneDebugSampleCount() << '\n';
        for (std::size_t index{}; index < render_system.sceneDebugSampleCount(); ++index) {
            const auto cpu = render_system.sceneCpuDebugSample(index);
            const auto gpu = render_system.sceneGpuDebugSample(index);
            if (cpu) { writeRenderDebugSample(file, "gpu_debug.sample" + std::to_string(index) + ".cpu", *cpu); }
            if (gpu) { writeRenderDebugSample(file, "gpu_debug.sample" + std::to_string(index) + ".gpu", *gpu); }
            const auto clip_error = render_system.sceneDebugClipError(index);
            const auto depth_error = render_system.sceneDebugDepthError(index);
            const auto light_space_error = render_system.sceneDebugLightSpaceError(index);
            const auto spot_light_space_error = render_system.sceneDebugSpotLightSpaceError(index);
            const auto point_light_space_error = render_system.sceneDebugPointLightSpaceError(index);
            const auto lighting_error = render_system.sceneDebugLightingError(index);
            const auto shadow_sample_error = render_system.sceneDebugShadowSampleError(index);
            const auto spot_shadow_sample_error = render_system.sceneDebugSpotShadowSampleError(index);
            const auto point_shadow_sample_error = render_system.sceneDebugPointShadowSampleError(index);
            file << "gpu_debug.sample" << index << ".has_gpu=" << gpu.has_value() << '\n';
            if (clip_error) { file << "gpu_debug.sample" << index << ".clip_error=" << *clip_error << '\n'; }
            if (depth_error) { file << "gpu_debug.sample" << index << ".depth_error=" << *depth_error << '\n'; }
            if (light_space_error) {
                file << "gpu_debug.sample" << index << ".light_space_error=" << *light_space_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".light_space_matches="
                 << (light_space_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (spot_light_space_error) {
                file << "gpu_debug.sample" << index << ".spot_light_space_error="
                     << *spot_light_space_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".spot_light_space_matches="
                 << (spot_light_space_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (point_light_space_error) {
                file << "gpu_debug.sample" << index << ".point_light_space_error="
                     << *point_light_space_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".point_light_space_matches="
                 << (point_light_space_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (lighting_error) {
                file << "gpu_debug.sample" << index << ".lighting_error=" << *lighting_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".lighting_matches="
                 << (lighting_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (shadow_sample_error) {
                file << "gpu_debug.sample" << index << ".shadow_sample_error=" << *shadow_sample_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".shadow_sample_matches="
                 << (shadow_sample_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (spot_shadow_sample_error) {
                file << "gpu_debug.sample" << index << ".spot_shadow_sample_error="
                     << *spot_shadow_sample_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".spot_shadow_sample_matches="
                 << (spot_shadow_sample_error.value_or(1.0F) < 1.0e-5F) << '\n';
            if (point_shadow_sample_error) {
                file << "gpu_debug.sample" << index << ".point_shadow_sample_error="
                     << *point_shadow_sample_error << '\n';
            }
            file << "gpu_debug.sample" << index << ".point_shadow_sample_matches="
                 << (point_shadow_sample_error.value_or(1.0F) < 1.0e-5F) << '\n';
            const auto sample_matches = clip_error.value_or(1.0F) < 1.0e-4F &&
                                        depth_error.value_or(1.0F) < 1.0e-5F &&
                                        light_space_error.value_or(1.0F) < 1.0e-5F &&
                                        spot_light_space_error.value_or(1.0F) < 1.0e-5F &&
                                        point_light_space_error.value_or(1.0F) < 1.0e-5F &&
                                        lighting_error.value_or(1.0F) < 1.0e-5F &&
                                        shadow_sample_error.value_or(1.0F) < 1.0e-5F &&
                                        spot_shadow_sample_error.value_or(1.0F) < 1.0e-5F &&
                                        point_shadow_sample_error.value_or(1.0F) < 1.0e-5F;
            file << "gpu_debug.sample" << index << ".matches=" << sample_matches << '\n';
            gpu_verified |= gpu.has_value();
            all_gpu_samples_match &= sample_matches;
        }
        file << "shadow_depth.sample_count=" << render_system.sceneShadowDepthSampleCount() << '\n';
        for (std::size_t index{}; index < render_system.sceneShadowDepthSampleCount(); ++index) {
            const auto sample = render_system.sceneShadowDepthSample(index);
            if (!sample) { continue; }
            writeShadowDepthSample(file, "shadow_depth.sample" + std::to_string(index), *sample);
            const auto matches = sample->has_gpu && sample->error < 3.0e-2F;
            file << "shadow_depth.sample" << index << ".matches=" << matches << '\n';
            shadow_verified |= sample->has_gpu;
            all_shadow_samples_match &= matches;
        }
        file << "spot_shadow_depth.sample_count=" << render_system.sceneSpotShadowDepthSampleCount() << '\n';
        for (std::size_t index{}; index < render_system.sceneSpotShadowDepthSampleCount(); ++index) {
            const auto sample = render_system.sceneSpotShadowDepthSample(index);
            if (!sample) { continue; }
            writeShadowDepthSample(file, "spot_shadow_depth.sample" + std::to_string(index), *sample);
            const auto matches = sample->has_gpu && sample->error < 3.0e-2F;
            file << "spot_shadow_depth.sample" << index << ".matches=" << matches << '\n';
            spot_shadow_verified |= sample->has_gpu;
            all_spot_shadow_samples_match &= matches;
        }
        file << "point_shadow_depth.sample_count=" << render_system.scenePointShadowDepthSampleCount() << '\n';
        for (std::size_t index{}; index < render_system.scenePointShadowDepthSampleCount(); ++index) {
            const auto sample = render_system.scenePointShadowDepthSample(index);
            if (!sample) { continue; }
            writeShadowDepthSample(file, "point_shadow_depth.sample" + std::to_string(index), *sample);
            const auto shadowed = sample->has_gpu && sample->expected_depth > sample->gpu_depth + 3.0e-2F;
            const auto matches = sample->has_gpu && (sample->error < 3.0e-2F || shadowed);
            file << "point_shadow_depth.sample" << index << ".shadowed=" << shadowed << '\n';
            file << "point_shadow_depth.sample" << index << ".matches=" << matches << '\n';
            point_shadow_verified |= sample->has_gpu;
            all_point_shadow_samples_match &= matches;
        }
    } else {
        std::cerr << "[light_shadow_debug] could not write " << reference_path << '\n';
        return 2;
    }
    if (!inspect && (!gpu_verified || !all_gpu_samples_match)) { return 3; }
    if (!inspect && (!shadow_verified || !all_shadow_samples_match)) { return 4; }
    if (!inspect && (!spot_shadow_verified || !all_spot_shadow_samples_match)) { return 5; }
    if (!inspect && (!point_shadow_verified || !all_point_shadow_samples_match)) { return 6; }
    return 0;
}
