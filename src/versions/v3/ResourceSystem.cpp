module;

#include "FacadeMacros.hpp"
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module VEEngine.V3;
import std;
import :Internal;

/**
 * @file
 * @brief v3 resource-system implementation.
 *
 * The resource system tracks imported resources and simulates resource upload
 * state transitions so the rest of the runtime can reason about asset
 * lifetime without a full backend-specific resource manager yet.
 */
namespace vve::v3 {

   namespace {

   /// @brief Loads text source into a resource-owned shader payload.
   [[nodiscard]] std::expected<ShaderSource, vve::Error> loadShaderSource(const std::filesystem::path &shader_path) {
      const auto absolute_shader_path = std::filesystem::absolute(shader_path);
      if (!std::filesystem::exists(absolute_shader_path)) {
         return std::unexpected(vve::Error::file_not_found);
      }

      std::ifstream input(absolute_shader_path, std::ios::binary);
      if (!input) {
         return std::unexpected(vve::Error::io_error);
      }

      return ShaderSource{.source_path = absolute_shader_path,
                          .source_code = std::string(std::istreambuf_iterator<char>(input),
                                                     std::istreambuf_iterator<char>())};
   }

   /// @brief Builds the stable resource handle for a source/renderer/shadow shader variant.
   [[nodiscard]] ShaderHandle shaderVariantHandle(const ShaderSource &shader_source, vve::RendererKind renderer,
                                                  vve::ShadowKind shadow) {
      auto seed = shader_source.source_path.generic_string();
      seed.push_back(':');
      seed += vve::rendererKindName(renderer);
      seed.push_back(':');
      seed += vve::shadowKindName(shadow);
      return ShaderHandle{detail::makeStableHandle(seed)};
   }

   /// @brief Copies a segmented vector's object bytes into contiguous upload storage.
   template <typename TValue> [[nodiscard]] std::vector<std::byte> copyObjectBytes(const Vector<TValue> &values) {
      std::vector<std::byte> bytes(values.size() * sizeof(TValue));
      auto *cursor = bytes.data();
      for (const auto &value : values) {
         std::memcpy(cursor, std::addressof(value), sizeof(TValue));
         cursor += sizeof(TValue);
      }
      return bytes;
   }

   /// @brief Returns a uint32 count when a host-side range fits Vulkan draw/upload metadata.
   [[nodiscard]] std::expected<std::uint32_t, vve::Error> checkedUint32(std::size_t value) {
      if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
         return std::unexpected(vve::Error::invalid_argument);
      }
      return static_cast<std::uint32_t>(value);
   }

   /// @brief CPU-side material constant payload uploaded for forward shading.
   struct MaterialConstantsUpload {
      std::array<float, 4> base_color_factor{};
      std::array<float, 4> scalar_factors{};
      std::array<float, 4> emissive_factor{};
      std::array<float, 4> reserved{};
   };

   static_assert(sizeof(MaterialConstantsUpload) == sizeof(float) * 16U);

   // Large scenes should reach the present loop quickly; textures can stream
   // after fallback-shaded geometry is already visible.
   constexpr std::size_t maxMaterialUploadsPerFrame = 64;
   constexpr std::size_t maxMeshUploadsPerFrame = 16;
   constexpr std::size_t maxTextureUploadsPerFrame = 1;

   /// @brief CPU-side decoded texture pixels ready for backend upload.
   struct LoadedTexturePixels {
      std::vector<std::byte> rgba_pixels{};
      std::uint32_t width{0};
      std::uint32_t height{0};
      GpuImageFormat format{GpuImageFormat::unknown};
   };

   /// @brief Material texture bindings resolved against current GPU texture residency.
   struct MaterialTextureBindingUpload {
      Vector<GpuMaterialTextureBinding> bindings{};
      bool all_resident{true};
   };

   /// @brief Builds the material constant payload consumed by rasterizer.slang.
   [[nodiscard]] MaterialConstantsUpload materialConstantsUpload(const ImportedMaterial &material) {
      return MaterialConstantsUpload{
          .base_color_factor = {static_cast<float>(material.base_color_factor.x),
                                static_cast<float>(material.base_color_factor.y),
                                static_cast<float>(material.base_color_factor.z),
                                static_cast<float>(material.base_color_factor.w)},
          .scalar_factors = {static_cast<float>(material.normal_scale),
                             static_cast<float>(material.roughness_factor),
                             static_cast<float>(material.metallic_factor),
                             static_cast<float>(material.alpha_cutoff)},
          .emissive_factor = {static_cast<float>(material.emissive_factor.x),
                              static_cast<float>(material.emissive_factor.y),
                              static_cast<float>(material.emissive_factor.z),
                              0.0F},
          .reserved = {0.0F, 0.0F, 0.0F, 0.0F}};
   }

   /// @brief Finds an uploaded mesh summary in mutable runtime scene data.
   [[nodiscard]] std::optional<std::size_t> gpuMeshIndex(const SceneData &scene, MeshHandle mesh) {
      const auto index_it = scene.gpu_mesh_indices.find(mesh.value.value());
      if (index_it == scene.gpu_mesh_indices.end() || index_it->second >= scene.gpu_meshes.size()) {
         return std::nullopt;
      }
      return index_it->second;
   }

   /// @brief Finds an uploaded texture summary in mutable runtime scene data.
   [[nodiscard]] std::optional<std::size_t> gpuTextureIndex(const SceneData &scene, TextureHandle texture) {
      const auto index_it = scene.gpu_texture_indices.find(texture.value.value());
      if (index_it == scene.gpu_texture_indices.end() || index_it->second >= scene.gpu_textures.size()) {
         return std::nullopt;
      }
      return index_it->second;
   }

   /// @brief Finds an uploaded material summary in mutable runtime scene data.
   [[nodiscard]] std::optional<std::size_t> gpuMaterialIndex(const SceneData &scene, MaterialHandle material) {
      const auto index_it = scene.gpu_material_indices.find(material.value.value());
      if (index_it == scene.gpu_material_indices.end() || index_it->second >= scene.gpu_materials.size()) {
         return std::nullopt;
      }
      return index_it->second;
   }

   /// @brief Returns whether a material texture semantic is linear data rather than authored color.
   [[nodiscard]] bool isLinearTextureSemantic(TextureSemantic semantic) {
      switch (semantic) {
      case TextureSemantic::normal:
      case TextureSemantic::metallic_roughness:
      case TextureSemantic::roughness:
      case TextureSemantic::metallic:
      case TextureSemantic::ambient_occlusion:
         return true;
      case TextureSemantic::unknown:
      case TextureSemantic::base_color:
      case TextureSemantic::specular:
      case TextureSemantic::emissive:
      case TextureSemantic::opacity:
         return false;
      }

      return false;
   }

   /// @brief Picks the upload format for a texture from all material slots that reference it.
   [[nodiscard]] GpuImageFormat textureFormatForReferences(const SceneData &scene, TextureHandle texture) {
      for (const auto &material : scene.materials) {
         for (const auto &texture_ref : material.textures) {
            if (texture_ref.texture.value == texture.value && isLinearTextureSemantic(texture_ref.semantic)) {
               return GpuImageFormat::rgba8_unorm;
            }
         }
      }

      return GpuImageFormat::rgba8_srgb;
   }

   /// @brief Decodes a file-backed texture into RGBA8 pixels. Missing and embedded textures remain non-resident.
   [[nodiscard]] std::expected<std::optional<LoadedTexturePixels>, vve::Error>
   loadTexturePixels(const ImportedTexture &texture, GpuImageFormat format) {
      if (texture.embedded || texture.resolved_path.empty()) {
         return std::optional<LoadedTexturePixels>{};
      }

      const auto texture_path = std::filesystem::absolute(texture.resolved_path);
      if (!std::filesystem::exists(texture_path)) {
         return std::optional<LoadedTexturePixels>{};
      }

      int width = 0;
      int height = 0;
      int channel_count = 0;
      auto *pixels = stbi_load(texture_path.string().c_str(), &width, &height, &channel_count, STBI_rgb_alpha);
      (void)channel_count;
      if (pixels == nullptr || width <= 0 || height <= 0) {
         if (pixels != nullptr) {
            stbi_image_free(pixels);
         }
         return std::unexpected(vve::Error::io_error);
      }

      const auto width_value = static_cast<std::size_t>(width);
      const auto height_value = static_cast<std::size_t>(height);
      if (width_value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
          height_value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
          width_value > std::numeric_limits<std::size_t>::max() / height_value ||
          width_value * height_value > std::numeric_limits<std::size_t>::max() / 4U) {
         stbi_image_free(pixels);
         return std::unexpected(vve::Error::invalid_argument);
      }

      const auto byte_count = width_value * height_value * 4U;
      std::vector<std::byte> rgba_pixels(byte_count);
      std::memcpy(rgba_pixels.data(), pixels, byte_count);
      stbi_image_free(pixels);

      return LoadedTexturePixels{.rgba_pixels = std::move(rgba_pixels),
                                 .width = static_cast<std::uint32_t>(width_value),
                                 .height = static_cast<std::uint32_t>(height_value),
                                 .format = format};
   }

   /// @brief Looks up an uploaded texture summary by imported texture handle.
   [[nodiscard]] const GpuTextureResources *findGpuTexture(const SceneData &scene, TextureHandle texture) {
      const auto index = gpuTextureIndex(scene, texture);
      if (!index.has_value()) {
         return nullptr;
      }

      return std::addressof(scene.gpu_textures[*index]);
   }

   /// @brief Builds material texture bindings from currently resident texture uploads.
   [[nodiscard]] std::expected<MaterialTextureBindingUpload, vve::Error>
   materialTextureBindings(const SceneData &scene, const ImportedMaterial &material) {
      MaterialTextureBindingUpload upload{};
      for (const auto &texture_ref : material.textures) {
         const auto *texture = findGpuTexture(scene, texture_ref.texture);
         if (texture == nullptr || !texture->resident || !texture->image.value.isValid() ||
             !texture->sampler.value.isValid()) {
            upload.all_resident = false;
            continue;
         }

         const auto binding_index = checkedUint32(upload.bindings.size());
         if (!binding_index) {
            return std::unexpected(binding_index.error());
         }

         upload.bindings.push_back(GpuMaterialTextureBinding{.texture = texture_ref.texture,
                                                             .image = texture->image,
                                                             .sampler = texture->sampler,
                                                             .semantic = texture_ref.semantic,
                                                             .binding = *binding_index,
                                                             .uv_set = texture_ref.uv_set});
      }

      return upload;
   }

   /// @brief Returns whether an uploaded material already has the same resolved texture bindings.
   [[nodiscard]] bool textureBindingsEqual(const Vector<GpuMaterialTextureBinding> &left,
                                           const Vector<GpuMaterialTextureBinding> &right) {
      if (left.size() != right.size()) {
         return false;
      }

      for (std::size_t index = 0; index < left.size(); ++index) {
         const auto &left_binding = left[index];
         const auto &right_binding = right[index];
         if (left_binding.texture.value != right_binding.texture.value ||
             left_binding.image.value != right_binding.image.value ||
             left_binding.sampler.value != right_binding.sampler.value ||
             left_binding.semantic != right_binding.semantic ||
             left_binding.binding != right_binding.binding ||
             left_binding.uv_set != right_binding.uv_set) {
            return false;
         }
      }

      return true;
   }

   } // namespace

   /**
    * @brief Concrete resource-system implementation used by v3.
    */
   class DefaultResourceSystemImplementation {
   public:
      /// @brief Returns the subsystem name for diagnostics.
      [[nodiscard]] std::string_view name() const noexcept { return "ResourceSystem"; }

      /**
       * @brief Registers the resources referenced by an imported scene.
       * @param scene Imported scene to register.
       * @param source_path Source asset path associated with the scene.
       */
      [[nodiscard]] std::expected<void, vve::Error> registerImportedScene(const ImportedScene &scene,
                                                                          const std::filesystem::path &source_path) {
         scenes_.insert_or_assign(scene.handle.value.value(), scene);
         upsertRecord(ResourceRecord{.id = scene.handle.value,
                                     .kind = ResourceKind::unknown,
                                     .location = ResourceLocation::source_file,
                                     .generation = 1,
                                     .source_path = source_path});

         for (const auto &texture : scene.textures) {
            textures_.insert_or_assign(texture.handle.value.value(), texture);
            upsertRecord(ResourceRecord{.id = texture.handle.value,
                                        .kind = ResourceKind::texture,
                                        .location = ResourceLocation::cpu_memory,
                                        .generation = 1,
                                        .source_path = texture.resolved_path.empty() ? source_path
                                                                                    : texture.resolved_path});
         }

         for (const auto &mesh : scene.meshes) {
            meshes_.insert_or_assign(mesh.handle.value.value(), mesh);
            upsertRecord(ResourceRecord{.id = mesh.handle.value,
                                        .kind = ResourceKind::mesh,
                                        .location = ResourceLocation::cpu_memory,
                                        .generation = 1,
                                        .source_path = mesh.source_path.empty() ? source_path : mesh.source_path});
         }

         for (const auto &material : scene.materials) {
            materials_.insert_or_assign(material.handle.value.value(), material);
            upsertRecord(ResourceRecord{.id = material.handle.value,
                                        .kind = ResourceKind::material,
                                        .location = ResourceLocation::cpu_memory,
                                        .generation = 1,
                                        .source_path = source_path});
         }

         return {};
      }

      /**
       * @brief Loads and compiles a shader program as a first-class resource.
       * @param shader_path Shader source path to load.
       * @param shader_system Shader compiler/reflection subsystem.
       * @param renderer Intended renderer kind.
       * @param shadow Intended shadow mode.
       */
      [[nodiscard]] std::expected<ShaderMetadata, vve::Error>
      loadShaderProgram(const std::filesystem::path &shader_path, ShaderSystem &shader_system,
                        vve::RendererKind renderer, vve::ShadowKind shadow) {
         const auto shader_source = loadShaderSource(shader_path);
         if (!shader_source) {
            return std::unexpected(shader_source.error());
         }

         const auto compile_key = shader_source->source_path.generic_string();
         auto compiled_shader = compiled_shader_cache_.find(compile_key);
         if (compiled_shader == compiled_shader_cache_.end()) {
            const auto shader_metadata = shader_system.compileAndReflect(*shader_source, renderer, shadow);
            if (!shader_metadata) {
               return std::unexpected(shader_metadata.error());
            }

            compiled_shader = compiled_shader_cache_.emplace(compile_key, *shader_metadata).first;
         }

         auto shader_metadata = compiled_shader->second;
         shader_metadata.handle = shaderVariantHandle(*shader_source, renderer, shadow);
         shader_metadata.intended_renderer = std::string(vve::rendererKindName(renderer));
         shader_metadata.intended_shadow = std::string(vve::shadowKindName(shadow));

         const auto shader_id = shader_metadata.handle.value.value();
         shader_programs_.insert_or_assign(shader_id, shader_metadata);

         auto generation = std::uint32_t{1};
         if (const auto record_index = record_indices_.find(shader_id); record_index != record_indices_.end()) {
            generation = records_[record_index->second].generation + 1;
         }

         upsertRecord(ResourceRecord{.id = shader_metadata.handle.value,
                                     .kind = ResourceKind::shader_program,
                                     .location = ResourceLocation::cpu_memory,
                                     .generation = generation,
                                     .source_path = shader_source->source_path});

         return shader_metadata;
      }

      /// @brief Returns stored shader metadata for an already registered shader program.
      [[nodiscard]] std::expected<std::optional<ShaderMetadata>, vve::Error>
      shaderProgram(ShaderHandle shader) const {
         if (!shader.value.isValid()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         const auto shader_it = shader_programs_.find(shader.value.value());
         if (shader_it == shader_programs_.end()) {
            return std::optional<ShaderMetadata>{};
         }

         return shader_it->second;
      }

      /// @brief Returns a copy of the currently registered resource records.
      [[nodiscard]] std::expected<std::vector<ResourceRecord>, vve::Error> enumerate() const {
         std::vector<ResourceRecord> records{};
         records.reserve(records_.size());
         for (const auto &record : records_) {
            records.push_back(record);
         }

         return records;
      }

      /// @brief Streams imported scene resources into backend-owned GPU objects.
      [[nodiscard]] std::expected<void, vve::Error> uploadResources(const FrameContext &, SceneData &scene,
                                                                    GraphicsBackend &graphics_backend) {
         const auto upload_materials = [this, &scene, &graphics_backend](
                                           std::size_t max_uploads) -> std::expected<std::size_t, vve::Error> {
            std::size_t upload_count = 0;
            for (const auto &material : scene.materials) {
               auto *record = findRecord(material.handle.value);
               if (record == nullptr) {
                  return std::unexpected(vve::Error::invalid_argument);
               }

               auto texture_bindings = materialTextureBindings(scene, material);
               if (!texture_bindings) {
                  return std::unexpected(texture_bindings.error());
               }

               if (const auto existing_material = gpuMaterialIndex(scene, material.handle);
                   existing_material.has_value() && scene.gpu_materials[*existing_material].constants_uploaded &&
                   scene.gpu_materials[*existing_material].generation == record->generation &&
                   scene.gpu_materials[*existing_material].textures_uploaded == texture_bindings->all_resident &&
                   textureBindingsEqual(scene.gpu_materials[*existing_material].textures, texture_bindings->bindings)) {
                  continue;
               }

               if (upload_count >= max_uploads) {
                  break;
               }

               const auto uploaded_generation = record->generation + 1U;
               const auto payload = materialConstantsUpload(material);
               const auto payload_bytes = std::as_bytes(std::span{std::addressof(payload), 1U});
               const auto constants_buffer =
                   graphics_backend.createBuffer(material.handle.value, ResourceKind::material, GpuBufferUsage::uniform,
                                                 payload_bytes, uploaded_generation);
               if (!constants_buffer) {
                  return std::unexpected(constants_buffer.error());
               }

               upsertGpuMaterial(scene, GpuMaterialResources{.material = material.handle,
                                                             .constants_buffer = constants_buffer->handle,
                                                             .textures = std::move(texture_bindings->bindings),
                                                             .generation = uploaded_generation,
                                                             .constants_uploaded = true,
                                                             .textures_uploaded = texture_bindings->all_resident});

               record->location = ResourceLocation::gpu_memory;
               record->generation = uploaded_generation;
               upsertRecord(ResourceRecord{.id = constants_buffer->handle.value,
                                           .kind = ResourceKind::buffer,
                                           .location = ResourceLocation::gpu_memory,
                                           .generation = uploaded_generation,
                                           .source_path = scene.source_path});
               ++upload_count;
            }

            return upload_count;
         };

         if (const auto material_uploads = upload_materials(maxMaterialUploadsPerFrame); !material_uploads) {
            return std::unexpected(material_uploads.error());
         }

         std::size_t mesh_uploads = 0;
         bool mesh_backlog_remaining = false;
         for (const auto &mesh : scene.meshes) {
            if (mesh.vertices.empty() || mesh.indices.empty()) {
               continue;
            }

            auto *record = findRecord(mesh.handle.value);
            if (record == nullptr) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            if (const auto existing_mesh = gpuMeshIndex(scene, mesh.handle);
                existing_mesh.has_value() && scene.gpu_meshes[*existing_mesh].resident &&
                scene.gpu_meshes[*existing_mesh].generation == record->generation) {
               continue;
            }

            if (mesh_uploads >= maxMeshUploadsPerFrame) {
               mesh_backlog_remaining = true;
               continue;
            }

            const auto vertex_count = checkedUint32(mesh.vertices.size());
            const auto index_count = checkedUint32(mesh.indices.size());
            const auto submesh_count = checkedUint32(mesh.submeshes.size());
            if (!vertex_count || !index_count || !submesh_count) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            const auto uploaded_generation = record->generation + 1U;
            const auto vertex_bytes = copyObjectBytes(mesh.vertices);
            const auto index_bytes = copyObjectBytes(mesh.indices);

            const auto vertex_buffer = graphics_backend.createBuffer(
                mesh.handle.value, ResourceKind::mesh, GpuBufferUsage::vertex,
                std::span<const std::byte>{vertex_bytes.data(), vertex_bytes.size()}, uploaded_generation);
            if (!vertex_buffer) {
               return std::unexpected(vertex_buffer.error());
            }

            const auto index_buffer = graphics_backend.createBuffer(
                mesh.handle.value, ResourceKind::mesh, GpuBufferUsage::index,
                std::span<const std::byte>{index_bytes.data(), index_bytes.size()}, uploaded_generation);
            if (!index_buffer) {
               [[maybe_unused]] const auto destroyed = graphics_backend.destroyBuffer(vertex_buffer->handle);
               return std::unexpected(index_buffer.error());
            }

            upsertGpuMesh(scene, GpuMeshResources{.mesh = mesh.handle,
                                                  .vertex_buffer = vertex_buffer->handle,
                                                  .index_buffer = index_buffer->handle,
                                                  .vertex_count = *vertex_count,
                                                  .index_count = *index_count,
                                                  .submesh_count = *submesh_count,
                                                  .vertex_stride = sizeof(ImportedVertex),
                                                  .vertex_byte_size = vertex_bytes.size(),
                                                  .index_byte_size = index_bytes.size(),
                                                  .generation = uploaded_generation,
                                                  .resident = true});

            record->location = ResourceLocation::gpu_memory;
            record->generation = uploaded_generation;
            upsertRecord(ResourceRecord{.id = vertex_buffer->handle.value,
                                        .kind = ResourceKind::buffer,
                                        .location = ResourceLocation::gpu_memory,
                                        .generation = uploaded_generation,
                                        .source_path = mesh.source_path.empty() ? scene.source_path
                                                                                : mesh.source_path});
            upsertRecord(ResourceRecord{.id = index_buffer->handle.value,
                                        .kind = ResourceKind::buffer,
                                        .location = ResourceLocation::gpu_memory,
                                        .generation = uploaded_generation,
                                        .source_path = mesh.source_path.empty() ? scene.source_path
                                                                                : mesh.source_path});
            ++mesh_uploads;
         }

         if (mesh_backlog_remaining) {
            return {};
         }

         std::size_t texture_uploads = 0;
         for (const auto &texture : scene.textures) {
            auto *record = findRecord(texture.handle.value);
            if (record == nullptr) {
               return std::unexpected(vve::Error::invalid_argument);
            }

            if (const auto existing_texture = gpuTextureIndex(scene, texture.handle);
                existing_texture.has_value() && scene.gpu_textures[*existing_texture].resident &&
                scene.gpu_textures[*existing_texture].generation == record->generation) {
               continue;
            }

            if (texture_uploads >= maxTextureUploadsPerFrame) {
               break;
            }

            const auto format = textureFormatForReferences(scene, texture.handle);
            const auto decoded_texture = loadTexturePixels(texture, format);
            if (!decoded_texture) {
               return std::unexpected(decoded_texture.error());
            }
            if (!decoded_texture->has_value()) {
               continue;
            }

            const auto &pixels = decoded_texture->value();
            const auto uploaded_generation = record->generation + 1U;
            const auto uploaded_texture = graphics_backend.createSampledImage(
                texture.handle.value, ResourceKind::texture, pixels.format, pixels.width, pixels.height,
                std::span<const std::byte>{pixels.rgba_pixels.data(), pixels.rgba_pixels.size()},
                uploaded_generation);
            if (!uploaded_texture) {
               return std::unexpected(uploaded_texture.error());
            }

            upsertGpuTexture(scene, *uploaded_texture);
            record->location = ResourceLocation::gpu_memory;
            record->generation = uploaded_generation;
            upsertRecord(ResourceRecord{.id = uploaded_texture->image.value,
                                        .kind = ResourceKind::image,
                                        .location = ResourceLocation::gpu_memory,
                                        .generation = uploaded_generation,
                                        .source_path = texture.resolved_path});
            ++texture_uploads;
         }

         if (texture_uploads > 0) {
            if (const auto material_uploads = upload_materials(maxMaterialUploadsPerFrame); !material_uploads) {
               return std::unexpected(material_uploads.error());
            }
         }

         return {};
      }

      /// @brief Registers the built-in resource upload task.
      void registerTasks(TaskGraphBuilder &builder, const SceneData &, GraphicsBackend &graphics_backend) {
         [[maybe_unused]] const auto upload_resources_task = builder.addTask(
             "task.upload_resources", TaskKernelId::upload_resources,
             detail::requireFrameScene([this, &graphics_backend](const FrameContext &frame_context, SceneData &scene) {
                return uploadResources(frame_context, scene, graphics_backend);
             }),
             {TaskGraphBuilder::taskHandleFor("task.cull_visibility_cpu")}, {}, "Upload Resources",
             TaskPhase::resources);
      }

   private:
      [[nodiscard]] ResourceRecord *findRecord(vve::Handle id) {
         const auto record_index = record_indices_.find(id.value());
         if (record_index == record_indices_.end() || record_index->second >= records_.size()) {
            return nullptr;
         }
         return std::addressof(records_[record_index->second]);
      }

      void upsertGpuMesh(SceneData &scene, GpuMeshResources resources) {
         const auto id = resources.mesh.value.value();
         if (const auto gpu_mesh_index = scene.gpu_mesh_indices.find(id);
             gpu_mesh_index != scene.gpu_mesh_indices.end() && gpu_mesh_index->second < scene.gpu_meshes.size()) {
            scene.gpu_meshes[gpu_mesh_index->second] = std::move(resources);
            return;
         }

         scene.gpu_mesh_indices.insert_or_assign(id, scene.gpu_meshes.size());
         scene.gpu_meshes.push_back(std::move(resources));
      }

      void upsertGpuTexture(SceneData &scene, GpuTextureResources resources) {
         const auto id = resources.texture.value.value();
         if (const auto gpu_texture_index = scene.gpu_texture_indices.find(id);
             gpu_texture_index != scene.gpu_texture_indices.end() &&
             gpu_texture_index->second < scene.gpu_textures.size()) {
            scene.gpu_textures[gpu_texture_index->second] = std::move(resources);
            return;
         }

         scene.gpu_texture_indices.insert_or_assign(id, scene.gpu_textures.size());
         scene.gpu_textures.push_back(std::move(resources));
      }

      void upsertGpuMaterial(SceneData &scene, GpuMaterialResources resources) {
         const auto id = resources.material.value.value();
         if (const auto gpu_material_index = scene.gpu_material_indices.find(id);
             gpu_material_index != scene.gpu_material_indices.end() &&
             gpu_material_index->second < scene.gpu_materials.size()) {
            scene.gpu_materials[gpu_material_index->second] = std::move(resources);
            return;
         }

         scene.gpu_material_indices.insert_or_assign(id, scene.gpu_materials.size());
         scene.gpu_materials.push_back(std::move(resources));
      }

      void upsertRecord(ResourceRecord record) {
         const auto id = record.id.value();
         if (const auto record_index = record_indices_.find(id); record_index != record_indices_.end()) {
            records_[record_index->second] = std::move(record);
            return;
         }

         record_indices_.emplace(id, records_.size());
         records_.push_back(std::move(record));
      }

      Vector<ResourceRecord> records_{}; ///< Registered resource records owned by the subsystem.
      std::unordered_map<vve::Handle::value_type, std::size_t> record_indices_{}; ///< Record lookup cache keyed by stable resource id.
      std::unordered_map<vve::Handle::value_type, ImportedScene> scenes_{}; ///< Imported scenes retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedTexture> textures_{}; ///< Imported texture references retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedMesh> meshes_{}; ///< Imported mesh payloads retained in CPU memory.
      std::unordered_map<vve::Handle::value_type, ImportedMaterial> materials_{}; ///< Imported material payloads retained in CPU memory.
      std::map<std::string, ShaderMetadata> compiled_shader_cache_{}; ///< Source-level compiler output reused by variants.
      std::unordered_map<vve::Handle::value_type, ShaderMetadata> shader_programs_{}; ///< Compiled shader metadata retained in CPU memory.
   };

   /// @brief Constructs the public resource-system facade around the concrete implementation.
   VVE_V3_DEFINE_FACADE_CTOR(ResourceSystemFacade, DefaultResourceSystemImplementation, (), ())

   /// @brief Returns the resource-system name for the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, name, (), (), const noexcept,
                               std::string_view)

   /// @brief Registers imported scene resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, registerImportedScene,
                               (const ImportedScene &scene, const std::filesystem::path &source_path),
                               (scene, source_path), , std::expected<void, vve::Error>)

   /// @brief Loads, compiles, and registers a shader program through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, loadShaderProgram,
                               (const std::filesystem::path &shader_path,
                                ShaderSystemFacade<SlangShaderSystemImplementation> &shader_system,
                                vve::RendererKind renderer, vve::ShadowKind shadow),
                               (shader_path, shader_system, renderer, shadow), ,
                               std::expected<ShaderMetadata, vve::Error>)

   /// @brief Returns registered shader metadata through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, shaderProgram,
                               (ShaderHandle shader), (shader), const,
                               std::expected<std::optional<ShaderMetadata>, vve::Error>)

   /// @brief Enumerates registered resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, enumerate, (), (), const,
                               std::expected<std::vector<ResourceRecord>, vve::Error>)

   /// @brief Uploads resources through the public facade.
   VVE_V3_DEFINE_FACADE_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, uploadResources,
                               (const FrameContext &frame_context, SceneData &scene,
                                GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend),
                               (frame_context, scene, graphics_backend), ,
                               std::expected<void, vve::Error>)

   /// @brief Registers resource tasks through the public facade.
   VVE_V3_DEFINE_FACADE_VOID_METHOD(ResourceSystemFacade, DefaultResourceSystemImplementation, registerTasks,
                                    (TaskGraphBuilder &builder, const SceneData &scene,
                                     GraphicsBackendFacade<VulkanGraphicsBackendImplementation> &graphics_backend),
                                    (builder, scene, graphics_backend), )

   /// @brief Emits the explicit resource-system facade instantiation for v3.
   template class ResourceSystemFacade<DefaultResourceSystemImplementation>;

} // namespace vve::v3
