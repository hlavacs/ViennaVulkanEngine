module;

#include "FacadeMacros.hpp"
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 asset-system implementation.
 *
 * The implementation preserves CPU-side scene content and references without
 * committing to any backend-specific upload or rendering strategy.
 */
namespace vve::v3 {

   namespace {

      static const std::map<aiTextureType, TextureSemantic> assimp_texture_semantic_map{
          {aiTextureType_BASE_COLOR, TextureSemantic::base_color},
          {aiTextureType_DIFFUSE, TextureSemantic::base_color},
          {aiTextureType_NORMALS, TextureSemantic::normal},
          {aiTextureType_HEIGHT, TextureSemantic::normal},
          {aiTextureType_UNKNOWN, TextureSemantic::metallic_roughness},
          {aiTextureType_DIFFUSE_ROUGHNESS, TextureSemantic::roughness},
          {aiTextureType_METALNESS, TextureSemantic::metallic},
          {aiTextureType_SPECULAR, TextureSemantic::specular},
          {aiTextureType_EMISSIVE, TextureSemantic::emissive},
          {aiTextureType_OPACITY, TextureSemantic::opacity},
          {aiTextureType_AMBIENT_OCCLUSION, TextureSemantic::ambient_occlusion},
          {aiTextureType_LIGHTMAP, TextureSemantic::ambient_occlusion},
      };

      [[nodiscard]] std::filesystem::path normalizePath(const std::filesystem::path &path) {
         std::error_code error_code{};
         const auto canonical_path = std::filesystem::weakly_canonical(path, error_code);
         return error_code ? path.lexically_normal() : canonical_path;
      }

      [[nodiscard]] vve::math::Vec2 toVec2(const aiVector3D &value) {
         return vve::math::Vec2(static_cast<vve::math::Scalar>(value.x), static_cast<vve::math::Scalar>(value.y));
      }

      [[nodiscard]] vve::math::Vec3 toVec3(const aiVector3D &value) {
         return vve::math::Vec3(static_cast<vve::math::Scalar>(value.x), static_cast<vve::math::Scalar>(value.y),
                                static_cast<vve::math::Scalar>(value.z));
      }

      [[nodiscard]] vve::math::Vec3 toVec3(const aiColor3D &value) {
         return vve::math::Vec3(static_cast<vve::math::Scalar>(value.r), static_cast<vve::math::Scalar>(value.g),
                                static_cast<vve::math::Scalar>(value.b));
      }

      [[nodiscard]] vve::math::Vec4 toVec4(const aiColor4D &value) {
         return vve::math::Vec4(static_cast<vve::math::Scalar>(value.r), static_cast<vve::math::Scalar>(value.g),
                                static_cast<vve::math::Scalar>(value.b), static_cast<vve::math::Scalar>(value.a));
      }

      [[nodiscard]] vve::math::Mat4 toMat4(const aiMatrix4x4 &matrix) {
         vve::math::Mat4 result{vve::math::identityMat4()};
         result[0][0] = static_cast<vve::math::Scalar>(matrix.a1);
         result[1][0] = static_cast<vve::math::Scalar>(matrix.a2);
         result[2][0] = static_cast<vve::math::Scalar>(matrix.a3);
         result[3][0] = static_cast<vve::math::Scalar>(matrix.a4);
         result[0][1] = static_cast<vve::math::Scalar>(matrix.b1);
         result[1][1] = static_cast<vve::math::Scalar>(matrix.b2);
         result[2][1] = static_cast<vve::math::Scalar>(matrix.b3);
         result[3][1] = static_cast<vve::math::Scalar>(matrix.b4);
         result[0][2] = static_cast<vve::math::Scalar>(matrix.c1);
         result[1][2] = static_cast<vve::math::Scalar>(matrix.c2);
         result[2][2] = static_cast<vve::math::Scalar>(matrix.c3);
         result[3][2] = static_cast<vve::math::Scalar>(matrix.c4);
         result[0][3] = static_cast<vve::math::Scalar>(matrix.d1);
         result[1][3] = static_cast<vve::math::Scalar>(matrix.d2);
         result[2][3] = static_cast<vve::math::Scalar>(matrix.d3);
         result[3][3] = static_cast<vve::math::Scalar>(matrix.d4);
         return result;
      }

      [[nodiscard]] TextureSemantic mapTextureSemantic(const aiTextureType texture_type) {
         return vve::detail::mapValueOr(assimp_texture_semantic_map, texture_type, TextureSemantic::unknown);
      }

      [[nodiscard]] SceneLightType mapLightType(const aiLightSourceType light_type) {
         switch (light_type) {
         case aiLightSource_DIRECTIONAL:
            return SceneLightType::directional;
         case aiLightSource_POINT:
            return SceneLightType::point;
         case aiLightSource_SPOT:
            return SceneLightType::spot;
         case aiLightSource_AMBIENT:
            return SceneLightType::ambient;
         default:
            return SceneLightType::unknown;
         }
      }

      [[nodiscard]] bool nearZero(const vve::math::Scalar value) {
         return std::abs(value) <= static_cast<vve::math::Scalar>(0.000001);
      }

      [[nodiscard]] vve::math::Scalar lengthSquared(const vve::math::Vec3 &value) {
         return (value.x * value.x) + (value.y * value.y) + (value.z * value.z);
      }

      [[nodiscard]] vve::math::Vec3 normalizeVec3(const vve::math::Vec3 &value,
                                                  const vve::math::Vec3 &fallback) {
         const auto squared_length = lengthSquared(value);
         if (squared_length <= static_cast<vve::math::Scalar>(0.000001)) {
            return fallback;
         }

         const auto inverse_length = vve::math::one() / std::sqrt(squared_length);
         return vve::math::Vec3(value.x * inverse_length, value.y * inverse_length, value.z * inverse_length);
      }

      [[nodiscard]] vve::math::Vec3 visibleColor(const aiLight &light) {
         auto color = toVec3(light.mColorDiffuse);
         if (lengthSquared(color) > static_cast<vve::math::Scalar>(0.000001)) {
            return color;
         }

         color = toVec3(light.mColorSpecular);
         if (lengthSquared(color) > static_cast<vve::math::Scalar>(0.000001)) {
            return color;
         }

         color = toVec3(light.mColorAmbient);
         return lengthSquared(color) > static_cast<vve::math::Scalar>(0.000001) ? color : vve::math::oneVec3();
      }

      [[nodiscard]] vve::math::Scalar lightIntensityFromColor(const vve::math::Vec3 &color) {
         return std::max({std::abs(color.x), std::abs(color.y), std::abs(color.z), vve::math::one()});
      }

      [[nodiscard]] vve::math::Scalar lightRangeFromAttenuation(const aiLight &light) {
         const auto constant = static_cast<vve::math::Scalar>(light.mAttenuationConstant);
         const auto linear = static_cast<vve::math::Scalar>(light.mAttenuationLinear);
         const auto quadratic = static_cast<vve::math::Scalar>(light.mAttenuationQuadratic);
         constexpr auto minimum_factor = static_cast<vve::math::Scalar>(100.0);

         if (nearZero(constant) && nearZero(linear - vve::math::one()) && nearZero(quadratic)) {
            return vve::math::zero();
         }

         if (nearZero(quadratic) && nearZero(linear)) {
            return vve::math::zero();
         }

         if (!nearZero(quadratic)) {
            const auto discriminant =
                (linear * linear) - (static_cast<vve::math::Scalar>(4.0) * quadratic * (constant - minimum_factor));
            if (discriminant <= vve::math::zero()) {
               return vve::math::zero();
            }

            const auto range = (-linear + std::sqrt(discriminant)) /
                               (static_cast<vve::math::Scalar>(2.0) * quadratic);
            return range > vve::math::zero() && range < static_cast<vve::math::Scalar>(10000.0)
                       ? range
                       : vve::math::zero();
         }

         const auto range = (minimum_factor - constant) / linear;
         return range > vve::math::zero() && range < static_cast<vve::math::Scalar>(10000.0)
                    ? range
                    : vve::math::zero();
      }

      [[nodiscard]] SceneNodeHandle findImportedNodeByName(const ImportedScene &scene, std::string_view name) {
         if (name.empty()) {
            return {};
         }

         const auto node = std::ranges::find_if(scene.nodes, [name](const ImportedSceneNode &candidate) {
            return candidate.name == name;
         });
         return node == scene.nodes.end() ? SceneNodeHandle{} : node->handle;
      }

      [[nodiscard]] ImportedLight importLight(const aiLight &light, const std::uint32_t light_index,
                                              const ImportedScene &scene, std::string_view scene_seed) {
         const auto name =
             light.mName.length > 0 ? std::string(light.mName.C_Str()) : std::format("Light_{}", light_index);
         const auto color = visibleColor(light);
         ImportedLight imported_light{};
         imported_light.handle =
             LightHandle{detail::makeStableHandle(std::format("{}::light::{}", scene_seed, light_index))};
         imported_light.node = findImportedNodeByName(scene, name);
         imported_light.name = name;
         imported_light.type = mapLightType(light.mType);
         imported_light.local_position = toVec3(light.mPosition);
         imported_light.local_direction = normalizeVec3(
             toVec3(light.mDirection),
             vve::math::Vec3(vve::math::zero(), -vve::math::one(), vve::math::zero()));
         imported_light.color = color;
         imported_light.intensity = lightIntensityFromColor(color);
         imported_light.range = lightRangeFromAttenuation(light);
         imported_light.inner_cone_cos = imported_light.type == SceneLightType::spot
                                             ? std::cos(static_cast<vve::math::Scalar>(light.mAngleInnerCone))
                                             : vve::math::one();
         imported_light.outer_cone_cos = imported_light.type == SceneLightType::spot
                                             ? std::cos(static_cast<vve::math::Scalar>(light.mAngleOuterCone))
                                             : vve::math::zero();
         if (imported_light.outer_cone_cos > imported_light.inner_cone_cos) {
            std::swap(imported_light.outer_cone_cos, imported_light.inner_cone_cos);
         }

         return imported_light;
      }

      [[nodiscard]] CameraFrameData cameraFrameDataFromAssimpCamera(const aiCamera &camera) {
         const auto position = toVec3(camera.mPosition);
         const auto look_direction = normalizeVec3(
             toVec3(camera.mLookAt),
             vve::math::Vec3(vve::math::zero(), vve::math::zero(), -vve::math::one()));
         const auto up = normalizeVec3(
             toVec3(camera.mUp),
             vve::math::Vec3(vve::math::zero(), vve::math::one(), vve::math::zero()));
         const auto target = vve::math::Vec3(position.x + look_direction.x, position.y + look_direction.y,
                                             position.z + look_direction.z);
         const auto aspect = std::max(static_cast<vve::math::Scalar>(camera.mAspect), vve::math::one());
         const auto horizontal_fov = std::max(static_cast<vve::math::Scalar>(camera.mHorizontalFOV),
                                              static_cast<vve::math::Scalar>(0.001));
         const auto vertical_fov =
             static_cast<vve::math::Scalar>(2.0) * std::atan(std::tan(horizontal_fov * static_cast<vve::math::Scalar>(0.5)) /
                                                             aspect);
         return CameraFrameData{.position = position,
                                .view_transform = vve::math::lookAt(position, target, up),
                                .vertical_fov_radians = vertical_fov,
                                .near_plane = std::max(static_cast<vve::math::Scalar>(camera.mClipPlaneNear),
                                                       static_cast<vve::math::Scalar>(0.001)),
                                .far_plane = std::max(static_cast<vve::math::Scalar>(camera.mClipPlaneFar),
                                                      static_cast<vve::math::Scalar>(1.0))};
      }

      [[nodiscard]] ImportedCamera importCamera(const aiCamera &camera, const std::uint32_t camera_index,
                                                const ImportedScene &scene, std::string_view scene_seed) {
         const auto name =
             camera.mName.length > 0 ? std::string(camera.mName.C_Str()) : std::format("Camera_{}", camera_index);
         return ImportedCamera{
             .handle = CameraHandle{detail::makeStableHandle(std::format("{}::camera::{}", scene_seed, camera_index))},
             .node = findImportedNodeByName(scene, name),
             .name = name,
             .camera = cameraFrameDataFromAssimpCamera(camera)};
      }

      class TextureRegistry {
      public:
         TextureRegistry(ImportedScene &scene, std::filesystem::path source_directory, std::string_view scene_seed)
             : scene_(scene), source_directory_(std::move(source_directory)), scene_seed_(scene_seed) {}

         [[nodiscard]] TextureHandle registerTexture(const aiString &texture_path) {
            const std::string original_path_string =
                texture_path.length == 0 ? std::string{} : std::string(texture_path.C_Str());
            if (original_path_string.empty()) {
               return {};
            }

            if (original_path_string.front() == '*') {
               return registerEmbeddedTexture(original_path_string);
            }

            const auto original_path = std::filesystem::path(original_path_string);
            const auto resolved_path = normalizePath(original_path.is_absolute() ? original_path
                                                                                 : source_directory_ / original_path);
            const auto cache_key = resolved_path.generic_string();
            if (const auto existing_handle = external_handles_.find(cache_key); existing_handle != external_handles_.end()) {
               return existing_handle->second;
            }

            const auto handle =
                TextureHandle{detail::makeStableHandle(std::format("{}::texture::{}", scene_seed_, cache_key))};
            const auto texture_name = resolved_path.filename().string().empty() ? cache_key : resolved_path.filename().string();
            scene_.textures.push_back(ImportedTexture{.handle = handle,
                                                      .name = texture_name,
                                                      .resolved_path = resolved_path,
                                                      .original_path = original_path,
                                                      .embedded = false,
                                                      .embedded_id = {}});
            external_handles_.emplace(cache_key, handle);
            return handle;
         }

      private:
         [[nodiscard]] TextureHandle registerEmbeddedTexture(const std::string &embedded_id) {
            if (const auto existing_handle = embedded_handles_.find(embedded_id); existing_handle != embedded_handles_.end()) {
               return existing_handle->second;
            }

            const auto handle =
                TextureHandle{detail::makeStableHandle(std::format("{}::embedded_texture::{}", scene_seed_, embedded_id))};
            scene_.textures.push_back(ImportedTexture{.handle = handle,
                                                      .name = embedded_id,
                                                      .resolved_path = {},
                                                      .original_path = std::filesystem::path(embedded_id),
                                                      .embedded = true,
                                                      .embedded_id = embedded_id});
            embedded_handles_.emplace(embedded_id, handle);
            return handle;
         }

         ImportedScene &scene_;
         std::filesystem::path source_directory_;
         std::string_view scene_seed_;
         std::unordered_map<std::string, TextureHandle> external_handles_{};
         std::unordered_map<std::string, TextureHandle> embedded_handles_{};
      };

      void appendTextureRefs(const aiMaterial &material, aiTextureType texture_type, TextureRegistry &texture_registry,
                             ImportedMaterial &imported_material) {
         const auto semantic = mapTextureSemantic(texture_type);
         for (unsigned texture_index = 0; texture_index < material.GetTextureCount(texture_type); ++texture_index) {
            aiString texture_path{};
            unsigned uv_index = 0;
            if (material.GetTexture(texture_type, texture_index, &texture_path, nullptr, &uv_index) != aiReturn_SUCCESS) {
               continue;
            }

            const auto texture_handle = texture_registry.registerTexture(texture_path);
            if (!texture_handle.value.isValid()) {
               continue;
            }

            imported_material.textures.push_back(
                ImportedTextureRef{.texture = texture_handle, .semantic = semantic, .uv_set = uv_index});
         }
      }

      [[nodiscard]] std::string readMaterialName(const aiMaterial &material, const std::uint32_t material_index) {
         aiString material_name{};
         if (material.Get(AI_MATKEY_NAME, material_name) == aiReturn_SUCCESS && material_name.length > 0) {
            return material_name.C_Str();
         }

         return std::format("Material_{}", material_index);
      }

      [[nodiscard]] ImportedMaterial importMaterial(const aiMaterial &material, const std::uint32_t material_index,
                                                    TextureRegistry &texture_registry, std::string_view scene_seed) {
         ImportedMaterial imported_material{};
         imported_material.handle =
             MaterialHandle{detail::makeStableHandle(std::format("{}::material::{}", scene_seed, material_index))};
         imported_material.name = readMaterialName(material, material_index);

         aiColor4D base_color{};
         if (material.Get(AI_MATKEY_BASE_COLOR, base_color) == aiReturn_SUCCESS) {
            imported_material.base_color_factor = toVec4(base_color);
         } else {
            aiColor3D diffuse_color{};
            if (material.Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color) == aiReturn_SUCCESS) {
               const auto diffuse = toVec3(diffuse_color);
               imported_material.base_color_factor =
                   vve::math::Vec4(diffuse.x, diffuse.y, diffuse.z, imported_material.base_color_factor.w);
            }
         }

         ai_real opacity = static_cast<ai_real>(imported_material.base_color_factor.w);
         if (material.Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS) {
            imported_material.base_color_factor.w = static_cast<vve::math::Scalar>(opacity);
            imported_material.alpha_blend = opacity < static_cast<ai_real>(1.0);
         }

         aiColor3D emissive_color{};
         if (material.Get(AI_MATKEY_COLOR_EMISSIVE, emissive_color) == aiReturn_SUCCESS) {
            imported_material.emissive_factor = toVec3(emissive_color);
         }

         ai_real roughness = static_cast<ai_real>(imported_material.roughness_factor);
         if (material.Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
            imported_material.roughness_factor = static_cast<vve::math::Scalar>(roughness);
         }

         ai_real metallic = static_cast<ai_real>(imported_material.metallic_factor);
         if (material.Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
            imported_material.metallic_factor = static_cast<vve::math::Scalar>(metallic);
         }

         ai_real bump_scaling = static_cast<ai_real>(imported_material.normal_scale);
         if (material.Get(AI_MATKEY_BUMPSCALING, bump_scaling) == aiReturn_SUCCESS) {
            imported_material.normal_scale = static_cast<vve::math::Scalar>(bump_scaling);
         }

         ai_real alpha_cutoff = static_cast<ai_real>(imported_material.alpha_cutoff);
         if (material.Get(AI_MATKEY_GLTF_ALPHACUTOFF, alpha_cutoff) == aiReturn_SUCCESS) {
            imported_material.alpha_cutoff = static_cast<vve::math::Scalar>(alpha_cutoff);
         }

         int two_sided = 0;
         if (material.Get(AI_MATKEY_TWOSIDED, two_sided) == aiReturn_SUCCESS) {
            imported_material.double_sided = two_sided != 0;
         }

         int blend_mode = 0;
         if (material.Get(AI_MATKEY_BLEND_FUNC, blend_mode) == aiReturn_SUCCESS) {
            imported_material.alpha_blend = imported_material.alpha_blend || blend_mode != 0;
         }

         appendTextureRefs(material, aiTextureType_BASE_COLOR, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_DIFFUSE, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_NORMALS, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_HEIGHT, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_UNKNOWN, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_DIFFUSE_ROUGHNESS, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_METALNESS, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_SPECULAR, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_EMISSIVE, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_OPACITY, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_AMBIENT_OCCLUSION, texture_registry, imported_material);
         appendTextureRefs(material, aiTextureType_LIGHTMAP, texture_registry, imported_material);

         return imported_material;
      }

      [[nodiscard]] ImportedMesh importMesh(const aiMesh &mesh, const std::uint32_t mesh_index,
                                            std::string_view scene_seed, const std::filesystem::path &source_path,
                                            const std::vector<MaterialHandle> &material_handles) {
         ImportedMesh imported_mesh{};
         imported_mesh.handle = MeshHandle{detail::makeStableHandle(std::format("{}::mesh::{}", scene_seed, mesh_index))};
         imported_mesh.name = mesh.mName.length > 0 ? std::string(mesh.mName.C_Str()) : std::format("Mesh_{}", mesh_index);
         imported_mesh.source_path = source_path;
         imported_mesh.vertices.reserve(mesh.mNumVertices);

         bool has_bounds = false;
         for (unsigned vertex_index = 0; vertex_index < mesh.mNumVertices; ++vertex_index) {
            ImportedVertex imported_vertex{};
            if (mesh.HasPositions()) {
               imported_vertex.position = toVec3(mesh.mVertices[vertex_index]);
            }

            if (mesh.HasNormals()) {
               imported_vertex.normal = toVec3(mesh.mNormals[vertex_index]);
            }

            if (mesh.HasTangentsAndBitangents()) {
               imported_vertex.tangent = toVec3(mesh.mTangents[vertex_index]);
               imported_vertex.bitangent = toVec3(mesh.mBitangents[vertex_index]);
            }

            if (mesh.HasTextureCoords(0)) {
               imported_vertex.texcoord0 = toVec2(mesh.mTextureCoords[0][vertex_index]);
            }

            if (mesh.HasVertexColors(0)) {
               imported_vertex.color0 = toVec4(mesh.mColors[0][vertex_index]);
            }

            if (!has_bounds) {
               imported_mesh.bounds_min = imported_vertex.position;
               imported_mesh.bounds_max = imported_vertex.position;
               has_bounds = true;
            } else {
               imported_mesh.bounds_min.x = std::min(imported_mesh.bounds_min.x, imported_vertex.position.x);
               imported_mesh.bounds_min.y = std::min(imported_mesh.bounds_min.y, imported_vertex.position.y);
               imported_mesh.bounds_min.z = std::min(imported_mesh.bounds_min.z, imported_vertex.position.z);
               imported_mesh.bounds_max.x = std::max(imported_mesh.bounds_max.x, imported_vertex.position.x);
               imported_mesh.bounds_max.y = std::max(imported_mesh.bounds_max.y, imported_vertex.position.y);
               imported_mesh.bounds_max.z = std::max(imported_mesh.bounds_max.z, imported_vertex.position.z);
            }

            imported_mesh.vertices.push_back(imported_vertex);
         }

         std::size_t total_index_count = 0;
         for (unsigned face_index = 0; face_index < mesh.mNumFaces; ++face_index) {
            total_index_count += mesh.mFaces[face_index].mNumIndices;
         }

         imported_mesh.indices.reserve(total_index_count);
         for (unsigned face_index = 0; face_index < mesh.mNumFaces; ++face_index) {
            const auto &face = mesh.mFaces[face_index];
            for (unsigned index_index = 0; index_index < face.mNumIndices; ++index_index) {
               imported_mesh.indices.push_back(face.mIndices[index_index]);
            }
         }

         MaterialHandle material_handle{};
         if (mesh.mMaterialIndex < material_handles.size()) {
            material_handle = material_handles[mesh.mMaterialIndex];
         }

         imported_mesh.submeshes.push_back(
             ImportedSubmesh{.index_offset = 0,
                             .index_count = static_cast<std::uint32_t>(imported_mesh.indices.size()),
                             .material = material_handle});
         return imported_mesh;
      }

      [[nodiscard]] std::expected<void, vve::Error>
      appendNodeRecursive(const aiNode &node, const SceneNodeHandle parent_handle, ImportedScene &scene,
                          const std::vector<MeshHandle> &mesh_handles, std::string_view scene_seed,
                          std::uint32_t &next_node_index, std::uint32_t &next_mesh_instance_index) {
         ImportedSceneNode imported_node{};
         imported_node.handle =
             SceneNodeHandle{detail::makeStableHandle(std::format("{}::node::{}", scene_seed, next_node_index++))};
         imported_node.parent = parent_handle;
         imported_node.name =
             node.mName.length > 0 ? std::string(node.mName.C_Str()) : std::format("Node_{}", next_node_index - 1);
         imported_node.local_transform = toMat4(node.mTransformation);
         imported_node.mesh_instances.reserve(node.mNumMeshes);

         for (unsigned mesh_slot = 0; mesh_slot < node.mNumMeshes; ++mesh_slot) {
            const auto mesh_index = static_cast<std::size_t>(node.mMeshes[mesh_slot]);
            if (mesh_index >= mesh_handles.size()) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            imported_node.mesh_instances.push_back(
                ImportedMeshInstance{.handle = detail::makeStableHandle(
                                         std::format("{}::mesh_instance::{}", scene_seed, next_mesh_instance_index++)),
                                     .mesh = mesh_handles[mesh_index],
                                     .material_override = std::nullopt});
         }

         const auto current_handle = imported_node.handle;
         scene.nodes.push_back(std::move(imported_node));
         for (unsigned child_index = 0; child_index < node.mNumChildren; ++child_index) {
            if (const auto child_result = appendNodeRecursive(*node.mChildren[child_index], current_handle, scene,
                                                              mesh_handles, scene_seed, next_node_index,
                                                              next_mesh_instance_index);
                !child_result) {
               return std::unexpected(child_result.error());
            }
         }

         return {};
      }

   } // namespace

   /**
    * @brief Assimp-backed asset importer used by the current v3 runtime.
    */
   class AssimpAssetSystemImplementation {
   public:
      /// @brief Returns the implementation name used in runtime diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "AssimpAssetSystem"; }

      /**
       * @brief Imports a source scene and preserves CPU-side geometry/material references.
       * @param source_path Scene path requested by the caller.
       * @return Imported scene content with geometry, materials, textures, and hierarchy preserved.
       */
      [[nodiscard]] std::expected<ImportedScene, vve::Error> importScene(const std::filesystem::path &source_path) {
         if (source_path.empty()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto normalized_source_path = normalizePath(source_path);
         if (!std::filesystem::exists(normalized_source_path)) {
            return std::unexpected(vve::Error::file_not_found);
         }

         Assimp::Importer importer{};
         constexpr unsigned import_flags =
             aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
             aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_SortByPType |
             aiProcess_FindInvalidData | aiProcess_ValidateDataStructure | aiProcess_GenBoundingBoxes;
         const aiScene *source_scene = importer.ReadFile(normalized_source_path.string(), import_flags);
         if (source_scene == nullptr || source_scene->mRootNode == nullptr ||
             (source_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0) {
            return std::unexpected(vve::Error::io_error);
         }

         ImportedScene scene{};
         scene.handle = SceneHandle{detail::makeStableHandle(normalized_source_path.string())};
         scene.name = normalized_source_path.stem().string().empty() ? normalized_source_path.filename().string()
                                                                     : normalized_source_path.stem().string();
         scene.source_path = normalized_source_path;

         const auto scene_seed = normalized_source_path.generic_string();
         TextureRegistry texture_registry(scene, normalized_source_path.parent_path(), scene_seed);
         std::vector<MaterialHandle> material_handles{};
         material_handles.reserve(source_scene->mNumMaterials);
         scene.materials.reserve(source_scene->mNumMaterials);
         for (unsigned material_index = 0; material_index < source_scene->mNumMaterials; ++material_index) {
            auto imported_material =
                importMaterial(*source_scene->mMaterials[material_index], material_index, texture_registry, scene_seed);
            material_handles.push_back(imported_material.handle);
            scene.materials.push_back(std::move(imported_material));
         }

         std::vector<MeshHandle> mesh_handles{};
         mesh_handles.reserve(source_scene->mNumMeshes);
         scene.meshes.reserve(source_scene->mNumMeshes);
         for (unsigned mesh_index = 0; mesh_index < source_scene->mNumMeshes; ++mesh_index) {
            auto imported_mesh =
                importMesh(*source_scene->mMeshes[mesh_index], mesh_index, scene_seed, normalized_source_path,
                           material_handles);
            mesh_handles.push_back(imported_mesh.handle);
            scene.meshes.push_back(std::move(imported_mesh));
         }

         std::uint32_t next_node_index = 0;
         std::uint32_t next_mesh_instance_index = 0;
         if (const auto node_result = appendNodeRecursive(*source_scene->mRootNode, {}, scene, mesh_handles, scene_seed,
                                                          next_node_index, next_mesh_instance_index);
             !node_result) {
            return std::unexpected(node_result.error());
         }

         scene.lights.reserve(source_scene->mNumLights);
         for (unsigned light_index = 0; light_index < source_scene->mNumLights; ++light_index) {
            if (source_scene->mLights[light_index] == nullptr) {
               continue;
            }
            scene.lights.push_back(importLight(*source_scene->mLights[light_index], light_index, scene, scene_seed));
         }

         scene.cameras.reserve(source_scene->mNumCameras);
         for (unsigned camera_index = 0; camera_index < source_scene->mNumCameras; ++camera_index) {
            if (source_scene->mCameras[camera_index] == nullptr) {
               continue;
            }
            scene.cameras.push_back(importCamera(*source_scene->mCameras[camera_index], camera_index, scene, scene_seed));
         }

         return scene;
      }
   };

   /// @brief Constructs the public asset-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(AssetSystemFacade, AssimpAssetSystemImplementation, (), ())

   /// @brief Returns the asset-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(AssetSystemFacade, AssimpAssetSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Imports a scene through the public asset-system facade.
   VVE_V3_DEFINE_FACADE_METHOD(AssetSystemFacade, AssimpAssetSystemImplementation, importScene,
                               (const std::filesystem::path &source_path), (source_path), ,
                               std::expected<ImportedScene, vve::Error>)

   /// @brief Emits the explicit asset-system facade instantiation for v3.
   template class AssetSystemFacade<AssimpAssetSystemImplementation>;

} // namespace vve::v3
